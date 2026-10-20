import { requiredSecret, base64UrlDecode, base64UrlEncode, constantTimeEqual } from '../core/crypto.mjs';
import { licenseExpiryMs, validSessionTimes } from '../session-policy.mjs';
import {requireRuntimeSession} from './runtime-v2.mjs';
// Deployed signed-session audience: this is a protocol identifier, not branding.
export const AUTH_AUDIENCE = "wolf-client-v1";
const HEX_64 = /^[a-f0-9]{64}$/i;
function signingKeyId(env) {
    const id=env.SIGNING_KEY_ID ?? 'primary';
    if (typeof id!=='string' || !/^[a-zA-Z0-9_-]{1,64}$/.test(id)) throw new Error('Invalid signing key id');
    return id;
}

export async function importSigningPrivateKey(env) {
    const der = base64UrlDecode(requiredSecret(env, "SIGNING_PRIVATE_KEY_PKCS8"));
    return crypto.subtle.importKey("pkcs8", der, { name: "ECDSA", namedCurve: "P-256" }, false, ["sign"]);
}

export async function importSigningPublicKey(env) {
    const der = base64UrlDecode(requiredSecret(env, "SIGNING_PUBLIC_KEY_SPKI"));
    return crypto.subtle.importKey("spki", der, { name: "ECDSA", namedCurve: "P-256" }, false, ["verify"]);
}

export async function signPayload(payload, env) {
    const payloadText = JSON.stringify({...payload,kid:signingKeyId(env)});
    const payloadBytes = new TextEncoder().encode(payloadText);
    const signature = await crypto.subtle.sign(
        { name: "ECDSA", hash: "SHA-256" },
        await importSigningPrivateKey(env),
        payloadBytes
    );
    const encodedPayload = base64UrlEncode(payloadBytes);
    return {
        payload: encodedPayload,
        signature: base64UrlEncode(signature),
        session_token: `${encodedPayload}.${base64UrlEncode(signature)}`,
    };
}

export async function verifySessionToken(token, env, audience = AUTH_AUDIENCE) {
    if (typeof token !== "string" || token.length > 16384) throw new Error("Invalid session token");
    const parts = token.split(".");
    if (parts.length !== 2) throw new Error("Invalid session token");
    const payloadBytes = base64UrlDecode(parts[0]);
    const signature = base64UrlDecode(parts[1]);
    const valid = await crypto.subtle.verify(
        { name: "ECDSA", hash: "SHA-256" },
        await importSigningPublicKey(env),
        signature,
        payloadBytes
    );
    if (!valid) throw new Error("Invalid session signature");
    const payload = JSON.parse(new TextDecoder().decode(payloadBytes));
    // Legacy no-kid tokens are accepted only before an operator activates an
    // explicit key ID. A cutover never silently trusts retired signing keys.
    if (!payload || (payload.kid !== signingKeyId(env) && !(payload.kid===undefined && env.SIGNING_KEY_ID===undefined)) ||
        payload.aud !== audience || !HEX_64.test(String(payload.key_hash || "")) ||
        !HEX_64.test(String(payload.hwid_hash || "")) || !Number.isInteger(payload.license_epoch) ||
        payload.license_epoch <= 0 || !validSessionTimes(payload, Date.now())) {
        throw new Error("Expired or malformed session token");
    }
    return payload;
}

export async function requireActiveConfigSession(body, env) {
    // Device identifiers are not credentials. No deployment flag may turn off auth.
    const suppliedToken = String(body && body.session_token || "");
    if (!suppliedToken) throw new Error("Missing license session");

    let claims;
    if (env.AUTH_V2_ONLY === 'true') claims = await requireRuntimeSession(suppliedToken, env);
    else {
        try { claims = await requireRuntimeSession(suppliedToken, env); }
        catch (_) { claims = await verifySessionToken(suppliedToken, env); }
    }
    const license = await env.DB.prepare(
        "SELECT key_hash, hwid_hash, device_key_hash, license_epoch, status, expires_at FROM license_keys WHERE key_hash = ? LIMIT 1"
    ).bind(claims.key_hash).first();
    if (!license || String(license.status || "").toLowerCase() !== "active") {
        throw new Error("License session is not active");
    }
    if (!HEX_64.test(String(license.device_key_hash || "")) ||
        Number(license.license_epoch || 1) !== claims.license_epoch ||
        !constantTimeEqual(String(license.hwid_hash || ""), claims.hwid_hash) ||
        (license.device_key_hash && !constantTimeEqual(license.device_key_hash, claims.device_key_hash || ""))) {
        throw new Error("License session was revoked");
    }
    const licenseExpiry = licenseExpiryMs(license.expires_at);
    if (licenseExpiry !== null && (!Number.isFinite(licenseExpiry) || Date.now() >= licenseExpiry)) {
        throw new Error("License session expired");
    }
    return claims;
}
