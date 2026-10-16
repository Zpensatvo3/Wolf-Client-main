import assert from 'node:assert/strict';
import { test } from 'node:test';
import { createRequire } from 'node:module';
import { execFileSync } from 'node:child_process';
import { mkdtempSync, readFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { fileURLToPath } from 'node:url';
import { join } from 'node:path';
import { harness } from './helpers/auth-harness.mjs';

// Miniflare is already installed by Wrangler. This adds no runtime dependency.
const require = createRequire(new URL('../server/package.json', import.meta.url));
const { Miniflare, convertV4MiniflareOptions } = require('miniflare');
const serverRoot = fileURLToPath(new URL('../server/', import.meta.url));

test('bundled Worker in workerd: D1, SQLite Durable Object concurrency and signed auth', { timeout: 60000 }, async () => {
    const directory = mkdtempSync(join(tmpdir(), 'onestep-workerd-'));
    execFileSync(process.execPath, [join(serverRoot, 'node_modules/wrangler/bin/wrangler.js'), 'deploy', '--dry-run', '--outdir', directory],
        { cwd: serverRoot, env: { ...process.env, WRANGLER_SEND_METRICS: 'false' }, stdio: 'pipe' });
    const h = await harness();
    const bindings = Object.fromEntries(['KEY_PEPPER', 'HWID_PEPPER', 'ADMIN_TOKEN_HASH', 'SIGNING_PRIVATE_KEY_PKCS8', 'SIGNING_PUBLIC_KEY_SPKI', 'ALLOW_DEVICE_CONFIGS'].map(key => [key, h.env[key]]));
    const runtime = new Miniflare(convertV4MiniflareOptions({ rootPath: directory, modulesRoot: directory,
        modules: true, scriptPath: join(directory, 'worker.js'),
        compatibilityDate: '2026-10-16', compatibilityFlags: ['nodejs_compat'], bindings,
        d1Databases: ['DB'], kvNamespaces: ['RELEASES'],
        durableObjects: { RATE_LIMITER: { className: 'RateLimiter', useSQLite: true } } }));
    try {
        const db = await runtime.getD1Database('DB');
        const schema = readFileSync(new URL('../server/schema.sql', import.meta.url), 'utf8').replace(/^--.*$/gm, '');
        await db.batch(schema.split(';').map(sql => sql.trim()).filter(Boolean).map(sql => db.prepare(sql)));
        const request = (path, body, admin = false) => runtime.dispatchFetch('https://local.test' + path, {
            method: body === undefined ? 'GET' : 'POST',
            headers: { ...(admin ? { Authorization: 'Bearer ' + h.admin } : {}), ...(body === undefined ? {} : { 'Content-Type': 'application/json' }) },
            body: body === undefined ? undefined : JSON.stringify(body)
        });
        assert.equal((await request('/health')).status, 200);
        assert.equal((await request('/admin')).status, 200);
        const app = await (await request('/admin/assets/app.js')).text();
        assert.doesNotThrow(() => new Function(app));
        assert.equal((await request('/api/v1/admin/list-keys')).status, 401);
        const created = await (await request('/api/v1/admin/create-key', { days: '7' }, true)).json();
        assert.equal(created.success, true);
        const device = await h.device();
        const activation = await request('/api/v1/auth', await device.proof('activate', created.key));
        assert.equal(activation.status, 200); const session = await activation.json();
        const heartbeat = () => device.proof('heartbeat', session.session_token);
        assert.equal((await request('/api/v1/auth', await heartbeat())).status, 200);
        assert.equal((await request('/api/v1/configs/list', { session_token: session.session_token })).status, 200);
        assert.equal((await request('/api/v1/admin/reset-hwid', { id: created.id }, true)).status, 200);
        assert.equal((await request('/api/v1/auth', await heartbeat())).status, 403);
        const namespace = await runtime.getDurableObjectNamespace('RATE_LIMITER');
        const limiter = namespace.get(namespace.idFromName('concurrent-test'));
        const responses = await Promise.all(Array.from({ length: 10 }, () => limiter.fetch('https://limiter.test', {
            method: 'POST', body: JSON.stringify({ limit: 5, windowMs: 60000 })
        }).then(response => response.json())));
        assert.equal(responses.filter(response => response.allowed).length, 5);
    } finally { await runtime.dispose(); await h.close(); }
});
