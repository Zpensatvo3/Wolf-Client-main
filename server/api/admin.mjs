import { requiredSecret, sha256 } from '../core/crypto.mjs';
import { verifyAdminAccess } from '../auth/admin-access.mjs';
import { jsonResponse, readJson } from '../core/http.mjs';
import { ensureDatabaseSchema } from '../database/schema.mjs';
import { enforceRateLimit } from '../core/rate-limit.mjs';
import { handleAdminRead } from './admin-read.mjs';

function generateRandomKey() {
    // 128 unpredictable bits. No administrator-selected dictionary keys.
    const hex = Array.from(crypto.getRandomValues(new Uint8Array(16)),byte=>byte.toString(16).padStart(2,'0')).join('').toUpperCase();
    return 'ONESTEP-'+hex.match(/.{8}/g).join('-');
}

export async function verifyAdminAuth(request, env) {
    // Access is the sole identity boundary. A legacy bearer never grants access.
    if (!await verifyAdminAccess(request, env)) return false;
    // Cookie-authenticated mutations must originate from this application.
    // Apply to release uploads too, not only JSON/form routes.
    if (!['GET', 'HEAD', 'OPTIONS'].includes(request.method)) {
        if (request.headers.get('Origin') !== new URL(request.url).origin) return false;
        if (request.headers.get('Sec-Fetch-Site') === 'cross-site') return false;
    }
    return true;
}

async function auditedUpdate(env, request, action, keyId, reason, update) {
    const results = await env.DB.batch([
        env.DB.prepare(
            "INSERT INTO admin_audit_logs (admin_id, action, key_hash, previous_epoch, new_epoch, reason, ip_address) " +
            "SELECT ?, ?, key_hash, license_epoch, license_epoch + 1, ?, ? FROM license_keys WHERE id = ?"
        ).bind("dashboard", action, reason, request.headers.get("CF-Connecting-IP") || "0.0.0.0", keyId),
        update
    ]);
    return results[1];
}

