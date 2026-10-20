import {verifySessionToken} from './sessions.mjs';
import {licenseExpiryMs} from '../session-policy.mjs';
import {RUNTIME_AUDIENCE,LEASE_MS} from './protocol-v2.mjs';

export const bindings = ['session_id','key_hash','hwid_hash','device_key_hash','license_epoch',
    'release_sha256','release_version','minecraft_target','pid','process_nonce','sequence'];

export async function activeRelease(env, hash, target) {
    const release = await env.DB.prepare('SELECT * FROM approved_releases WHERE sha256 = ? AND minecraft_target = ? AND revoked = 0')
        .bind(hash,target).first();
    if (!release) throw Error('Release unavailable');
    return release;
}

export async function activeLicenseV2(env, claims) {
    const row = await env.DB.prepare('SELECT * FROM license_keys WHERE key_hash = ?').bind(claims.key_hash).first();
    const expiry = row ? licenseExpiryMs(row.expires_at) : NaN;
    if (!row || row.status !== 'active' || Number(row.license_epoch) !== claims.license_epoch ||
        row.hwid_hash !== claims.hwid_hash || row.device_key_hash !== claims.device_key_hash ||
        (expiry !== null && (!Number.isFinite(expiry) || expiry <= Date.now()))) throw Error('Inactive license');
    return row;
}

export async function requireRuntimeSession(token, env) {
    const claims = await verifySessionToken(token,env,RUNTIME_AUDIENCE);
    if (claims.protocol !== 2 || claims.exp-claims.iat > LEASE_MS) throw Error('Invalid lease');
    const row = await env.DB.prepare('SELECT * FROM runtime_sessions WHERE session_id = ? AND revoked = 0 AND ticket_hash IS NULL AND exp > ?')
        .bind(claims.session_id,Date.now()).first();
    if (!row || bindings.some(name=>row[name] !== claims[name]) || row.exp !== claims.exp) throw Error('Runtime session revoked');
    await activeLicenseV2(env,claims);
    await activeRelease(env,claims.release_sha256,claims.minecraft_target);
    return claims;
}
