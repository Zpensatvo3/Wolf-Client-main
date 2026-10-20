import assert from 'node:assert/strict';
import { test } from 'node:test';
import { harness, LocalD1 } from './helpers/auth-harness.mjs';
import worker, { RateLimiter } from '../server/worker.js';
import { ensureDatabaseSchema } from '../server/database/schema.mjs';
import { signPayload, verifySessionToken } from '../server/auth/sessions.mjs';
import { sha256 } from '../server/core/crypto.mjs';

async function create(h, key = 'LOCAL-LICENSE-1234', days = 'lifetime') {
    const response = await h.request('/api/v1/admin/create-key', { days }, { admin: true });
    assert.equal(response.status, 200); return response.json();
}
async function activate(h, key) {
    const device = await h.device();
    const response = await h.request('/api/v1/auth', await device.proof('activate', key));
    assert.equal(response.status, 200); const result = await response.json();
    return { device, result, claims: await verifySessionToken(result.session_token, h.env) };
}

test('admin authorization, methods, pagination, expiry statistics and protected reveal', async () => {
    const h = await harness();
    try {
        assert.equal((await h.request('/api/v1/admin/list-keys',undefined,{headers:{'Cf-Access-Jwt-Assertion':''}})).status, 403);
        assert.equal((await h.request('/api/v1/admin/list-keys', undefined, { headers: { 'Cf-Access-Jwt-Assertion':'', Authorization: 'Bearer ' + 'x'.repeat(40) } })).status, 403);
        assert.equal((await h.request('/api/v1/admin/create-key', undefined, { admin: true })).status, 405);
        assert.equal((await h.request('/api/v1/admin/missing', undefined, { admin: true })).status, 404);
        assert.equal((await h.request('/api/v1/admin/create-key', { key: [], days: '30' }, { admin: true })).status, 400);
        assert.equal((await h.request('/api/v1/admin/create-key', { days: '365' }, { admin: true })).status, 400);
        const created = await create(h);
        assert.equal((await h.request('/api/v1/admin/create-key', { key: created.key }, { admin: true })).status, 400);
        await create(h, 'LOCAL-LICENSE-SECOND', '7');
        h.env.DB.sqlite.prepare("UPDATE license_keys SET expires_at='2020-01-01 00:00:00' WHERE id=?").run(created.id);
        const overview = await (await h.request('/api/v1/admin/overview', undefined, { admin: true })).json();
        assert.deepEqual(overview.stats, { total: 2, active: 0, unused: 1, banned: 0, expired: 1 });
        assert.equal(overview.activity.length, 2);
        const expiredDevice = await h.device();
        assert.equal((await h.request('/api/v1/auth', await expiredDevice.proof('activate', created.key))).status, 403);
        const response = await h.request('/api/v1/admin/list-keys?page=1&page_size=1&status=expired', undefined, { admin: true });
        const text = await response.text(), data = JSON.parse(text);
        assert.equal(data.total, 1); assert.equal(data.keys.length, 1); assert.equal(data.keys[0].id, created.id);
        assert.ok(!text.includes(created.key) && !text.includes('key_encrypted') && !text.includes('device_key_hash'));
        assert.equal((await h.request('/api/v1/admin/list-keys?page=-1', undefined, { admin: true })).status, 400);
        assert.equal((await h.request('/api/v1/admin/list-keys?page=1&sort=id%3BDROP', undefined, { admin: true })).status, 400);
        const wildcard = await (await h.request('/api/v1/admin/list-keys?page=1&search=%25', undefined, { admin: true })).json();
        assert.equal(wildcard.total, 0);
        const revealResponse = await h.request('/api/v1/admin/reveal-key', { id: created.id }, { admin:true });
        assert.equal(revealResponse.status,410); assert.ok(!(await revealResponse.text()).includes(created.key));
        h.env.DB.sqlite.prepare('UPDATE license_keys SET key_encrypted=NULL WHERE id=?').run(created.id);
        assert.equal((await h.request('/api/v1/admin/reveal-key', { id: created.id }, { admin: true })).status, 410);
        assert.equal((await h.request('/api/v1/admin/reveal-key', { id: 999 }, { admin: true })).status, 410);
        for (const route of ['reset-hwid', 'ban-key', 'delete-key'])
            assert.equal((await h.request('/api/v1/admin/' + route, { id: 999 }, { admin: true })).status, 404);
        const status = await (await h.request('/api/v1/admin/status', undefined, { admin: true })).json();
        assert.equal(status.auth_configured, true); assert.equal(status.release, null);
    } finally { await h.close(); }
});

