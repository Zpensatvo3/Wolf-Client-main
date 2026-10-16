import assert from 'node:assert/strict';
import { test } from 'node:test';
import { dashboardResponse } from '../server/dashboard/index.mjs';
import { messages } from '../server/dashboard/i18n.mjs';

test('dashboard documents and bundled assets use restrictive same-origin security headers', async () => {
    for (const path of ['/', '/admin', '/admin/', '/admin/assets/styles.css', '/admin/assets/app.js']) {
        const response = dashboardResponse(new Request('https://local.test' + path));
        assert.equal(response.status, 200);
        assert.equal(response.headers.get('Cache-Control'), 'no-store');
        assert.equal(response.headers.get('X-Frame-Options'), 'DENY');
        assert.match(response.headers.get('Content-Security-Policy'), /script-src 'self'/);
        assert.ok(!response.headers.get('Content-Security-Policy').includes('unsafe-inline'));
        const head = dashboardResponse(new Request('https://local.test' + path, { method: 'HEAD' }));
        assert.equal((await head.text()).length, 0);
        assert.equal(dashboardResponse(new Request('https://local.test' + path, { method: 'POST' })).status, 405);
    }
    assert.equal(dashboardResponse(new Request('https://local.test/api/v1/auth')), null);
    const html = await dashboardResponse(new Request('https://local.test/admin?token=must-not-echo')).text();
    assert.ok(!html.includes('must-not-echo'));
    assert.ok(!html.includes('fonts.googleapis.com'));
    assert.match(html, /type="password" autocomplete="off"/);
    assert.match(html, /aria-describedby="confirm-message"/);
});

test('English and Turkish dashboard dictionaries cover the same interface', () => {
    assert.deepEqual(Object.keys(messages.tr).sort(), Object.keys(messages.en).sort());
    for (const locale of Object.values(messages)) for (const value of Object.values(locale)) assert.ok(typeof value === 'string' && value.length > 0);
});