export async function handleAdmin(request, env, ctx) {
    const url = new URL(request.url);
    if (url.pathname.startsWith("/api/v1/admin/")) {
        let retryAfter;
        try {
            retryAfter = await enforceRateLimit(request, env, "admin", 10, 60 * 1000);
        } catch (_) {
            return jsonResponse({ success: false, message: "Admin service is not configured." }, 503);
        }
        if (retryAfter !== null) {
            return jsonResponse({ success: false, message: "Too many admin requests. Please wait." }, 429);
        }

        let isAuth = false;
        try {
            isAuth = await verifyAdminAuth(request, env);
        } catch (_) {
            return jsonResponse({ success: false, message: "Admin service is not configured." }, 503);
        }
        if (!isAuth) {
            return jsonResponse({ success: false, message: "Unauthorized Admin Access" }, 401);
        }

        try {
            await ensureDatabaseSchema(env);
        } catch (error) {
            console.error("license_schema_validation_failed");
            return jsonResponse({ success: false, message: "License database schema requires operator review." }, 500);
        }

        const readRoutes = new Set(['list-keys', 'overview', 'activity', 'status']);
        const writeRoutes = new Set(['create-key', 'reveal-key', 'reset-hwid', 'delete-key', 'ban-key', 'delete-config', 'revoke-release']);
        const route = url.pathname.slice('/api/v1/admin/'.length);
        if (!readRoutes.has(route) && !writeRoutes.has(route))
            return jsonResponse({ success: false, message: 'Unknown admin endpoint.' }, 404);
        if (request.method !== (readRoutes.has(route) ? 'GET' : 'POST'))
            return jsonResponse({ success: false, message: 'Method not allowed.' }, 405);
        if (readRoutes.has(route)) {
            try { return await handleAdminRead(request, env); }
            catch (_) { return jsonResponse({ success: false, message: 'Unable to load admin data.' }, 500); }
        }
        let body;
        try { body = await readJson(request); }
        catch (_) { return jsonResponse({ success: false, message: 'Invalid JSON request.' }, 400); }

        if (route === 'revoke-release') {
            if (!/^[a-f0-9]{64}$/.test(body.sha256 || '')) return jsonResponse({message:'Invalid release hash.'},400);
            await env.DB.batch([
                env.DB.prepare('UPDATE approved_releases SET revoked=1 WHERE sha256=?').bind(body.sha256),
                env.DB.prepare('UPDATE runtime_sessions SET revoked=1 WHERE release_sha256=?').bind(body.sha256),
                env.DB.prepare('DELETE FROM release_channels WHERE sha256=?').bind(body.sha256),
                env.DB.prepare("INSERT INTO admin_audit_logs(admin_id,action,reason,ip_address) VALUES('dashboard','REVOKE_RELEASE',?,?)")
                    .bind(body.sha256,request.headers.get('CF-Connecting-IP') || '0.0.0.0')
            ]);
            return jsonResponse({success:true});
        }

        // CREATE KEY
        if (url.pathname === "/api/v1/admin/create-key" && request.method === "POST") {
            try {

                if (body.key !== undefined) return jsonResponse({success:false,message:'Custom license keys are not supported.'},400);
                const keyToCreate = generateRandomKey();
                const days = String(body.days || "lifetime");
                if (!["lifetime", "1", "7", "30"].includes(days)) {
                    return jsonResponse({ success: false, message: "Invalid license request." }, 400);
                }

                let expiresAt = null;
                if (days && days !== "lifetime") {
                    const expDate = new Date();
                    expDate.setDate(expDate.getDate() + parseInt(days));
                    expiresAt = expDate.toISOString().replace("T", " ").substring(0, 19);
                }

                const keyHash = await sha256(keyToCreate + requiredSecret(env, "KEY_PEPPER"));

                const insert = env.DB.prepare(
                    "INSERT INTO license_keys (key_hash, expires_at, status) VALUES (?, ?, 'unused')"
                )
                    .bind(keyHash, expiresAt);
                const [insertResult] = await env.DB.batch([insert, env.DB.prepare(
                    "INSERT INTO admin_audit_logs (admin_id,action,key_hash,previous_epoch,new_epoch,reason,ip_address) VALUES (?,?,?,?,?,?,?)"
                ).bind("dashboard", "CREATE_KEY", keyHash, 1, 1, "License created from admin dashboard",
                    request.headers.get("CF-Connecting-IP") || "0.0.0.0")]);

                return jsonResponse({
                    success: true,
                    key: keyToCreate,
                    id: insertResult && insertResult.meta ? insertResult.meta.last_row_id : null,
                    expires_at: expiresAt
                });
            } catch (error) {
                // Log an event name only; database errors can include input material.
                console.error("admin_create_key_failed");
                return jsonResponse({ success: false, message: "Unable to create license." },
                    String(error?.message).includes("UNIQUE constraint failed") ? 409 : 500);
            }
        }

        // REVEAL KEY
        if (url.pathname === "/api/v1/admin/reveal-key" && request.method === "POST") {
            return jsonResponse({success:false,message:'License keys are shown only once, when created.'},410);
        }

        // RESET HWID
        if (url.pathname === "/api/v1/admin/reset-hwid" && request.method === "POST") {
            try {

                const keyId = Number(body.id);
                if (!Number.isSafeInteger(keyId) || keyId <= 0) {
                    return jsonResponse({ success: false, message: "Invalid license id." }, 400);
                }
                const update = await auditedUpdate(env, request, "RESET_HWID", keyId, "HWID reset from admin dashboard",
                    env.DB.prepare("UPDATE license_keys SET hwid_hash = NULL, hwid_hint = NULL, device_key_hash = NULL, status = 'unused', license_epoch = license_epoch + 1 WHERE id = ?")
                        .bind(keyId));
                if (update.meta?.changes !== 1) return jsonResponse({ success: false, message: "License not found." }, 404);

                return jsonResponse({ success: true, message: "HWID Reset Successful" });
            } catch (_) {
                return jsonResponse({ success: false, message: "Unable to reset HWID." }, 500);
            }
        }

        // DELETE KEY
        if (url.pathname === "/api/v1/admin/delete-key" && request.method === "POST") {
            try {

                const keyId = Number(body.id);
                if (!Number.isSafeInteger(keyId) || keyId <= 0) {
                    return jsonResponse({ success: false, message: "Invalid license id." }, 400);
                }

                // Keep an audit record in the same D1 transaction before
                // deleting the license row, so a deleted key is traceable
                // without retaining the usable license itself.
                const results = await env.DB.batch([
                    env.DB.prepare(
                        "INSERT INTO admin_audit_logs (admin_id, action, key_hash, previous_epoch, new_epoch, reason, ip_address) " +
                        "SELECT ?, ?, key_hash, license_epoch, license_epoch, ?, ? FROM license_keys WHERE id = ?"
                    ).bind(
                        "dashboard",
                        "DELETE_KEY",
                        "License permanently deleted from admin dashboard",
                        request.headers.get("CF-Connecting-IP") || "0.0.0.0",
                        keyId
                    ),
                    env.DB.prepare("DELETE FROM license_keys WHERE id = ?").bind(keyId)
                ]);

                const deletion = results && results[1];
                if (!deletion || !deletion.meta || deletion.meta.changes !== 1) {
                    return jsonResponse({ success: false, message: "License not found." }, 404);
                }

                return jsonResponse({ success: true, message: "License deleted." });
            } catch (error) {
                console.error("admin_delete_key_failed");
                return jsonResponse({ success: false, message: "Unable to delete license." }, 500);
            }
        }

        // BAN KEY
        if (url.pathname === "/api/v1/admin/ban-key" && request.method === "POST") {
            try {

                const keyId = Number(body.id);
                const newStatus = String(body.status || "banned").toLowerCase();
                if (!Number.isSafeInteger(keyId) || keyId <= 0 || !["active", "banned"].includes(newStatus)) {
                    return jsonResponse({ success: false, message: "Invalid status request." }, 400);
                }
                const update = await auditedUpdate(env, request, "SET_STATUS", keyId, `Status changed to ${newStatus}`,
                    env.DB.prepare("UPDATE license_keys SET status = CASE WHEN ? = 'active' AND (hwid_hash IS NULL OR hwid_hash = '') THEN 'unused' ELSE ? END, license_epoch = license_epoch + 1 WHERE id = ?")
                        .bind(newStatus, newStatus, keyId));
                if (update.meta?.changes !== 1) return jsonResponse({ success: false, message: "License not found." }, 404);

                return jsonResponse({ success: true, message: "Status Updated" });
            } catch (_) {
                return jsonResponse({ success: false, message: "Unable to update license status." }, 500);
            }
        }

        // CONFIG MODERATION
        if (url.pathname === "/api/v1/admin/delete-config" && request.method === "POST") {
            try {

                const configId = Number(body.id);
                if (!Number.isSafeInteger(configId) || configId <= 0) {
                    return jsonResponse({ success: false, message: "Invalid config id." }, 400);
                }
                const result = await env.DB.prepare("DELETE FROM public_configs WHERE id = ?").bind(configId).run();
                if (!result.meta || result.meta.changes !== 1) {
                    return jsonResponse({ success: false, message: "Config not found." }, 404);
                }
                return jsonResponse({ success: true, message: "Public config deleted." });
            } catch (error) {
                console.error("admin_delete_config_failed");
                return jsonResponse({ success: false, message: "Unable to delete config." }, 500);
            }
        }
    }

    return null;
}
