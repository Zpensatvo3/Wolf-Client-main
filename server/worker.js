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
import { verifyAdminAccess } from './auth/admin-access.mjs';
import {ensureDatabaseSchema} from './database/schema.mjs';
import {handleLauncherV2} from './api/launcher-v2.mjs';
export { RateLimiter } from './core/rate-limit.mjs';

export default {
    async fetch(request, env, ctx) {
        const url = new URL(request.url);
        try {
            if (url.pathname === '/health' && request.method === 'GET') {
                try {
                    for (const name of ['KEY_PEPPER', 'HWID_PEPPER',
                        'SIGNING_PRIVATE_KEY_PKCS8', 'SIGNING_PUBLIC_KEY_SPKI']) requiredSecret(env, name);
                    if (!env.RATE_LIMITER) throw new Error('Missing rate limiter');
                    await ensureDatabaseSchema(env);
                    if (env.AUTH_V2_RELEASES === 'true') {
                        await env.DB.prepare('SELECT session_id,sequence,ticket_hash,exp FROM runtime_sessions LIMIT 0').all();
                        await env.DB.prepare('SELECT sha256,revoked,minecraft_target FROM approved_releases LIMIT 0').all();
                        await env.DB.prepare('SELECT minecraft_target,sha256 FROM release_channels LIMIT 0').all();
                    }
                    return jsonResponse({ status: 'ok', service: 'onestep-auth' });
                } catch (_) { return jsonResponse({ status: 'unavailable' }, 503); }
            }
            if (request.method === 'OPTIONS') return jsonResponse(null, 204);
            if (url.pathname === '/admin' || url.pathname.startsWith('/admin/') || url.pathname.startsWith('/api/v1/admin/')) {
                if (!await verifyAdminAccess(request,env))
                    return jsonResponse({success:false,message:'Administrator identity verification required.'},403);
            }
            const dashboard = dashboardResponse(request);
            if (dashboard) return dashboard;
            const v2 = await handleLauncherV2(request, env, ctx);
            if (v2) return v2;
            if (env.AUTH_V2_ONLY === 'true' && (url.pathname === '/api/v1/auth' ||
                url.pathname.startsWith('/api/v1/launcher/')))
                return jsonResponse({message:'Update required. Use the current OneStep launcher.'},410);
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
