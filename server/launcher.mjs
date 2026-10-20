import { verifyDeviceProof, hashText } from './device-proof.mjs';
import { licenseExpiryMs, sessionExpiryMs } from './session-policy.mjs';

export const MAX_RELEASE_BYTES = 24 * 1024 * 1024;
const HEX = /^[a-f0-9]{64}$/;
export function validRelease(release) {
    return release && /^[0-9A-Za-z][0-9A-Za-z._-]{0,63}$/.test(release.version) &&
        HEX.test(release.sha256) && Number.isSafeInteger(release.bytes) &&
        release.bytes >= 4096 && release.bytes <= MAX_RELEASE_BYTES;
}
export function validPid(pid) { return Number.isSafeInteger(pid) && pid > 0 && pid <= 0xffffffff; }

async function currentRelease(env) {
    const release = await env.RELEASES.get('current', 'json');
    if (!validRelease(release)) throw new Error('Release unavailable');
    return release;
}

async function activeLicense(env, claims) {
    const row = await env.DB.prepare('SELECT * FROM license_keys WHERE key_hash = ? LIMIT 1')
        .bind(claims.key_hash).first();
    const expiry = row ? licenseExpiryMs(row.expires_at) : NaN;
    if (!row || row.status !== 'active' || Number(row.license_epoch) !== claims.license_epoch ||
        row.hwid_hash !== claims.hwid_hash || !row.device_key_hash ||
        row.device_key_hash !== claims.device_key_hash ||
        (expiry !== null && (!Number.isFinite(expiry) || expiry <= Date.now()))) throw new Error('Inactive license');
    return row;
}

