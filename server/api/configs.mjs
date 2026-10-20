import { MAX_CONFIG_JSON_BYTES, jsonResponse, readConfigJson } from '../core/http.mjs';
import { requireActiveConfigSession } from '../auth/sessions.mjs';
import { ensureDatabaseSchema } from '../database/schema.mjs';
import { enforceRateLimit } from '../core/rate-limit.mjs';
const MAX_CONFIGS_PER_PAGE = 80;
const MINECRAFT_NAME = /^[A-Za-z0-9_]{3,16}$/;

function validateConfigInput(body) {
    const name = String(body && body.name || "").trim();
    const description = String(body && body.description || "").trim();
    const authorName = String(body && body.author_name || "").trim();
    const payload = String(body && body.payload || "");
    const moduleCount = Number(body && body.module_count || 0);
    if (name.length < 2 || name.length > 48 || description.length > 180 ||
        !MINECRAFT_NAME.test(authorName) || payload.length < 8 ||
        payload.length > MAX_CONFIG_JSON_BYTES || !Number.isSafeInteger(moduleCount) ||
        moduleCount < 0 || moduleCount > 128) {
        throw new Error("Invalid config fields");
    }
    const parsedPayload = JSON.parse(payload);
    if (!parsedPayload || typeof parsedPayload !== "object" || Array.isArray(parsedPayload) ||
        !Array.isArray(parsedPayload.modules) || parsedPayload.modules.length > 128) {
        throw new Error("Invalid config payload");
    }
    return { name, description, authorName, payload, moduleCount };
}

export async function handleConfigs(request, env, ctx) {
    const url = new URL(request.url);
    if (url.pathname.startsWith("/api/v1/configs/")) {
        try {
            await ensureDatabaseSchema(env);
            const retryAfter = await enforceRateLimit(request, env, "configs", 30, 60 * 1000);
            if (retryAfter !== null) {
                return jsonResponse({ success: false, message: "Too many config requests. Please wait." }, 429);
            }
            if (request.method !== "POST") {
                return jsonResponse({ success: false, message: "Method not allowed." }, 405);
            }

            const body = await readConfigJson(request);
            let claims;
            try { claims = await requireActiveConfigSession(body, env); }
            catch (_) { return jsonResponse({success:false,message:'Runtime permission required.'},403); }

            if (url.pathname === "/api/v1/configs/list") {
                const { results } = await env.DB.prepare(
                    "SELECT id, author_name, name, description, module_count, downloads, created_at, updated_at, " +
                    "CASE WHEN owner_key_hash = ? OR owner_hwid_hash = ? THEN 1 ELSE 0 END AS mine " +
                    "FROM public_configs ORDER BY updated_at DESC, id DESC LIMIT ?"
                ).bind(claims.key_hash, claims.hwid_hash, MAX_CONFIGS_PER_PAGE).all();
                return jsonResponse({ success: true, configs: results || [] });
            }

            if (url.pathname === "/api/v1/configs/publish") {
                const input = validateConfigInput(body);
                const requestedId = Number(body.id || 0);
                if (requestedId > 0) {
                    if (!Number.isSafeInteger(requestedId)) {
                        return jsonResponse({ success: false, message: "Invalid config id." }, 400);
                    }
                    const update = await env.DB.prepare(
                        "UPDATE public_configs SET owner_key_hash = ?, owner_hwid_hash = ?, author_name = ?, name = ?, description = ?, payload = ?, module_count = ?, updated_at = CURRENT_TIMESTAMP " +
                        "WHERE id = ? AND (owner_key_hash = ? OR owner_hwid_hash = ?)"
                    ).bind(claims.key_hash, claims.hwid_hash, input.authorName, input.name, input.description,
                        input.payload, input.moduleCount, requestedId, claims.key_hash, claims.hwid_hash).run();
                    if (!update.meta || update.meta.changes !== 1) {
                        return jsonResponse({ success: false, message: "Only the original publisher can update this config." }, 403);
                    }
                    return jsonResponse({ success: true, id: requestedId, message: "Config updated." });
                }

                const insert = await env.DB.prepare(
                    "INSERT INTO public_configs (owner_key_hash, owner_hwid_hash, author_name, name, description, payload, module_count) " +
                    "VALUES (?, ?, ?, ?, ?, ?, ?)"
                ).bind(claims.key_hash, claims.hwid_hash, input.authorName, input.name, input.description,
                    input.payload, input.moduleCount).run();
                return jsonResponse({
                    success: true,
                    id: insert.meta ? insert.meta.last_row_id : null,
                    message: "Config published publicly."
                });
            }

            if (url.pathname === "/api/v1/configs/load") {
                const configId = Number(body.id);
                if (!Number.isSafeInteger(configId) || configId <= 0) {
                    return jsonResponse({ success: false, message: "Invalid config id." }, 400);
                }
                const config = await env.DB.prepare(
                    "SELECT id, author_name, name, description, payload, module_count, downloads, created_at, updated_at " +
                    "FROM public_configs WHERE id = ? LIMIT 1"
                ).bind(configId).first();
                if (!config) return jsonResponse({ success: false, message: "Config not found." }, 404);
                await env.DB.prepare("UPDATE public_configs SET downloads = downloads + 1 WHERE id = ?").bind(configId).run();
                return jsonResponse({ success: true, config: { ...config, downloads: Number(config.downloads || 0) + 1 } });
            }

            if (url.pathname === "/api/v1/configs/delete") {
                const configId = Number(body.id);
                if (!Number.isSafeInteger(configId) || configId <= 0) {
                    return jsonResponse({ success: false, message: "Invalid config id." }, 400);
                }
                const deletion = await env.DB.prepare(
                    "DELETE FROM public_configs WHERE id = ? AND (owner_key_hash = ? OR owner_hwid_hash = ?)"
                ).bind(configId, claims.key_hash, claims.hwid_hash).run();
                if (!deletion.meta || deletion.meta.changes !== 1) {
                    return jsonResponse({ success: false, message: "Only the original publisher can delete this config." }, 403);
                }
                return jsonResponse({ success: true, message: "Config deleted." });
            }

            return jsonResponse({ success: false, message: "Invalid config endpoint." }, 404);
        } catch (error) {
            const message = error && error.message === "Expired or malformed session token"
                ? "Config session expired. Please re-open the client."
                : "Config service is temporarily unavailable.";
            console.error("configs_request_failed");
            return jsonResponse({ success: false, message }, 500);
        }
    }

    return null;
}
