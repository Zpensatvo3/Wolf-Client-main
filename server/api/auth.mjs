import { jsonResponse, readJson } from '../core/http.mjs';
import { requiredSecret, sha256, constantTimeEqual } from '../core/crypto.mjs';
import { AUTH_AUDIENCE, verifySessionToken, signPayload } from '../auth/sessions.mjs';
import { verifyDeviceProof } from '../device-proof.mjs';
import { licenseExpiryMs, sessionExpiryMs } from '../session-policy.mjs';
import { ensureDatabaseSchema } from '../database/schema.mjs';
import { enforceRateLimit } from '../core/rate-limit.mjs';
const HEX_64 = /^[a-f0-9]{64}$/i;
const NONCE = /^[a-f0-9]{32}$/i;
const KEY_CODE = /^[A-Za-z0-9-]{8,128}$/;

export async function handleAuth(request, env, ctx) {
    const url = new URL(request.url);
    if (url.pathname === "/api/v1/auth" && request.method === "POST") {
        try {
            await ensureDatabaseSchema(env);
            const retryAfter = await enforceRateLimit(request, env, "auth", 12, 60 * 1000);
            if (retryAfter !== null) {
                return jsonResponse({ success: false, message: "Too many authentication attempts. Please wait." }, 429);
            }

            const body = await readJson(request);
            const {
                hwid,
                nonce,
                timestamp,
                request_type: requestType,
            } = body;

            if (!hwid || !nonce || !timestamp || !requestType ||
                !HEX_64.test(String(hwid)) || !NONCE.test(String(nonce))) {
                return jsonResponse({
                    success: false,
                    message: "Missing Parameters",
                }, 400);
            }

            if (requestType !== "activate" && requestType !== "heartbeat") {
                return jsonResponse({
                    success: false,
                    message: "Invalid Request Type",
                }, 400);
            }

            const requestTime = Number(timestamp);
            const now = Date.now();
            const maxClockSkewMs = 2 * 60 * 1000;

            if (!Number.isFinite(requestTime) ||
                Math.abs(now - requestTime) > maxClockSkewMs) {
                return jsonResponse({
                    success: false,
                    message: "Authentication Request Expired",
                }, 408);
            }

            const hwidHash = await sha256(hwid + requiredSecret(env, "HWID_PEPPER"));
            // Display only: this matches the fingerprint shown by the
            // client without exposing the full value in the dashboard.
            const hwidHint = hwid.substring(0, 10) + "..." + hwid.substring(hwid.length - 6);
            const nonceHash = await sha256(nonce);
            let keyHash;
            let heartbeatClaims = null;
            let license;

            if (requestType === "activate") {
                const key = String(body.key || "").trim();
                if (!KEY_CODE.test(key)) {
                    return jsonResponse({ success: false, message: "Invalid License Key" }, 401);
                }
                keyHash = await sha256(key + requiredSecret(env, "KEY_PEPPER"));
                license = await env.DB.prepare(
                    "SELECT * FROM license_keys WHERE key_hash = ? LIMIT 1"
                ).bind(keyHash).first();
            } else {
                heartbeatClaims = await verifySessionToken(body.session_token, env);
                if (!constantTimeEqual(heartbeatClaims.hwid_hash, hwidHash)) {
                    return jsonResponse({ success: false, message: "HWID Mismatch!" }, 403);
                }
                keyHash = heartbeatClaims.key_hash;
                license = await env.DB.prepare(
                    "SELECT * FROM license_keys WHERE key_hash = ? LIMIT 1"
                ).bind(keyHash).first();
            }

            if (!license) {
                return jsonResponse({ success: false, message: "Invalid License Key" }, 401);
            }

            let deviceKeyHash = "";
            if (body.device_key || body.device_signature) {
                try { deviceKeyHash = await verifyDeviceProof(body); }
                catch (_) { return jsonResponse({ success: false, message: "Device verification failed." }, 403); }
            }
            if (license.device_key_hash && !constantTimeEqual(license.device_key_hash, deviceKeyHash)) {
                return jsonResponse({ success: false, message: "License belongs to another device key. Reset HWID to change devices." }, 403);
            }
            if (requestType === "heartbeat" && deviceKeyHash && !license.device_key_hash) {
                return jsonResponse({ success: false, message: "Sign in again to register this device." }, 403);
            }
            if (heartbeatClaims && license.device_key_hash &&
                !constantTimeEqual(license.device_key_hash, heartbeatClaims.device_key_hash || "")) {
                return jsonResponse({ success: false, message: "Device session was revoked." }, 403);
            }

            const status = String(license.status || "unused").toLowerCase();

            if (status === "banned") {
                return jsonResponse({
                    success: false,
                    message: "License Key is Banned",
                }, 403);
            }

            if (status === "expired") {
                return jsonResponse({
                    success: false,
                    message: "License Expired",
                }, 403);
            }

            const licenseExpiry = licenseExpiryMs(license.expires_at);
            if (licenseExpiry !== null) {
                if (!Number.isFinite(licenseExpiry) || now >= licenseExpiry) {
                    try {
                        await env.DB.prepare(
                            "UPDATE license_keys SET status = 'expired', license_epoch = license_epoch + 1 WHERE id = ? AND status != 'expired'"
                        ).bind(license.id).run();
                    } catch (error) {
                        try {
                            await env.DB.prepare(
                                "UPDATE license_keys SET status = 'expired' WHERE id = ?"
                            ).bind(license.id).run();
                        } catch (_) { }
                    }

                    return jsonResponse({
                        success: false,
                        message: "License Expired",
                    }, 403);
                }
            }

            const currentBoundHwid =
                String(license.hwid_hash || license.hwid || "");
            const hwidMatches =
                currentBoundHwid === hwidHash || currentBoundHwid === hwid;

            if (requestType === "heartbeat") {
                // Heartbeats are never allowed to activate or re-bind a license.
                // This prevents an old device from reclaiming a key after an admin reset.
                if (status !== "active" || !currentBoundHwid) {
                    return jsonResponse({
                        success: false,
                        message: "Session Revoked",
                    }, 403);
                }

                if (!hwidMatches) {
                    try {
                        ctx.waitUntil(
                            env.DB.prepare(
                                "INSERT INTO auth_logs (key_hash, hwid_hash, ip_address, status) VALUES (?, ?, ?, ?)"
                            ).bind(
                                keyHash,
                                hwidHash,
                                request.headers.get("CF-Connecting-IP") || "0.0.0.0",
                                "HEARTBEAT_HWID_MISMATCH"
                            ).run()
                        );
                    } catch (_) { }

                    return jsonResponse({
                        success: false,
                        message: "HWID Mismatch! Key bound to another device.",
                    }, 403);
                }
            } else {
                // Initial activation or normal re-login.
                if (status === "active") {
                    if (!currentBoundHwid || !hwidMatches) {
                        return jsonResponse({
                            success: false,
                            message: "HWID Mismatch! Key bound to another device.",
                        }, 403);
                    }
                } else if (status === "unused") {
                    if (currentBoundHwid) {
                        if (!hwidMatches) {
                            return jsonResponse({
                                success: false,
                                message: "HWID Mismatch! Key bound to another device.",
                            }, 403);
                        }

                        // Reactivate only if the same HWID is still bound at write time.
                        let reactivateResult;
                        try {
                            reactivateResult = await env.DB.prepare(
                                "UPDATE license_keys SET status = 'active' WHERE id = ? AND status = 'unused' AND (hwid_hash = ? OR hwid = ?)"
                            ).bind(license.id, hwidHash, hwid).run();
                        } catch (error) {
                            try {
                                reactivateResult = await env.DB.prepare(
                                    "UPDATE license_keys SET status = 'active' WHERE id = ? AND status = 'unused' AND hwid_hash = ?"
                                ).bind(license.id, hwidHash).run();
                            } catch (_) {
                                reactivateResult = await env.DB.prepare(
                                    "UPDATE license_keys SET status = 'active' WHERE id = ? AND status = 'unused' AND hwid = ?"
                                ).bind(license.id, hwid).run();
                            }
                        }

                        if (!reactivateResult ||
                            !reactivateResult.meta ||
                            reactivateResult.meta.changes !== 1) {
                            return jsonResponse({
                                success: false,
                                message: "License State Changed. Please retry.",
                            }, 409);
                        }
                    } else {
                        // First activation is one conditional write. Two machines cannot
                        // both win the initial HWID bind.
                        let bindResult;
                        try {
                            bindResult = await env.DB.prepare(
                                "UPDATE license_keys SET hwid_hash = ?, hwid_hint = ?, status = 'active', activated_at = COALESCE(activated_at, CURRENT_TIMESTAMP) WHERE id = ? AND status = 'unused' AND (hwid_hash IS NULL OR hwid_hash = '') AND (hwid IS NULL OR hwid = '')"
                            ).bind(hwidHash, hwidHint, license.id).run();
                        } catch (error) {
                            try {
                                bindResult = await env.DB.prepare(
                                    "UPDATE license_keys SET hwid_hash = ?, hwid_hint = ?, status = 'active', activated_at = COALESCE(activated_at, CURRENT_TIMESTAMP) WHERE id = ? AND status = 'unused' AND (hwid_hash IS NULL OR hwid_hash = '')"
                                ).bind(hwidHash, hwidHint, license.id).run();
                            } catch (_) {
                                bindResult = await env.DB.prepare(
                                    "UPDATE license_keys SET hwid = ?, status = 'active' WHERE id = ? AND status = 'unused' AND (hwid IS NULL OR hwid = '')"
                                ).bind(hwid, license.id).run();
                            }
                        }

                        if (!bindResult ||
                            !bindResult.meta ||
                            bindResult.meta.changes !== 1) {
                            return jsonResponse({
                                success: false,
                                message: "License Activation Conflict. Please retry.",
                            }, 409);
                        }
                    }
                } else {
                    return jsonResponse({
                        success: false,
                        message: "License is not active.",
                    }, 403);
                }
            }

            // Upgrade a legacy raw binding only after it has matched the
            // current device. The stored value remains a peppered hash;
            // the UI receives only the compact display hint.
            if (hwidMatches &&
                (String(license.hwid_hash || "") !== hwidHash ||
                    String(license.hwid_hint || "") !== hwidHint)) {
                try {
                    await env.DB.prepare(
                        "UPDATE license_keys SET hwid_hash = ?, hwid_hint = ? WHERE id = ?"
                    ).bind(hwidHash, hwidHint, license.id).run();
                    license.hwid_hash = hwidHash;
                    license.hwid_hint = hwidHint;
                } catch (error) {
                    console.error("hwid_binding_migration_failed");
                }
            }

            // Re-read the epoch after any activation write so the token reflects
            // the current server state instead of the earlier SELECT snapshot.
            if (deviceKeyHash && !license.device_key_hash && requestType === "activate") {
                await env.DB.prepare(
                    "UPDATE license_keys SET device_key_hash = ? WHERE id = ? AND status = 'active' AND hwid_hash = ? AND license_epoch = ? AND (device_key_hash IS NULL OR device_key_hash = '')"
                ).bind(deviceKeyHash, license.id, hwidHash, Number(license.license_epoch || 1)).run();
            }
            let currentEpoch;
            {
                const epochRow = await env.DB.prepare(
                    "SELECT license_epoch, status, hwid_hash, device_key_hash FROM license_keys WHERE id = ? LIMIT 1"
                ).bind(license.id).first();

                if (!epochRow || String(epochRow.status).toLowerCase() !== "active" ||
                    !constantTimeEqual(epochRow.hwid_hash || "", hwidHash) ||
                    !constantTimeEqual(epochRow.device_key_hash || "", deviceKeyHash)) {
                    return jsonResponse({
                        success: false,
                        message: "Session Revoked",
                    }, 403);
                }

                currentEpoch = Number(epochRow.license_epoch || 1);
            }

            if (!Number.isInteger(currentEpoch) || currentEpoch <= 0) {
                return jsonResponse({
                    success: false,
                    message: "Invalid License Epoch",
                }, 500);
            }

            if (heartbeatClaims && heartbeatClaims.license_epoch !== currentEpoch) {
                return jsonResponse({
                    success: false,
                    message: "Session Revoked",
                }, 403);
            }

            const issuedAt = Date.now();
            const sessionExpiresAt = sessionExpiryMs(issuedAt, licenseExpiry);
            if (sessionExpiresAt === null) {
                return jsonResponse({ success: false, message: "License Expired" }, 403);
            }

            const payload = {
                success: true,
                session_id: await sha256(
                    keyHash + nonceHash + issuedAt + requestType
                ),
                key_hash: keyHash,
                hwid_hash: hwidHash,
                device_key_hash: deviceKeyHash || undefined,
                nonce_hash: nonceHash,
                aud: AUTH_AUDIENCE,
                license_epoch: currentEpoch,
                iat: issuedAt,
                exp: sessionExpiresAt,
                expires_at: license.expires_at || "lifetime",
            };

            try {
                ctx.waitUntil(
                    env.DB.prepare(
                        "INSERT INTO auth_logs (key_hash, hwid_hash, ip_address, status) VALUES (?, ?, ?, ?)"
                    ).bind(
                        keyHash,
                        hwidHash,
                        request.headers.get("CF-Connecting-IP") || "0.0.0.0",
                        requestType === "heartbeat" ? "HEARTBEAT_SUCCESS" : "SUCCESS"
                    ).run()
                );
            } catch (_) { }

            return jsonResponse(await signPayload(payload, env));
        } catch (error) {
            // The native client receives only a generic failure. Keep the
            // operator-visible cause in Worker logs without leaking it.
            console.error("client_auth_failed");
            return jsonResponse({
                success: false,
                message: "Authentication service unavailable.",
            }, 500);
        }
    }

    return null;
}
