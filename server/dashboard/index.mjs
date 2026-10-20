import { messages } from './i18n.mjs';
import { styles } from './styles.mjs';
import { dashboardApp } from './scripts.mjs';

const t = key => messages.tr[key];
const copy = key => `<span data-i18n="${key}">${t(key)}</span>`;
const icons = {
    overview: '<rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/>',
    licenses: '<circle cx="8" cy="15" r="4"/><path d="m11 12 9-9m-5 5 3 3m-1-5 3 3"/>',
    activity: '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>',
    system: '<path d="M3 12h4l3-8 4 16 3-8h4"/>',
    settings: '<path d="M4 6h16M4 12h16M4 18h16"/><circle cx="9" cy="6" r="2"/><circle cx="15" cy="12" r="2"/><circle cx="9" cy="18" r="2"/>',
    refresh: '<path d="M20 11a8 8 0 1 0-2 6M20 4v7h-7"/>',
    arrow: '<path d="M6 18 18 6M6 6h12v12"/>',
    plus: '<path d="M12 4v16M4 12h16"/>',
    logout: '<path d="M10 4H4v16h6m5-13 5 5-5 5m-5-5h10"/>'
};
const icon = name => `<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${icons[name]}</svg>`;
const brand = `<div class="brand"><svg viewBox="0 0 32 32" fill="none" aria-hidden="true"><path d="M5 25V15h7v10H5Zm8-7V8h7v10h-7Zm8-7V1h7v10h-7Z" fill="currentColor"/><path d="M13 26h15" stroke="currentColor" stroke-width="2"/></svg>OneStep <span>ADMIN</span></div>`;
const nav = page => `<button class="nav" data-page="${page}"${page === 'overview' ? ' aria-current="page"' : ''}><span class="nav-icon" aria-hidden="true">${icon(page)}</span>${copy(page)}</button>`;
const empty = (id, title, help) => `<div id="${id}" class="empty" hidden><strong data-i18n="${title}">${t(title)}</strong><p data-i18n="${help}">${t(help)}</p></div>`;
const pager = prefix => `<footer class="pager"><span id="${prefix}-summary" aria-live="polite">—</span><div class="actions"><button id="${prefix}-prev" disabled>${copy('previous')}</button><span id="${prefix}-page">—</span><button id="${prefix}-next" disabled>${copy('next')}</button></div></footer>`;
const html = `<!doctype html>
<html lang="tr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="theme-color" content="#0b0b10"><title>OneStep — Yönetim merkezi</title><link rel="stylesheet" href="/admin/assets/styles.css"><script type="module" src="/admin/assets/app.js"></script></head>
<body>
<section id="login" class="login"><div class="login-card">${brand}<h1 data-i18n="loginTitle">${t('loginTitle')}</h1><p class="login-copy" data-i18n="loginText">${t('loginText')}</p>
<form id="login-form"><p id="login-error" class="error" role="alert" hidden></p><button id="sign-in" class="primary full" type="submit" data-i18n="signIn">${t('signIn')}</button></form>
<p id="token-help" class="login-help" data-i18n="tokenHelp">${t('tokenHelp')}</p><p class="login-footer">ONESTEP / ADMINISTRATION</p></div></section>
<div id="app" class="shell" hidden><button id="mobile-shade" class="mobile-shade" aria-label="${t('close')}"></button>
<aside id="sidebar" class="sidebar" aria-label="${t('admin')}"><button id="mobile-close" class="mobile-close close-button" aria-label="${t('close')}">×</button>${brand}
<div class="workspace"><div class="workspace-mark">O</div><div><strong>OneStep</strong><small data-i18n="workspace">${t('workspace')}</small></div></div>
<p class="nav-label" data-i18n="admin">${t('admin')}</p><nav aria-label="${t('workspace')}">${nav('overview')}${nav('licenses')}${nav('activity')}${nav('system')}</nav>
<div class="sidebar-bottom">${nav('settings')}<button id="logout" class="nav"><span class="nav-icon" aria-hidden="true">${icon('logout')}</span>${copy('logout')}</button><div class="sidebar-foot">ONESTEP <span class="muted">/</span> CONTROL CENTER</div></div></aside>
<main class="main"><header class="topbar"><div class="breadcrumbs"><button id="mobile-menu" class="mobile-menu ghost" aria-label="${t('workspace')}" aria-expanded="false" aria-controls="sidebar">☰</button><span>OneStep</span><span>/</span><strong id="breadcrumb">${t('overview')}</strong></div><div class="account"><span id="connection" class="connection" data-state="unknown">${t('unchecked')}</span><span class="avatar" aria-hidden="true">A</span></div></header>
<div class="content"><div class="heading"><div><h1 id="page-title" tabindex="-1">${t('overview')}</h1><p id="page-description">${t('overviewText')}</p></div><div class="actions"><button id="refresh">${icon('refresh')} ${copy('refresh')}</button><button id="new-license" class="primary">${icon('plus')} ${copy('newLicense')}</button></div></div>
<div id="page-error" class="page-error" role="alert" hidden></div>
<section data-view="overview"><div class="hero"><div class="eyebrow" data-i18n="eyebrow">${t('eyebrow')}</div><h1 data-i18n="overviewTitle">${t('overviewTitle')}</h1><p data-i18n="overviewText">${t('overviewText')}</p></div>
<div id="stats" class="stats">${['total','active','unused','banned','expired'].map(key=>`<button class="stat" data-status="${key === 'total' ? 'all' : key}"><span class="stat-label"><span class="dot ${key}"></span>${copy(key)}</span><span class="stat-number" id="stat-${key}">—</span><span class="stat-foot">OneStep / ${key === 'total' ? 'ALL LICENSES' : key.toUpperCase()}</span></button>`).join('')}</div>
<div class="overview-grid"><article class="panel"><div class="panel-head"><h2 data-i18n="recent">${t('recent')}</h2><button class="link-button" data-page="activity">${copy('seeAll')} ${icon('arrow')}</button></div><div id="recent-activity" class="feed"></div>${empty('recent-empty','noActivity','noActivityHelp')}</article>
<article class="panel"><div class="panel-head"><h2 data-i18n="portfolio">${t('portfolio')}</h2><span class="small muted">${icon('arrow')}</span></div><div class="panel-body"><div id="distribution"></div><p class="note" data-i18n="portfolioHelp">${t('portfolioHelp')}</p></div></article></div></section>
<section data-view="licenses" hidden><article class="panel"><div class="toolbar"><input id="search" type="search" autocomplete="off" aria-label="${t('search')}" placeholder="${t('search')}"><select id="status-filter" aria-label="${t('status')}"><option value="all" data-i18n="allStatuses">${t('allStatuses')}</option>${['active','unused','banned','expired'].map(s=>`<option value="${s}" data-i18n="${s}">${t(s)}</option>`).join('')}</select><select id="sort" aria-label="${t('newest')}">${['newest','oldest','expiry'].map(s=>`<option value="${s}" data-i18n="${s==='expiry'?'expirySort':s}">${t(s==='expiry'?'expirySort':s)}</option>`).join('')}</select></div>
<div class="table-wrap"><table class="license-table"><thead><tr>${['license','status','device','expiration','epoch','details'].map(key=>`<th scope="col" data-i18n="${key}">${t(key)}</th>`).join('')}</tr></thead><tbody id="licenses-body"></tbody></table></div>${empty('licenses-empty','noLicenses','noLicensesHelp')}${pager('licenses')}</article></section>
<section data-view="activity" hidden><article class="panel"><div class="panel-head"><h2 data-i18n="activity">${t('activity')}</h2><span class="small muted">AUDIT LOG</span></div><div class="table-wrap"><table class="activity-table"><thead><tr>${['action','fingerprint','reason','epoch','time'].map(key=>`<th scope="col" data-i18n="${key}">${t(key)}</th>`).join('')}</tr></thead><tbody id="activity-body"></tbody></table></div>${empty('activity-empty','noActivity','noActivityHelp')}${pager('activity')}</article></section>
<section data-view="system" hidden><div id="system-cards" class="system-grid"></div><form id="release-form" class="panel preferences"><h2 data-i18n="publishRelease">${t('publishRelease')}</h2><p class="note" data-i18n="releaseHelp">${t('releaseHelp')}</p><label for="release-version" data-i18n="releaseVersion">${t('releaseVersion')}</label><input id="release-version" maxlength="64" pattern="[0-9A-Za-z][0-9A-Za-z._-]{0,63}" required autocomplete="off"><label for="release-file">DLL</label><input id="release-file" type="file" accept=".dll" required><button class="primary" type="submit" data-i18n="publishRelease">${t('publishRelease')}</button></form></section>
<section data-view="settings" class="preferences panel" hidden><div class="preference-row"><div><label for="language" data-i18n="language">${t('language')}</label><p data-i18n="languageHelp">${t('languageHelp')}</p></div><select id="language"><option value="tr">Türkçe</option><option value="en">English</option></select></div><div class="preference-row"><div><h2 data-i18n="session">${t('session')}</h2><p data-i18n="sessionHelp">${t('sessionHelp')}</p></div><button id="settings-logout" class="ghost">${copy('logout')}</button></div></section>
<footer class="bottom-note"><span>OneStep <span aria-hidden="true">·</span> ${copy('admin')}</span><span id="last-updated">—</span></footer></div></main></div>
<dialog id="detail-dialog" class="drawer" aria-labelledby="detail-title"><div class="modal-head"><h2 id="detail-title">${t('license')}</h2><button class="close-button" data-close="detail-dialog" aria-label="${t('close')}">×</button></div><div class="modal-body"><div id="detail-content"></div><div id="revealed-key" hidden><p class="note" data-i18n="keyHelp">${t('keyHelp')}</p><div id="revealed-value" class="key-box"></div><button id="copy-revealed">${copy('copy')}</button></div><div id="detail-actions" class="detail-actions"></div></div></dialog>
<dialog id="create-dialog" class="modal" aria-labelledby="create-title"><form id="create-form"><div class="modal-head"><h2 id="create-title" data-i18n="newLicense">${t('newLicense')}</h2><button type="button" class="close-button" data-close="create-dialog" aria-label="${t('close')}">×</button></div><div class="modal-body"><label for="duration" data-i18n="duration">${t('duration')}</label><select id="duration" class="full"><option value="30" data-i18n="thirtyDays">${t('thirtyDays')}</option><option value="7" data-i18n="sevenDays">${t('sevenDays')}</option><option value="1" data-i18n="oneDay">${t('oneDay')}</option><option value="lifetime" data-i18n="lifetime">${t('lifetime')}</option></select><p class="note" data-i18n="createWarning">${t('createWarning')}</p></div><div class="modal-actions"><button type="button" data-close="create-dialog">${copy('cancel')}</button><button type="submit" class="primary">${copy('create')}</button></div></form></dialog>
<dialog id="confirm-dialog" class="modal" aria-labelledby="confirm-title" aria-describedby="confirm-message"><div class="modal-head"><h2 id="confirm-title">${t('confirmTitle')}</h2></div><div class="modal-body"><p id="confirm-message"></p></div><div class="modal-actions"><button id="confirm-cancel" autofocus>${copy('cancel')}</button><button id="confirm-accept" class="primary">${copy('confirm')}</button></div></dialog>
<div id="toasts" class="toast-stack" role="status" aria-live="polite" aria-atomic="false"></div>
</body></html>`;