test('activation, signed audience, heartbeat, binding mismatch, ban, epoch and reset revocation', async () => {
    const h = await harness();
    try {
        const license = await create(h), active = await activate(h, license.key);
        assert.equal(active.claims.aud, 'wolf-client-v1'); assert.equal(active.claims.license_epoch, 1);
        assert.match(active.claims.device_key_hash, /^[a-f0-9]{64}$/);
        const heartbeat = () => active.device.proof('heartbeat', active.result.session_token);
        assert.equal((await h.request('/api/v1/auth', await heartbeat())).status, 200);
        const other = await h.device();
        assert.equal((await h.request('/api/v1/auth', await other.proof('activate', license.key))).status, 403);
        const tampered = await heartbeat(); tampered.nonce = 'a'.repeat(32);
        assert.equal((await h.request('/api/v1/auth', tampered)).status, 403);
        assert.equal((await h.request('/api/v1/admin/ban-key', { id: license.id, status: 'banned' }, { admin: true })).status, 200);
        assert.equal((await h.request('/api/v1/auth', await heartbeat())).status, 403);
        assert.equal((await h.request('/api/v1/admin/ban-key', { id: license.id, status: 'active' }, { admin: true })).status, 200);
        assert.equal((await h.request('/api/v1/auth', await heartbeat())).status, 403);
        const relogin = await h.request('/api/v1/auth', await active.device.proof('activate', license.key));
        assert.equal(relogin.status, 200);
        assert.equal((await h.request('/api/v1/admin/reset-hwid', { id: license.id }, { admin: true })).status, 200);
        assert.equal((await h.request('/api/v1/auth', await heartbeat())).status, 403);
        const row = h.env.DB.sqlite.prepare('SELECT * FROM license_keys WHERE id=?').get(license.id);
        assert.equal(row.hwid_hash, null); assert.equal(row.device_key_hash, null); assert.equal(row.license_epoch, 4);
        assert.equal((await h.request('/api/v1/auth', await other.proof('activate', license.key))).status, 200);
        await h.request('/api/v1/admin/delete-key', { id: license.id }, { admin: true });
        assert.equal(h.env.DB.sqlite.prepare('SELECT COUNT(*) AS n FROM license_keys').get().n, 0);
        assert.equal(h.env.DB.sqlite.prepare("SELECT COUNT(*) AS n FROM admin_audit_logs WHERE action='DELETE_KEY'").get().n, 1);
    } finally { await h.close(); }
});

test('existing pre-device sessions and license hashes remain compatible; invalid signatures fail closed', async () => {
    const h = await harness();
    try {
        const license = await create(h); const hwid = 'a'.repeat(64);
        const hwidHash = await sha256(hwid + h.env.HWID_PEPPER), keyHash = await sha256(license.key + h.env.KEY_PEPPER);
        h.env.DB.sqlite.prepare("UPDATE license_keys SET status='active',hwid_hash=? WHERE id=?").run(hwidHash, license.id);
        const now = Date.now();
        const claims = { aud: 'wolf-client-v1', success: true, key_hash: keyHash, hwid_hash: hwidHash, license_epoch: 1, iat: now, exp: now + 600000 };
        const signed = await signPayload(claims, h.env);
        assert.deepEqual(await verifySessionToken(signed.session_token, h.env), {...claims,kid:'primary'});
        const request = { request_type: 'heartbeat', hwid, nonce: 'b'.repeat(32), timestamp: Date.now(), session_token: signed.session_token };
        assert.equal((await h.request('/api/v1/auth', request)).status, 403);
        const wrongAudience = await signPayload({ ...claims, aud: 'onestep-client-v1' }, h.env);
        await assert.rejects(verifySessionToken(wrongAudience.session_token, h.env));
        await assert.rejects(verifySessionToken(signed.session_token.slice(1), h.env));
        const expired = await signPayload({ ...claims, iat: now - 600000, exp: now - 1 }, h.env);
        await assert.rejects(verifySessionToken(expired.session_token, h.env));
    } finally { await h.close(); }
});

