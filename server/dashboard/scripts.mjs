// Serialized as a same-origin module asset. No browser code runs in the Worker.
export function dashboardApp(messages) {
    'use strict';
    const $ = id => document.getElementById(id);
    let locale = 'tr', token = '', generation = 0, page = 'overview', busy = false;
    let licenses = [], stats = null, recent = [], activity = [], system = null;
    let licensePage = 1, activityPage = 1, licenseTotal = 0, activityTotal = 0;
    let selected = null, confirmResolve = null, searchTimer;
    const controllers = new Set();
    const t = key => messages[locale][key] || messages.tr[key] || key;
    const el = (tag, className, text) => {
        const node = document.createElement(tag);
        if (className) node.className = className;
        if (text !== undefined) node.textContent = String(text);
        return node;
    };
    const short = value => value ? String(value).slice(0, 8) + '…' + String(value).slice(-6) : '—';
    function date(value, expiry = false) {
        if (!value || value === 'lifetime') return expiry ? t('lifetime') : '—';
        const iso = /^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/.test(value) ? value.replace(' ', 'T') + 'Z' : value;
        const parsed = new Date(iso);
        return Number.isFinite(parsed.getTime()) ? parsed.toLocaleString(locale === 'tr' ? 'tr-TR' : 'en-GB',
            { year: 'numeric', month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit' }) : t('unknown');
    }
    function badge(value) {
        const status = ['active', 'unused', 'banned', 'expired'].includes(value) ? value : 'unknown';
        return el('span', 'badge ' + status, t(status));
    }
    function toast(text, error = false) {
        const item = el('div', 'toast' + (error ? ' error-toast' : ''), text);
        $('toasts').append(item);
        while ($('toasts').children.length > 3) $('toasts').firstChild.remove();
        setTimeout(() => item.remove(), 5000);
    }
    function connection(value) {
        $('connection').dataset.state = value;
        $('connection').textContent = t(value === 'ok' ? 'online' : value === 'error' ? 'offline' : 'checking');
    }
    async function api(route, body) {
        const current = generation, controller = new AbortController();
        controllers.add(controller);
        const timeout = setTimeout(() => controller.abort(), 15000);
        try {
            const response = await fetch('/api/v1/admin/' + route, {
                method: body === undefined ? 'GET' : 'POST', cache: 'no-store', credentials: 'omit',
                headers: { Authorization: 'Bearer ' + token, ...(body === undefined ? {} : { 'Content-Type': 'application/json' }) },
                body: body === undefined ? undefined : JSON.stringify(body), signal: controller.signal
            });
            const data = await response.json();
            if (current !== generation) throw new Error(t('unauthorized'));
            if (!response.ok || data.success === false) {
                const key = ({ 400: 'invalid', 401: 'unauthorized', 404: 'notFound', 409: route === 'reveal-key' ? 'revealUnavailable' : 'conflict', 429: 'rateLimited', 500: 'unavailable', 503: 'unavailable' })[response.status] || 'apiFailure';
                if (response.status === 401 && !$('app').hidden) {
                    logout(); $('login-error').textContent = t('unauthorized'); $('login-error').hidden = false;
                }
                throw new Error(t(key));
            }
            connection('ok');
            return data;
        } catch (error) {
            if (current === generation) connection('error');
            if (error.name === 'AbortError' || error instanceof TypeError) throw new Error(t('network'));
            throw error;
        } finally {
            clearTimeout(timeout);
            controllers.delete(controller);
        }
    }
    async function run(task) {
        if (busy) return;
        busy = true;
        const buttons = [...document.querySelectorAll('button')].map(button => [button, button.disabled]);
        buttons.forEach(([button]) => { button.disabled = true; });
        document.body.setAttribute('aria-busy', 'true');
        try { await task(); }
        catch (error) { if (token) toast(error.message || t('error'), true); }
        finally {
            busy = false;
            buttons.forEach(([button, disabled]) => { if (button.isConnected) button.disabled = disabled; });
            document.body.removeAttribute('aria-busy');
            updatePager('licenses', licensePage, licenseTotal);
            updatePager('activity', activityPage, activityTotal);
        }
    }
    function clearRevealed() {
        $('revealed-value').textContent = '';
        $('revealed-key').hidden = true;
    }
    function mobile(open) {
        $('sidebar').classList.toggle('open', open);
        $('mobile-shade').classList.toggle('open', open);
        $('mobile-menu').setAttribute('aria-expanded', String(open));
        // Prevent keyboard access to an offscreen mobile navigation panel.
        $('sidebar').inert = window.matchMedia('(max-width:760px)').matches && !open;
        if (open) $('mobile-close').focus();
    }
    function logout() {
        generation++;
        token = '';
        controllers.forEach(controller => controller.abort());
        clearTimeout(searchTimer);
        licenses = []; activity = []; recent = []; stats = null; system = null; selected = null;
        licenseTotal = activityTotal = 0; licensePage = activityPage = 1;
        $('admin-token').value = ''; $('custom-key').value = '';
        $('search').value = ''; $('status-filter').value = 'all'; $('sort').value = 'newest';
        for (const dialog of document.querySelectorAll('dialog[open]')) dialog.close();
        clearRevealed();
        ['detail-content', 'detail-actions', 'licenses-body', 'activity-body', 'recent-activity', 'distribution', 'system-cards', 'toasts'].forEach(id => $(id).replaceChildren());
        document.querySelectorAll('.stat-number').forEach(node => { node.textContent = '—'; });
        $('app').hidden = true; $('login').hidden = false;
        $('page-error').hidden = true; $('last-updated').textContent = '—';
        mobile(false);
        $('admin-token').focus();
    }
    function translate() {
        $('toasts').replaceChildren();
        document.documentElement.lang = locale;
        document.title = 'OneStep — ' + t('admin');
        document.querySelectorAll('[data-i18n]').forEach(node => { node.textContent = t(node.dataset.i18n); });
        $('search').placeholder = t('search'); $('search').setAttribute('aria-label', t('search'));
        $('status-filter').setAttribute('aria-label', t('status'));
        document.querySelectorAll('[data-close], #mobile-shade, #mobile-close').forEach(node => node.setAttribute('aria-label', t('close')));
        displayPage(page, false);
        if (stats) renderOverview();
        renderLicenses(); renderActivity(); if (system) renderSystem();
    }
    function displayPage(next, focus = true) {
        page = next;
        document.querySelectorAll('[data-view]').forEach(node => { node.hidden = node.dataset.view !== page; });
        document.querySelectorAll('.nav[data-page]').forEach(node => {
            if (node.dataset.page === page) node.setAttribute('aria-current', 'page');
            else node.removeAttribute('aria-current');
        });
        $('page-title').textContent = t(page);
        $('breadcrumb').textContent = t(page);
        $('page-description').textContent = t(page + 'Text');
        $('refresh').hidden = page === 'settings';
        $('new-license').hidden = !['overview', 'licenses'].includes(page);
        $('page-error').hidden = true;
        mobile(false);
        if (focus) $('page-title').focus();
    }
    function renderOverview() {
        for (const key of ['total', 'active', 'unused', 'banned', 'expired']) $('stat-' + key).textContent = String(stats[key]);
        $('recent-activity').replaceChildren();
        $('recent-empty').hidden = recent.length !== 0;
        for (const row of recent) {
            const item = el('div', 'feed-item');
            item.append(el('span', 'feed-symbol', row.action === 'CREATE_KEY' ? '+' : '·'));
            const text = el('div', 'feed-copy');
            text.append(el('strong', '', t(row.action)), el('p', 'mono', short(row.key_hash)));
            item.append(text, el('time', '', date(row.timestamp)));
            $('recent-activity').append(item);
        }
        $('distribution').replaceChildren();
        for (const key of ['active', 'unused', 'banned', 'expired']) {
            const row = el('div', 'distribution-item'), label = el('span');
            label.append(el('i', 'dot ' + key), document.createTextNode(t(key)));
            row.append(label, el('strong', '', stats[key]));
            $('distribution').append(row);
        }
    }
    function updatePager(prefix, number, total) {
        const pages = Math.max(1, Math.ceil(total / 20));
        $(prefix + '-summary').textContent = total + ' ' + t('records');
        $(prefix + '-page').textContent = t('page') + ' ' + number + ' / ' + pages;
        $(prefix + '-prev').disabled = busy || number <= 1;
        $(prefix + '-next').disabled = busy || number >= pages;
    }
    function renderLicenses() {
        $('licenses-body').replaceChildren();
        $('licenses-empty').hidden = licenses.length !== 0;
        for (const row of licenses) {
            const tr = el('tr');
            const cell = key => { const td = el('td'); td.dataset.label = t(key); tr.append(td); return td; };
            const ref = el('button', 'license-ref', 'License #' + row.id);
            ref.addEventListener('click', () => openDetails(row));
            cell('license').append(ref, el('small', 'mono', short(row.key_hash)));
            cell('status').append(badge(row.effective_status || row.status));
            cell('device').append(document.createTextNode(t(row.hwid_bound ? 'bound' : 'unbound')), el('small', '', t('deviceKey') + ': ' + t(row.device_bound ? 'bound' : 'unbound')));
            cell('expiration').textContent = date(row.expires_at, true);
            cell('epoch').append(el('span', 'mono', row.license_epoch));
            const more = el('button', 'row-more', '···');
            more.setAttribute('aria-label', t('details') + ' #' + row.id);
            more.addEventListener('click', () => openDetails(row)); cell('details').append(more);
            $('licenses-body').append(tr);
        }
        updatePager('licenses', licensePage, licenseTotal);
    }
    function renderActivity() {
        $('activity-body').replaceChildren(); $('activity-empty').hidden = activity.length !== 0;
        for (const row of activity) {
            const tr = el('tr');
            for (const [value, className] of [[t(row.action), ''], [short(row.key_hash), 'mono'],
                [row.reason || '—', 'muted'], [String(row.previous_epoch ?? '—') + ' → ' + String(row.new_epoch ?? '—'), 'mono'], [date(row.timestamp), 'muted']])
                tr.append(el('td', className, value));
            $('activity-body').append(tr);
        }
        updatePager('activity', activityPage, activityTotal);
    }
    function renderSystem() {
        $('system-cards').replaceChildren();
        const rows = [['database', system.database === 'ok' ? t('online') : t('offline')],
            ['authConfig', t(system.auth_configured ? 'configured' : 'missing')],
            ['rateLimit', t(system.rate_limiter_configured ? 'configured' : 'missing')],
            ['releases', t(system.releases_configured ? 'configured' : 'missing')],
            ['release', system.release?.version || t('noRelease')], ['checked', date(system.checked_at)]];
        for (const [label, value] of rows) {
            const card = el('article', 'system-card'); card.append(el('h2', '', t(label)), el('p', '', value)); $('system-cards').append(card);
        }
    }
    function skeleton(id, table = false) {
        $(id).replaceChildren();
        for (let i = 0; i < 4; i++) {
            const shape = el('div', 'skeleton' + (table ? '' : ' wide')); shape.setAttribute('aria-hidden', 'true');
            if (table) { const tr = el('tr'), td = el('td'); td.colSpan = 6; td.append(shape); tr.append(td); $(id).append(tr); }
            else $(id).append(shape);
        }
    }
    async function loadView() {
        $('page-error').hidden = true;
        const current = generation;
        try {
            if (page === 'overview') {
                skeleton('recent-activity');
                const data = await api('overview'); stats = data.stats; recent = data.activity; renderOverview();
            } else if (page === 'licenses') {
                $('licenses-empty').hidden = true; skeleton('licenses-body', true);
                const query = new URLSearchParams({ page: licensePage, page_size: 20, search: $('search').value.trim(), status: $('status-filter').value, sort: $('sort').value });
                const data = await api('list-keys?' + query); licenses = data.keys; licenseTotal = data.total;
                if (licensePage > 1 && licenses.length === 0) { licensePage = Math.max(1, Math.ceil(licenseTotal / 20)); return await loadView(); }
                renderLicenses();
            } else if (page === 'activity') {
                $('activity-empty').hidden = true; skeleton('activity-body', true);
                const data = await api('activity?page=' + activityPage + '&page_size=20'); activity = data.activity; activityTotal = data.total; renderActivity();
            } else if (page === 'system') {
                skeleton('system-cards'); system = await api('status'); renderSystem();
            }
            if (current === generation && token) $('last-updated').textContent = t('checked') + ' · ' + date(new Date().toISOString());
        } catch (error) {
            if (current === generation && token) {
                document.querySelectorAll('.skeleton').forEach(node => node.remove());
                $('page-error').textContent = error.message || t('loadError'); $('page-error').hidden = false;
            }
            throw error;
        }
    }
    async function navigate(next) {
        if (busy || !token) return;
        clearTimeout(searchTimer);
        displayPage(next);
        await run(loadView);
    }
    function openDetails(row) {
        if (busy) return;
        selected = row; clearRevealed();
        $('detail-title').textContent = t('license') + ' #' + row.id;
        $('detail-content').replaceChildren(badge(row.effective_status || row.status));
        const dl = el('dl', 'details');
        for (const [label, value] of [['fingerprint', row.key_hash || '—'], ['device', row.hwid_hint || t('unbound')],
            ['deviceKey', t(row.device_bound ? 'bound' : 'unbound')], ['epoch', row.license_epoch], ['expiration', date(row.expires_at, true)],
            ['created', date(row.created_at)], ['activated', date(row.activated_at)]]) {
            const item = el('div'); item.append(el('dt', '', t(label)), el('dd', label === 'fingerprint' ? 'mono' : '', value)); dl.append(item);
        }
        $('detail-content').append(dl); $('detail-actions').replaceChildren();
        for (const action of ['reveal', 'reset', row.status === 'banned' ? 'unban' : 'ban', 'delete']) {
            const button = el('button', action === 'delete' || action === 'ban' ? 'danger' : '', t(action));
            button.dataset.action = action; button.addEventListener('click', () => licenseAction(action)); $('detail-actions').append(button);
        }
        const copyHash = el('button', 'ghost', t('copy') + ' · ' + t('fingerprint'));
        copyHash.addEventListener('click', () => copy(row.key_hash)); $('detail-actions').append(copyHash);
        $('detail-dialog').showModal();
    }
    function confirm(action) {
        if (busy || confirmResolve) return Promise.resolve(false);
        $('confirm-title').textContent = t(action); $('confirm-message').textContent = t(action + 'Warning');
        return new Promise(resolve => { confirmResolve = resolve; $('confirm-dialog').showModal(); });
    }
    function resolveConfirm(accepted) {
        const resolve = confirmResolve; confirmResolve = null; $('confirm-dialog').close();
        if (resolve) resolve(accepted);
    }
    async function copy(value) {
        try { await navigator.clipboard.writeText(String(value)); toast(t('copied')); }
        catch (_) { toast(t('copyError'), true); }
    }
    async function licenseAction(action) {
        const row = selected, current = generation;
        if (!row || !await confirm(action) || current !== generation) return;
        await run(async () => {
            const routes = { reveal: 'reveal-key', reset: 'reset-hwid', ban: 'ban-key', unban: 'ban-key', delete: 'delete-key' };
            const data = await api(routes[action], { id: row.id, ...(['ban', 'unban'].includes(action) ? { status: action === 'ban' ? 'banned' : 'active' } : {}) });
            if (action === 'reveal') {
                $('revealed-value').textContent = data.key; $('revealed-key').hidden = false;
                $('copy-revealed').focus();
            } else {
                $('detail-dialog').close(); selected = null; await loadView();
            }
            toast(t('success'));
        });
        if (action === 'reveal' && token && $('detail-dialog').open && !$('revealed-key').hidden)
            $('copy-revealed').focus();
    }
    $('login-form').addEventListener('submit', event => {
        event.preventDefault();
        if (busy) return;
        const value = $('admin-token').value.trim(); $('admin-token').value = '';
        if (value.length < 32 || value.length > 512) { $('login-error').textContent = t('tokenRequired'); $('login-error').hidden = false; return; }
        run(async () => {
            generation++; token = value; $('login-error').hidden = true;
            $('sign-in').textContent = t('verifying');
            try {
                const data = await api('overview'); stats = data.stats; recent = data.activity;
                $('login').hidden = true; $('app').hidden = false; displayPage('overview'); renderOverview();
                $('last-updated').textContent = t('checked') + ' · ' + date(data.as_of);
            } catch (error) {
                token = ''; $('login-error').textContent = error.message || t('loginError'); $('login-error').hidden = false;
            } finally { $('sign-in').textContent = t('signIn'); }
        });
    });
    $('create-form').addEventListener('submit', event => {
        event.preventDefault();
        run(async () => {
            const data = await api('create-key', { key: $('custom-key').value.trim(), days: $('duration').value });
            $('custom-key').value = ''; $('create-dialog').close(); await loadView();
            selected = null; clearRevealed(); $('detail-actions').replaceChildren();
            $('detail-title').textContent = t('licenseCreated') + ' #' + data.id;
            $('detail-content').replaceChildren(el('p', 'muted small', date(data.expires_at, true)));
            $('revealed-value').textContent = data.key; $('revealed-key').hidden = false; $('detail-dialog').showModal();
            toast(t('licenseCreated'));
        });
    });
    $('logout').addEventListener('click', logout); $('settings-logout').addEventListener('click', logout);
    $('refresh').addEventListener('click', () => run(loadView));
    document.querySelectorAll('[data-page]').forEach(node => node.addEventListener('click', () => navigate(node.dataset.page)));
    document.querySelectorAll('[data-status]').forEach(node => node.addEventListener('click', () => { if (busy) return; licensePage = 1; $('status-filter').value = node.dataset.status; navigate('licenses'); }));
    $('new-license').addEventListener('click', () => { if (!busy) $('create-dialog').showModal(); });
    document.querySelectorAll('[data-close]').forEach(node => node.addEventListener('click', () => {
        if (!busy) {
            if (node.dataset.close === 'detail-dialog') clearRevealed();
            $(node.dataset.close).close();
        }
    }));
    $('detail-dialog').addEventListener('close', () => { selected = null; clearRevealed(); $('detail-content').replaceChildren(); $('detail-actions').replaceChildren(); });
    $('create-dialog').addEventListener('close', () => { $('custom-key').value = ''; });
    document.querySelectorAll('dialog').forEach(dialog => dialog.addEventListener('cancel', event => { if (busy) event.preventDefault(); }));
    $('confirm-cancel').addEventListener('click', () => resolveConfirm(false));
    $('confirm-accept').addEventListener('click', () => resolveConfirm(true));
    $('confirm-dialog').addEventListener('close', () => { if (confirmResolve) { const resolve = confirmResolve; confirmResolve = null; resolve(false); } });
    $('copy-revealed').addEventListener('click', () => copy($('revealed-value').textContent));
    $('search').addEventListener('input', () => {
        clearTimeout(searchTimer); searchTimer = setTimeout(() => { if (!busy && token && page === 'licenses') { licensePage = 1; run(loadView); } }, 450);
    });
    for (const id of ['status-filter', 'sort']) $(id).addEventListener('change', () => { licensePage = 1; run(loadView); });
    for (const prefix of ['licenses', 'activity']) for (const [direction, step] of [['prev', -1], ['next', 1]]) {
        $(prefix + '-' + direction).addEventListener('click', () => {
            if (busy) return;
            if (prefix === 'licenses') licensePage += step; else activityPage += step;
            run(loadView);
        });
    }
    $('language').addEventListener('change', () => { locale = $('language').value === 'en' ? 'en' : 'tr'; translate(); });
    $('mobile-menu').addEventListener('click', () => mobile(!$('sidebar').classList.contains('open')));
    $('mobile-close').addEventListener('click', () => { mobile(false); $('mobile-menu').focus(); });
    $('mobile-shade').addEventListener('click', () => { mobile(false); $('mobile-menu').focus(); });
    document.addEventListener('keydown', event => {
        if (event.key !== 'Escape') return;
        // Consume Escape once so stacked native dialogs do not both close.
        if ($('confirm-dialog').open) {
            event.preventDefault(); event.stopImmediatePropagation(); resolveConfirm(false);
        } else if ($('sidebar').classList.contains('open')) {
            event.preventDefault(); mobile(false); $('mobile-menu').focus();
        }
    }, true);
    window.matchMedia('(max-width:760px)').addEventListener('change', () => mobile(false));
    window.addEventListener('pagehide', logout);
    mobile(false); $('admin-token').focus();
}
