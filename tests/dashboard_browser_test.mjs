import assert from 'node:assert/strict';
import { test } from 'node:test';
import { createServer } from 'node:http';
import { spawn } from 'node:child_process';
import { existsSync, mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { harness } from './helpers/auth-harness.mjs';
const { default: worker } = await import(process.env.ONESTEP_WORKER_BUNDLE
    ? pathToFileURL(process.env.ONESTEP_WORKER_BUNDLE).href : '../server/worker.js');
import { sha256 } from '../server/core/crypto.mjs';

const chromium = process.env.CHROMIUM || '/usr/bin/chromium';
const pause = ms => new Promise(resolve => setTimeout(resolve, ms));

async function browser(directory) {
    const child = spawn(chromium, ['--headless', '--no-sandbox', '--disable-gpu', '--disable-dev-shm-usage',
        '--no-first-run', '--no-default-browser-check', '--remote-debugging-port=0', '--user-data-dir=' + join(directory, 'profile'), 'about:blank'], { stdio: ['ignore', 'ignore', 'pipe'] });
    const endpoint = await new Promise((resolve, reject) => {
        let stderr = '';
        const timer = setTimeout(() => { child.kill(); reject(new Error('Chromium startup timed out')); }, 15000);
        child.stderr.on('data', chunk => {
            stderr = (stderr + chunk).slice(-20000);
            const match = stderr.match(/DevTools listening on (ws:\/\/[^\s]+)/);
            if (match) { clearTimeout(timer); resolve(match[1]); }
        });
        child.on('error', error => { clearTimeout(timer); reject(error); });
    });
    const socket = new WebSocket(endpoint);
    await new Promise((resolve, reject) => { socket.onopen = resolve; socket.onerror = reject; });
    let nextId = 1, session;
    const pending = new Map();
    socket.onmessage = event => {
        const data = JSON.parse(event.data), item = pending.get(data.id);
        if (item) { pending.delete(data.id); clearTimeout(item.timer); data.error ? item.reject(new Error(data.error.message)) : item.resolve(data.result); }
    };
    function command(method, params = {}, root = false) {
        const id = nextId++;
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => { pending.delete(id); reject(new Error('Browser command timed out: ' + method)); }, 15000);
            pending.set(id, { resolve, reject, timer });
            socket.send(JSON.stringify({ id, method, params, ...(!root && session ? { sessionId: session } : {}) }));
        });
    }
    const target = await command('Target.createTarget', { url: 'about:blank' }, true);
    session = (await command('Target.attachToTarget', { targetId: target.targetId, flatten: true }, true)).sessionId;
    await command('Page.enable'); await command('Runtime.enable');
    async function evaluate(expression) {
        const value = await command('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
        if (value.exceptionDetails) throw new Error('Browser script evaluation failed: ' +
            (value.exceptionDetails.exception?.description || value.exceptionDetails.text).split('\n')[0]);
        return value.result.value;
    }
    async function wait(expression) {
        for (let n = 0; n < 240; n++) { if (await evaluate(expression)) return; await pause(25); }
        throw new Error('Browser condition timed out: ' + expression);
    }
    return { command, evaluate, wait, close() { socket.close(); child.kill(); } };
}

