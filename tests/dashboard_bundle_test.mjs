import assert from 'node:assert/strict';
import { test } from 'node:test';
import { execFileSync } from 'node:child_process';
import { mkdtempSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { runInNewContext } from 'node:vm';
import {accessFixture} from './helpers/access-fixture.mjs';

const serverRoot = fileURLToPath(new URL('../server/', import.meta.url));

for (const minify of [false, true]) {
    test(`deployed dashboard bootstrap has no missing bundler helpers (minify=${minify})`, { timeout: 30000 }, async () => {
        const directory = mkdtempSync(join(tmpdir(), 'onestep-dashboard-bundle-'));
        execFileSync(process.execPath, [join(serverRoot, 'node_modules/wrangler/bin/wrangler.js'),
            'deploy', '--dry-run', '--outdir', directory, ...(minify ? ['--minify'] : [])],
            { cwd: serverRoot, env: { ...process.env, WRANGLER_SEND_METRICS: 'false' }, stdio: 'pipe' });
        const { default: worker, RateLimiter } = await import(pathToFileURL(join(directory, 'worker.js')).href);
        assert.equal(typeof RateLimiter, 'function'); // Named DO export remains intact.
        const access=await accessFixture();
        const response = await worker.fetch(new Request('https://local.test/admin/assets/app.js',{headers:access.header}), access.env, {});
        assert.equal(response.status, 200);
        const script = await response.text();
        const reachedDocument = new Error('Browser document reached');
        // A healthy bootstrap reaches the DOM. The broken bundle throws an
        // earlier ReferenceError for __name / its minified alias instead.
        assert.throws(() => runInNewContext(script, {
            document: { getElementById() { throw reachedDocument; } }
        }, { timeout: 1000 }), error => error === reachedDocument);
    });
}
