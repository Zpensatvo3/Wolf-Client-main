import { test } from 'node:test';
import assert from 'node:assert/strict';
import { validRelease, validPid, handleLauncher, MAX_RELEASE_BYTES } from '../server/launcher.mjs';

test('release fields reject paths, unsupported size and malformed hashes', () => {
    const good = { version: '2026.10.16-1', sha256: 'a'.repeat(64), bytes: 20000000 };
    assert.ok(validRelease(good));
    for (const change of [{ version: '../bad' }, { sha256: 'x'.repeat(64) }, { bytes: 0 },
        { bytes: MAX_RELEASE_BYTES + 1 }, { bytes: 4096.5 }]) assert.ok(!validRelease({ ...good, ...change }));
    for (const pid of [0, -1, 1.5, 2 ** 32, '123']) assert.ok(!validPid(pid));
    assert.ok(validPid(123));
});
test('launcher routes fail closed and do not intercept existing auth/config routes', async () => {
    const s = { json: (body, status = 200) => Response.json(body, { status }), rate: async () => null,
        admin: async () => false, read: async request => request.json() };
    const base = 'https://example.test';
    assert.equal(await handleLauncher(new Request(base + '/api/v1/auth'), {}, {}, s), null);
    assert.equal((await handleLauncher(new Request(base + '/api/v1/launcher/manifest'), {}, {}, s)).status, 503);
    const env = { RELEASES: {} };
    assert.equal((await handleLauncher(new Request(base + '/api/v1/admin/release', { method: 'PUT' }), env, {}, s)).status, 401);
    assert.equal((await handleLauncher(new Request(base + '/api/v1/launcher/download'), env, {}, s)).status, 405);
    const post = new Request(base + '/api/v1/launcher/download', { method: 'POST', body: JSON.stringify({ timestamp: 1 }) });
    assert.equal((await handleLauncher(post, env, {}, s)).status, 408);
});