test('Chromium: login, real API pages, filters, confirmations, key clearing, XSS, mobile and logout',
    { timeout: 120000, skip: !existsSync(chromium) ? 'Set CHROMIUM to run the browser integration test' : false }, async () => {
    const h = await harness(), requests = [], directory = mkdtempSync(join(tmpdir(), 'onestep-dashboard-'));
    let client, failBan = false;
    const server = createServer(async (incoming, outgoing) => {
        try {
            if (incoming.url==='/cdn-cgi/access/logout') {
                outgoing.writeHead(200,{'Content-Type':'text/html'}); outgoing.end('<body data-logged-out="true"></body>'); return;
            }
            const chunks = []; for await (const chunk of incoming) chunks.push(chunk);
            requests.push({ method: incoming.method, path: incoming.url.split('?')[0] });
            if (failBan && incoming.url === '/api/v1/admin/ban-key') {
                await pause(150); outgoing.writeHead(500, { 'Content-Type': 'application/json' }); outgoing.end('{"success":false}'); return;
            }
            const body = chunks.length ? Buffer.concat(chunks) : undefined;
            const request = new Request('http://127.0.0.1:' + server.address().port + incoming.url,
                { method: incoming.method, headers: {...incoming.headers,...h.access.header}, body });
            const response = await worker.fetch(request, h.env, h.ctx); await h.drain();
            outgoing.writeHead(response.status, Object.fromEntries(response.headers)); outgoing.end(Buffer.from(await response.arrayBuffer()));
        } catch (_) { outgoing.writeHead(500); outgoing.end(); }
    });
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    try {
        for (let n = 0; n < 26; n++) h.env.DB.sqlite.prepare("INSERT INTO license_keys(key_hash,status) VALUES(?,'unused')").run(await sha256('browser-fixture-' + n));
        const license = await (await h.request('/api/v1/admin/create-key', {}, { admin: true })).json();
        h.env.DB.sqlite.prepare("INSERT INTO admin_audit_logs(admin_id,action,reason) VALUES('dashboard','TEST',?)")
            .run('<img src=x onerror="window.xssExecuted=true">');
        client = await browser(directory);
        const { command, evaluate: ev, wait } = client;
        const click = selector => ev(`document.querySelector(${JSON.stringify(selector)}).click()`);
        const value = (selector, text) => ev(`document.querySelector(${JSON.stringify(selector)}).value=${JSON.stringify(text)};void 0`);
        await command('Emulation.setDeviceMetricsOverride', { width: 1440, height: 1000, deviceScaleFactor: 1, mobile: false });
        await command('Page.navigate', { url: 'http://127.0.0.1:' + server.address().port + '/admin' });
        await wait('!!document.getElementById("app") && !document.getElementById("app").hidden && !document.body.hasAttribute("aria-busy")');
        assert.equal(await ev('document.getElementById("stat-total").textContent'), '27');
        assert.equal(await ev('localStorage.length + sessionStorage.length'), 0);
        assert.equal(await ev('document.getElementById("admin-token")'), null);
        const shot = await command('Page.captureScreenshot', { format: 'png' }); writeFileSync(join(directory, 'desktop.png'), Buffer.from(shot.data, 'base64'));
        await click('[data-page="licenses"]'); await wait('document.querySelectorAll("#licenses-body tr").length===20 && !document.body.hasAttribute("aria-busy")');
        await click('#licenses-next'); await wait('document.querySelectorAll("#licenses-body tr").length===7 && !document.body.hasAttribute("aria-busy")');
        const keyHash = h.env.DB.sqlite.prepare('SELECT key_hash FROM license_keys WHERE id=?').get(license.id).key_hash;
        await value('#search', keyHash); await ev('document.getElementById("search").dispatchEvent(new Event("input"))');
        await wait('document.querySelectorAll("#licenses-body tr").length===1 && !document.body.hasAttribute("aria-busy")');
        await click('.license-ref'); await wait('document.getElementById("detail-dialog").open');
        assert.equal(await ev('!!document.querySelector("[data-action=reveal]")'),false);
        assert.equal(requests.filter(r => r.path.endsWith('/reveal-key')).length, 0);
        assert.equal(await ev('document.getElementById("revealed-key").hidden'),true);
        failBan = true;
        await click('[data-action="ban"]'); await click('#confirm-accept');
        assert.equal(h.env.DB.sqlite.prepare('SELECT status FROM license_keys WHERE id=?').get(license.id).status, 'unused');
        await wait('!!document.querySelector(".error-toast") && !document.body.hasAttribute("aria-busy")');
        assert.equal(h.env.DB.sqlite.prepare('SELECT status FROM license_keys WHERE id=?').get(license.id).status, 'unused');
        assert.equal(await ev('document.getElementById("detail-dialog").open'), true);
        failBan = false;
        await click('[data-action="ban"]'); await click('#confirm-accept');
        await wait('!document.getElementById("detail-dialog").open && !document.body.hasAttribute("aria-busy")');
        assert.equal(await ev('document.getElementById("revealed-value").textContent.length'), 0);
        assert.equal(h.env.DB.sqlite.prepare('SELECT status FROM license_keys WHERE id=?').get(license.id).status, 'banned');
        await click('.license-ref'); await click('[data-action="unban"]'); await click('#confirm-accept');
        await wait('!document.getElementById("detail-dialog").open && !document.body.hasAttribute("aria-busy")');
        assert.equal(h.env.DB.sqlite.prepare('SELECT status FROM license_keys WHERE id=?').get(license.id).status, 'unused');
        await click('.license-ref'); await click('[data-action="reset"]'); await click('#confirm-accept');
        await wait('!document.getElementById("detail-dialog").open && !document.body.hasAttribute("aria-busy")');
        assert.equal(h.env.DB.sqlite.prepare('SELECT license_epoch FROM license_keys WHERE id=?').get(license.id).license_epoch, 4);
        await click('.license-ref'); await click('[data-action="delete"]'); await click('#confirm-cancel');
        assert.equal(h.env.DB.sqlite.prepare('SELECT COUNT(*) AS n FROM license_keys').get().n, 27);
        await click('[data-action="delete"]'); await click('#confirm-accept');
        await wait('!document.getElementById("detail-dialog").open && !document.body.hasAttribute("aria-busy")');
        assert.equal(h.env.DB.sqlite.prepare('SELECT COUNT(*) AS n FROM license_keys').get().n, 26);
        assert.equal(await ev('document.getElementById("licenses-empty").hidden'), false);
        await click('[data-page="overview"]'); await wait('!document.body.hasAttribute("aria-busy")');
        await click('#new-license'); await value('#duration', '7');
        await click('#create-form button[type="submit"]');
        await wait('document.getElementById("detail-dialog").open && !document.body.hasAttribute("aria-busy")');
        assert.equal(h.env.DB.sqlite.prepare('SELECT COUNT(*) AS n FROM license_keys').get().n, 27);
        assert.equal(await ev('/^ONESTEP-(?:[A-F0-9]{8}-){3}[A-F0-9]{8}$/.test(document.getElementById("revealed-value").textContent)'), true);
        await click('[data-close="detail-dialog"]'); await wait('!document.getElementById("detail-dialog").open');
        assert.equal(await ev('document.getElementById("revealed-value").textContent.length'), 0);
        await click('[data-page="activity"]'); await wait('!document.body.hasAttribute("aria-busy") && !!document.querySelector("#activity-body tr")');
        assert.equal(await ev('document.getElementById("activity-body").textContent.includes("<img")'), true);
        assert.equal(await ev('!!window.xssExecuted || !!document.querySelector("#activity-body img")'), false);
        await click('[data-page="system"]'); await wait('document.querySelectorAll(".system-card").length===6 && !document.body.hasAttribute("aria-busy")');
        await click('[data-page="settings"]'); await wait('!document.body.hasAttribute("aria-busy")');
        await value('#language', 'en'); await ev('document.getElementById("language").dispatchEvent(new Event("change"))');
        assert.equal(await ev('document.documentElement.lang'), 'en');
        await click('[data-page="overview"]'); await wait('!document.body.hasAttribute("aria-busy")');
        await command('Emulation.setDeviceMetricsOverride', { width: 390, height: 844, deviceScaleFactor: 1, mobile: true });
        await command('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-reduced-motion', value: 'reduce' }] });
        await pause(100);
        assert.equal(await ev('document.documentElement.scrollWidth <= 390'), true);
        assert.equal(await ev('getComputedStyle(document.querySelector(".stat")).transitionDuration'), '0s');
        await click('#mobile-menu'); assert.equal(await ev('document.getElementById("mobile-menu").getAttribute("aria-expanded")'), 'true');
        await command('Input.dispatchKeyEvent', { type: 'keyDown', key: 'Escape', code: 'Escape', windowsVirtualKeyCode: 27 });
        await wait('document.getElementById("mobile-menu").getAttribute("aria-expanded")==="false"');
        const mobileShot = await command('Page.captureScreenshot', { format: 'png' }); writeFileSync(join(directory, 'mobile.png'), Buffer.from(mobileShot.data, 'base64'));
        await ev('document.getElementById("logout").click()'); await wait('document.body.dataset.loggedOut==="true"');
        assert.equal(await ev('localStorage.length + sessionStorage.length'), 0);
        console.log('Dashboard screenshots:', join(directory, 'desktop.png'), join(directory, 'mobile.png'));
    } finally {
        client?.close(); await new Promise(resolve => server.close(resolve)); await h.close();
    }
});