test('concurrent activation has one winner and failed audit writes roll back access changes', async () => {
    const h = await harness();
    try {
        const license = await create(h), first = await h.device(), second = await h.device();
        const responses = await Promise.all([
            h.request('/api/v1/auth', await first.proof('activate', license.key)),
            h.request('/api/v1/auth', await second.proof('activate', license.key))
        ]);
        assert.equal(responses.filter(response => response.status === 200).length, 1);
        assert.ok(responses.some(response => [403, 409].includes(response.status)));
        const before = { ...h.env.DB.sqlite.prepare('SELECT * FROM license_keys WHERE id=?').get(license.id) };
        h.env.DB.sqlite.exec("CREATE TRIGGER reject_test_audit BEFORE INSERT ON admin_audit_logs BEGIN SELECT RAISE(ABORT,'Local audit test failure'); END;");
        const response = await h.request('/api/v1/admin/ban-key', { id: license.id, status: 'banned' }, { admin: true });
        assert.equal(response.status, 500);
        assert.deepEqual({ ...h.env.DB.sqlite.prepare('SELECT * FROM license_keys WHERE id=?').get(license.id) }, before);
    } finally { await h.close(); }
});

test('public configs preserve ownership, payloads, download counters and session revocation', async () => {
    const h = await harness();
    try {
        const first = await create(h), second = await create(h, 'LOCAL-OTHER-LICENSE');
        const a = await activate(h, first.key), b = await activate(h, second.key);
        const call = (route, body, owner = a) => h.request('/api/v1/configs/' + route, { session_token: owner.result.session_token, ...body });
        const input = { name: '<img src=x onerror=alert(1)>', description: '<script>bad()</script>', author_name: 'Player_1', payload: '{"modules":[]}', module_count: 0 };
        const published = await (await call('publish', input)).json(); assert.equal(published.success, true);
        const list = await (await call('list', {})).json(); assert.equal(list.configs[0].mine, 1); assert.equal(list.configs[0].name, input.name);
        assert.equal((await (await call('list', {}, b)).json()).configs[0].mine, 0);
        assert.equal((await call('publish', { ...input, id: published.id }, b)).status, 403);
        assert.equal((await call('delete', { id: published.id }, b)).status, 403);
        const loaded = await (await call('load', { id: published.id })).json();
        assert.equal(loaded.config.payload, input.payload); assert.equal(loaded.config.downloads, 1);
        assert.equal((await call('publish', { ...input, id: published.id, description: 'Updated' })).status, 200);
        await h.request('/api/v1/admin/reset-hwid', { id: first.id }, { admin: true });
        assert.notEqual((await call('list', {})).status, 200);
        assert.notEqual((await h.request('/api/v1/configs/list', { device_id: 'a'.repeat(64) })).status, 200);
        assert.equal((await h.request('/api/v1/admin/delete-config', { id: published.id }, { admin: true })).status, 200);
    } finally { await h.close(); }
});

test('launcher release verification, device tickets, single redemption and revocation', async () => {
    const h = await harness();
    try {
        const license = await create(h), a = await activate(h, license.key);
        const bytes = crypto.getRandomValues(new Uint8Array(4096));
        const hash = Buffer.from(await crypto.subtle.digest('SHA-256', bytes)).toString('hex');
        async function upload(digest) {
            return worker.fetch(new Request('https://local.test/api/v1/admin/release', { method: 'PUT',
                headers: { ...h.access.header, Origin:'https://local.test', 'X-Release-Version': 'local.1', 'X-Release-Sha256': digest, 'Content-Length': '4096' }, body: bytes }), h.env, h.ctx);
        }
        assert.equal((await upload('a'.repeat(64))).status, 403); assert.equal(h.releases.has('current'), false);
        assert.equal((await upload(hash)).status, 200);
        const call = async (route, extra = {}) => h.request('/api/v1/launcher/' + route, await a.device.proof('heartbeat', a.result.session_token, extra));
        const manifest = await (await call('manifest')).json();
        const claims = JSON.parse(Buffer.from(manifest.payload, 'base64url'));
        assert.equal(claims.aud, 'onestep-release-v1'); assert.equal(claims.sha256, hash);
        const pub = await crypto.subtle.importKey('spki', Buffer.from(h.env.SIGNING_PUBLIC_KEY_SPKI, 'base64url'), { name: 'ECDSA', namedCurve: 'P-256' }, false, ['verify']);
        assert.equal(await crypto.subtle.verify({ name: 'ECDSA', hash: 'SHA-256' }, pub, Buffer.from(manifest.signature, 'base64url'), Buffer.from(manifest.payload, 'base64url')), true);
        assert.equal((await call('download', { release_sha256: 'b'.repeat(64) })).status, 409);
        const download = await call('download', { release_sha256: hash }); assert.equal(download.status, 200);
        assert.equal((await download.arrayBuffer()).byteLength, 4096);
        const ticket = await (await call('ticket', { release_sha256: hash, pid: 1234 })).json();
        const redeem = () => a.device.proof('redeem', ticket.ticket, { release_sha256: hash, pid: 1234 });
        assert.equal((await h.request('/api/v1/launcher/redeem', await a.device.proof('redeem', ticket.ticket, { release_sha256: hash, pid: 4321 }))).status, 403);
        const results = await Promise.all([h.request('/api/v1/launcher/redeem', await redeem()), h.request('/api/v1/launcher/redeem', await redeem())]);
        assert.deepEqual(results.map(r => r.status).sort(), [200, 403]);
        const ticket2 = await (await call('ticket', { release_sha256: hash, pid: 1234 })).json();
        await h.request('/api/v1/admin/ban-key', { id: license.id, status: 'banned' }, { admin: true });
        assert.equal((await h.request('/api/v1/launcher/redeem', await a.device.proof('redeem', ticket2.ticket, { release_sha256: hash, pid: 1234 }))).status, 403);
    } finally { await h.close(); }
});

