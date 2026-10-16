// OneStep composition root. Auth/session wire formats remain compatible.
import { jsonResponse, readBoundedJson } from './core/http.mjs';
import { requiredSecret, sha256 } from './core/crypto.mjs';
import { enforceRateLimit } from './core/rate-limit.mjs';
import { requireActiveConfigSession, signPayload } from './auth/sessions.mjs';
import { handleAdmin, verifyAdminAuth } from './api/admin.mjs';
import { handleAuth } from './api/auth.mjs';
import { handleConfigs } from './api/configs.mjs';
import { handleLauncher } from './launcher.mjs';
import { dashboardResponse } from './dashboard/index.mjs';
export { RateLimiter } from './core/rate-limit.mjs';

export default {
    async fetch(request, env, ctx) {
        const url = new URL(request.url);
        try {
            if (url.pathname === '/health' && request.method === 'GET') {
                try {
                    for (const name of ['KEY_PEPPER', 'HWID_PEPPER', 'ADMIN_TOKEN_HASH',
                        'SIGNING_PRIVATE_KEY_PKCS8', 'SIGNING_PUBLIC_KEY_SPKI']) requiredSecret(env, name);
                    if (!env.RATE_LIMITER) throw new Error('Missing rate limiter');
                    await env.DB.prepare('SELECT 1').first();
                    return jsonResponse({ status: 'ok', service: 'onestep-auth' });
                } catch (_) { return jsonResponse({ status: 'unavailable' }, 503); }
            }
            if (request.method === 'OPTIONS') return jsonResponse(null, 204);
            const dashboard = dashboardResponse(request);
            if (dashboard) return dashboard;
            const launcher = await handleLauncher(request, env, ctx, {
                json: jsonResponse, read: readBoundedJson, rate: enforceRateLimit,
                admin: verifyAdminAuth, session: requireActiveConfigSession,
                hash: sha256, secret: requiredSecret, sign: signPayload
            });
            if (launcher) return launcher;
            for (const handler of [handleAdmin, handleConfigs, handleAuth]) {
                const response = await handler(request, env, ctx);
                if (response) return response;
            }
            return jsonResponse({ error: 'Invalid Endpoint' }, 404);
        } catch (_) {
            console.error('request_failed');
            return jsonResponse({ success: false, message: 'Service unavailable.' }, 503);
        }
    }
};