export async function handleLauncher(request, env, ctx, s) {
    const path = new URL(request.url).pathname;
    if (!path.startsWith('/api/v1/launcher/') && path !== '/api/v1/admin/release') return null;
    const reply = s.json;
    try {
        if (!env.RELEASES) return reply({ message: 'Release service unavailable.' }, 503);
        const retry = await s.rate(request, env, 'launcher', 24, 60000);
        if (retry !== null) return reply({ message: 'Please wait before retrying.' }, 429);
        if (path === '/api/v1/admin/release') {
            if (!await s.admin(request, env)) return reply({ message: 'Unauthorized' }, 401);
            if (request.method !== 'PUT') return reply({ message: 'Method not allowed' }, 405);
            const release = { version: request.headers.get('X-Release-Version'),
                sha256: request.headers.get('X-Release-Sha256'),
                bytes: Number(request.headers.get('Content-Length')) };
            if (!validRelease(release) || !request.body) return reply({ message: 'Invalid release' }, 400);
            // Bound and hash the stream; a failed upload never changes the current pointer.
            const { createHash } = await import('node:crypto');
            const hash = createHash('sha256');
            let received = 0;
            const checked = request.body.pipeThrough(new TransformStream({
                transform(chunk, controller) {
                    received += chunk.byteLength;
                    if (received > release.bytes) throw new Error('Release too large');
                    hash.update(chunk);
                    controller.enqueue(chunk);
                },
                flush() {
                    if (received !== release.bytes || hash.digest('hex') !== release.sha256)
                        throw new Error('Release hash mismatch');
                }
            }));
            await env.RELEASES.put('dll/' + release.sha256, checked);
            if (env.AUTH_V2_RELEASES === 'true') {
                const target=request.headers.get('X-Minecraft-Target') || '1.21.11';
                if (!['1.21.1','1.21.4','1.21.8','1.21.11','26.1','26.1.1','26.1.2'].includes(target))
                    return reply({message:'Unsupported Minecraft target.'},400);
                const old=await env.DB.prepare('SELECT * FROM approved_releases WHERE sha256=?').bind(release.sha256).first();
                if (old && (old.revoked || old.version!==release.version || old.minecraft_target!==target || old.bytes!==release.bytes))
                    return reply({message:'Release identity is immutable; publish a new build.'},409);
                await env.DB.batch([
                    env.DB.prepare('INSERT INTO approved_releases(sha256,version,minecraft_target,bytes,created_at) VALUES(?,?,?,?,?) ON CONFLICT(sha256) DO NOTHING')
                        .bind(release.sha256,release.version,target,release.bytes,Date.now()),
                    env.DB.prepare('INSERT INTO release_channels(minecraft_target,sha256) VALUES(?,?) ON CONFLICT(minecraft_target) DO UPDATE SET sha256=excluded.sha256')
                        .bind(target,release.sha256)
                ]);
            }
            await env.RELEASES.put('current', JSON.stringify(release));
            return reply({ success: true, ...release });
        }
        if (request.method !== 'POST') return reply({ message: 'Method not allowed' }, 405);
        const body = await s.read(request, 16384);
        if (!Number.isSafeInteger(body.timestamp) || Math.abs(Date.now() - body.timestamp) > 30000)
            return reply({ message: 'Request expired. Check Windows time.' }, 408);
        const redeem = path === '/api/v1/launcher/redeem';
        if (body.request_type !== (redeem ? 'redeem' : 'heartbeat')) return reply({ message: 'Invalid proof type' }, 400);
        const deviceHash = await verifyDeviceProof(body);
        const hwidHash = await s.hash(body.hwid + s.secret(env, 'HWID_PEPPER'));
        if (redeem) {
            if (!HEX.test(body.ticket || '') || !HEX.test(body.release_sha256 || '') || !validPid(body.pid))
                return reply({ message: 'Invalid launch ticket' }, 400);
            const ticketHash = await hashText(body.ticket);
            const ticket = await env.DB.prepare('SELECT * FROM launcher_tickets WHERE ticket_hash = ?')
                .bind(ticketHash).first();
            if (!ticket || ticket.exp <= Date.now() || ticket.pid !== body.pid ||
                ticket.release_sha256 !== body.release_sha256 || ticket.hwid_hash !== hwidHash ||
                ticket.device_key_hash !== deviceHash) return reply({ message: 'Launch ticket expired or invalid.' }, 403);
            const license = await activeLicense(env, ticket);
            // Atomic consumption: concurrent redemptions cannot both succeed.
            const consumed = await env.DB.prepare('DELETE FROM launcher_tickets WHERE ticket_hash = ? AND exp > ? RETURNING ticket_hash')
                .bind(ticketHash, Date.now()).first();
            if (!consumed) return reply({ message: 'Launch ticket already used.' }, 403);
            const now = Date.now();
            const exp = sessionExpiryMs(now, licenseExpiryMs(license.expires_at));
            if (!exp) return reply({ message: 'License expired.' }, 403);
            return reply(await s.sign({ success: true, aud: 'wolf-client-v1',
                session_id: crypto.randomUUID(), key_hash: ticket.key_hash, hwid_hash: hwidHash,
                device_key_hash: deviceHash, license_epoch: ticket.license_epoch,
                nonce_hash: await hashText(body.nonce), pid: body.pid,
                release_sha256: body.release_sha256, iat: now, exp,
                expires_at: license.expires_at || 'lifetime' }, env));
        }
        const claims = await s.session(body, env);
        if (claims.config_only || !claims.device_key_hash || claims.device_key_hash !== deviceHash ||
            claims.hwid_hash !== hwidHash) return reply({ message: 'Device session required.' }, 403);
        await activeLicense(env, claims);
        const release = await currentRelease(env);
        if (path === '/api/v1/launcher/manifest') {
            const now = Date.now();
            return reply(await s.sign({ aud: 'onestep-release-v1', ...release, iat: now, exp: now + 600000 }, env));
        }
        if (body.release_sha256 !== release.sha256) return reply({ message: 'Update available. Sign in again.' }, 409);
        if (path === '/api/v1/launcher/download') {
            const stream = await env.RELEASES.get('dll/' + release.sha256, 'stream');
            if (!stream) return reply({ message: 'Release is propagating. Please retry shortly.' }, 503);
            return new Response(stream, { headers: { 'Content-Type': 'application/octet-stream',
                'Content-Length': String(release.bytes), 'Content-Disposition': 'attachment; filename="Onestep.dll"',
                'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff' } });
        }
        if (path === '/api/v1/launcher/ticket') {
            if (!validPid(body.pid)) return reply({ message: 'Invalid Minecraft process.' }, 400);
            const ticket = Array.from(crypto.getRandomValues(new Uint8Array(32)), x => x.toString(16).padStart(2, '0')).join('');
            const now = Date.now();
            await env.DB.batch([
                env.DB.prepare('DELETE FROM launcher_tickets WHERE exp <= ?').bind(now),
                env.DB.prepare('INSERT INTO launcher_tickets (ticket_hash,key_hash,hwid_hash,device_key_hash,license_epoch,pid,release_sha256,exp) VALUES (?,?,?,?,?,?,?,?)')
                    .bind(await hashText(ticket), claims.key_hash, hwidHash, deviceHash, claims.license_epoch, body.pid, release.sha256, now + 120000)
            ]);
            return reply({ ticket, expires_in: 120 });
        }
        return reply({ message: 'Unknown launcher endpoint' }, 404);
    } catch (_) {
        // No credentials, proofs or request bodies in logs or error responses.
        return reply({ message: 'Launcher request could not be verified. Please sign in again.' }, 403);
    }
}