test('schema validation never rewrites data and rejects raw-key Wolf schemas without migration', async () => {
    const h = await harness();
    try {
        const license = await create(h), active = await activate(h, license.key);
        await h.request('/api/v1/configs/publish', { session_token: active.result.session_token, name: 'Historical config', author_name: 'Player_1', payload: '{"modules":[]}' });
        const tables = ['license_keys', 'public_configs', 'admin_audit_logs', 'auth_logs', 'launcher_tickets'];
        const snapshot = () => tables.map(table => h.env.DB.sqlite.prepare('SELECT * FROM ' + table).all());
        const before = snapshot(); await ensureDatabaseSchema(h.env); await ensureDatabaseSchema(h.env);
        assert.deepEqual(snapshot(), before);
        const old = new LocalD1(false);
        old.sqlite.exec("CREATE TABLE license_keys(id INTEGER PRIMARY KEY,key_code TEXT NOT NULL,hwid TEXT); INSERT INTO license_keys VALUES(1,'HISTORICAL-KEY','old-device'); CREATE TABLE public_configs(id INTEGER PRIMARY KEY,payload TEXT); INSERT INTO public_configs VALUES(1,'historical');");
        await assert.rejects(ensureDatabaseSchema({ DB: old }));
        assert.equal(old.sqlite.prepare('SELECT COUNT(*) AS n FROM license_keys').get().n, 1);
        assert.equal(old.sqlite.prepare('SELECT payload FROM public_configs').get().payload, 'historical'); old.close();
        h.env.DB.sqlite.exec('ALTER TABLE license_keys DROP COLUMN device_key_hash');
        await assert.rejects(ensureDatabaseSchema(h.env));
    } finally { await h.close(); }
});

test('rate limiter and unconfigured services fail closed; malformed admin JSON is rejected', async () => {
    const h = await harness();
    try {
        const invalid = await worker.fetch(new Request('https://local.test/api/v1/admin/create-key', { method: 'POST',
            headers: { ...h.access.header, Origin:'https://local.test', 'Content-Type': 'application/json' }, body: '{broken' }), h.env, h.ctx);
        assert.equal(invalid.status, 400);
        h.env.RATE_LIMITER.get = () => ({ fetch: async () => Response.json({ allowed: false, retryAfterMs: 1000 }) });
        assert.equal((await h.request('/api/v1/admin/list-keys', undefined, { admin: true })).status, 429);
        delete h.env.RATE_LIMITER;
        assert.equal((await h.request('/api/v1/admin/list-keys', undefined, { admin: true })).status, 503);
        assert.equal((await h.request('/health')).status, 503);
        const memory = new Map(), limiter = new RateLimiter({ storage: {
            async get(k) { return memory.get(k); }, async put(k, v) { memory.set(k, v); },
            async transaction(task) { return task(this); }
        } });
        for (let n = 0; n < 3; n++) {
            const result = await limiter.fetch(new Request('https://local.test', { method: 'POST', body: JSON.stringify({ limit: 2, windowMs: 60000 }) }));
            assert.equal((await result.json()).allowed, n < 2);
        }
    } finally { await h.close(); }
});