const script = `(${dashboardApp.toString()})(${JSON.stringify(messages)});`;
const headers = {
    'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff',
    'X-Frame-Options': 'DENY', 'Referrer-Policy': 'no-referrer',
    'Content-Security-Policy': "default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; img-src 'self'; base-uri 'none'; form-action 'self'; frame-ancestors 'none'",
    'Permissions-Policy': 'camera=(), microphone=(), geolocation=()'
};

export function dashboardResponse(request) {
    const path = new URL(request.url).pathname;
    if (path === '/' && ['GET', 'HEAD'].includes(request.method))
        return new Response(null, {status:302, headers:{...headers, Location:'/admin'}});
    const assets = { '/admin/assets/styles.css': [styles, 'text/css'], '/admin/assets/app.js': [script, 'text/javascript'] };
    const document = ['/', '/admin', '/admin/'].includes(path);
    if (!document && !Object.hasOwn(assets, path)) return null;
    if (!['GET', 'HEAD'].includes(request.method)) return new Response(null, { status: 405, headers: { ...headers, Allow: 'GET, HEAD' } });
    const [body, type] = document ? [html, 'text/html'] : assets[path];
    return new Response(request.method === 'HEAD' ? null : body, { headers: { ...headers, 'Content-Type': type + '; charset=utf-8' } });
}
