import { jsonResponse } from '../core/http.mjs';
import { requiredSecret } from '../core/crypto.mjs';
import { validRelease } from '../launcher.mjs';

// Display expiry immediately, even before the next client heartbeat persists it.
const effectiveStatus = `CASE WHEN status = 'banned' THEN 'banned'
    WHEN status = 'expired' OR (expires_at IS NOT NULL AND expires_at != '' AND
        (julianday(expires_at) IS NULL OR julianday(expires_at) <= julianday('now')))
    THEN 'expired' ELSE status END`;
const licenseFields = `id, key_hash, hwid_hash, hwid_hint, license_epoch, expires_at,
    status, activated_at, created_at,
    (hwid_hash IS NOT NULL AND hwid_hash != '') AS hwid_bound,
    (device_key_hash IS NOT NULL AND device_key_hash != '') AS device_bound,
    ${effectiveStatus} AS effective_status`;

function pagination(url) {
    const page = Number(url.searchParams.get('page') || 1);
    const size = Number(url.searchParams.get('page_size') || 20);
    if (!Number.isSafeInteger(page) || page < 1 || page > 1000000 ||
        !Number.isSafeInteger(size) || size < 1 || size > 100) throw new Error('Invalid pagination');
    return { page, size, offset: (page - 1) * size };
}

// Called only after the common admin authorization and rate limit checks.
export async function handleAdminRead(request, env) {
    const url = new URL(request.url);
    const path = url.pathname;
    if (path === '/api/v1/admin/overview') {
        const { results } = await env.DB.prepare(`SELECT ${effectiveStatus} AS status, COUNT(*) AS count
            FROM license_keys GROUP BY 1`).all();
        const stats = { total: 0, active: 0, unused: 0, banned: 0, expired: 0 };
        for (const row of results || []) {
            stats.total += Number(row.count);
            if (Object.hasOwn(stats, row.status) && row.status !== 'total') stats[row.status] = Number(row.count);
        }
        const activity = await env.DB.prepare('SELECT id, action, key_hash, previous_epoch, new_epoch, reason, timestamp FROM admin_audit_logs ORDER BY id DESC LIMIT 6').all();
        return jsonResponse({ success: true, stats, activity: activity.results || [], as_of: new Date().toISOString() });
    }
    if (path === '/api/v1/admin/list-keys') {
        // Keep the original unpaginated contract for existing admin clients.
        if (!url.searchParams.has('page')) {
            const { results } = await env.DB.prepare(`SELECT ${licenseFields} FROM license_keys ORDER BY id DESC`).all();
            return jsonResponse({ success: true, keys: results || [] });
        }
        let paging;
        try { paging = pagination(url); } catch (_) { return jsonResponse({ success: false, message: 'Invalid pagination.' }, 400); }
        const query = (url.searchParams.get('search') || '').trim();
        const status = url.searchParams.get('status') || 'all';
        const sorts = { newest: 'id DESC', oldest: 'id ASC', expiry: 'expires_at IS NULL, expires_at ASC, id DESC' };
        const order = url.searchParams.get('sort') || 'newest';
        if (query.length > 128 || !['all', 'active', 'unused', 'banned', 'expired'].includes(status) || !Object.hasOwn(sorts, order))
            return jsonResponse({ success: false, message: 'Invalid license filter.' }, 400);
        const like = '%' + query.replace(/[\\%_]/g, '\\$&') + '%';
        const where = ` WHERE (? = 'all' OR (${effectiveStatus}) = ?) AND
            (CAST(id AS TEXT) LIKE ? ESCAPE '\\' OR key_hash LIKE ? ESCAPE '\\' OR COALESCE(hwid_hint, '') LIKE ? ESCAPE '\\')`;
        const values = [status, status, like, like, like];
        const count = await env.DB.prepare('SELECT COUNT(*) AS total FROM license_keys' + where).bind(...values).first();
        const { results } = await env.DB.prepare(`SELECT ${licenseFields} FROM license_keys${where} ORDER BY ${sorts[order]} LIMIT ? OFFSET ?`)
            .bind(...values, paging.size, paging.offset).all();
        return jsonResponse({ success: true, keys: results || [], total: Number(count.total), page: paging.page, page_size: paging.size });
    }
    if (path === '/api/v1/admin/activity') {
        let paging;
        try { paging = pagination(url); } catch (_) { return jsonResponse({ success: false, message: 'Invalid pagination.' }, 400); }
        const { results } = await env.DB.prepare('SELECT id, admin_id, action, key_hash, previous_epoch, new_epoch, reason, timestamp FROM admin_audit_logs ORDER BY id DESC LIMIT ? OFFSET ?')
            .bind(paging.size, paging.offset).all();
        const count = await env.DB.prepare('SELECT COUNT(*) AS total FROM admin_audit_logs').first();
        return jsonResponse({ success: true, activity: results || [], total: Number(count.total), page: paging.page, page_size: paging.size });
    }
    if (path === '/api/v1/admin/status') {
        const configured = ['KEY_PEPPER', 'HWID_PEPPER', 'SIGNING_PRIVATE_KEY_PKCS8', 'SIGNING_PUBLIC_KEY_SPKI']
            .every(name => { try { requiredSecret(env, name); return true; } catch (_) { return false; } }) &&
            ['ADMIN_ACCESS_TEAM_DOMAIN', 'ADMIN_ACCESS_AUD', 'ADMIN_ACCESS_EMAIL']
                .every(name => typeof env[name] === 'string' && env[name].length > 0);
        await env.DB.prepare('SELECT 1').first();
        let release = null;
        try {
            const value = env.AUTH_V2_RELEASES === 'true'
                ? await env.DB.prepare(`SELECT r.version,r.bytes,r.sha256 FROM release_channels c
                    JOIN approved_releases r ON r.sha256=c.sha256 WHERE c.minecraft_target='1.21.11' AND r.revoked=0`).first()
                : await env.RELEASES?.get('current', 'json');
            if (validRelease(value)) release = { version: value.version, bytes: value.bytes };
        } catch (_) { /* A release may not be published yet. */ }
        return jsonResponse({ success: true, service: 'onestep-auth', database: 'ok',
            auth_configured: configured, rate_limiter_configured: Boolean(env.RATE_LIMITER),
            releases_configured: Boolean(env.RELEASES), device_configs: false, admin_auth: 'cloudflare-access',
            release, runtime_protocol:env.AUTH_V2_ONLY === 'true' ? 2 : 1, checked_at: new Date().toISOString() });
    }
    return null;
}
