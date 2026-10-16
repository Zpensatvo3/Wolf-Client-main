export const styles = String.raw`
:root {
    color-scheme:dark;
    --bg:#0b0b10;
    --side:#101016;
    --surface:#131319;
    --raised:#191920;
    --line:#27272f;
    --text:#ededf2;
    --muted:#9695a6;
    --dim:#858491;
    --accent:#a697f9;
    --accent-bg:#252034;
    --success:#86cfaf;
    --danger:#ef9da8;
    --radius:14px;
    font-family:Inter,ui-sans-serif,-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
    font-synthesis:none;
}
* {
    box-sizing:border-box;
}
body {
    margin:0;
    background:var(--bg);
    color:var(--text);
    font-size:14px;
    line-height:1.5;
    -webkit-font-smoothing:antialiased;
}
button,input,select {
    font:inherit;
}
button,a,input,select {
    -webkit-tap-highlight-color:transparent;
}
button {
    cursor:pointer;
}
button:disabled {
    opacity:.5;
    cursor:not-allowed;
}
button,input,select,a {
    outline-offset:4px;
}
button:focus-visible,input:focus-visible,select:focus-visible,a:focus-visible {
    outline:2px solid var(--accent);
}
[hidden] {
    display:none!important;
}
h1,h2,h3,p {
    margin:0;
}
h1 {
    font-size:30px;
    font-weight:600;
    letter-spacing:-1px;
}
h2 {
    font-size:15px;
    font-weight:600;
}
a {
    color:var(--accent);
}
button {
    border:1px solid var(--line);
    border-radius:8px;
    background:var(--raised);
    color:var(--text);
    padding:9px 14px;
    transition:background .16s,border-color .16s;
}
button:hover:not(:disabled) {
    background:#23232e;
    border-color:#41404f;
}
.primary {
    color:#16101f;
    background:#b9a9ff;
    border-color:#b9a9ff;
    font-weight:600;
    box-shadow:0 2px 6px #0004;
}
.primary:hover:not(:disabled) {
    background:#cabdff;
    border-color:#cabdff;
}
.ghost {
    background:transparent;
}
.danger {
    color:var(--danger);
    border-color:#59313d;
    background:#291b23;
}
.full {
    width:100%;
}
.muted {
    color:var(--muted);
}
.small {
    font-size:12px;
}
.mono {
    font-family:ui-monospace,SFMono-Regular,Consolas,monospace;
    font-size:12px;
    overflow-wrap:anywhere;
}
input,select {
    min-height:40px;
    border:1px solid var(--line);
    border-radius:8px;
    background:#101016;
    color:var(--text);
    padding:9px 12px;
    max-width:100%;
}
input:focus,select:focus {
    border-color:var(--accent);
}
input::placeholder {
    color:var(--dim);
}
label {
    display:grid;
    gap:8px;
    font-size:12px;
    color:var(--muted);
}
.brand {
    display:flex;
    gap:10px;
    align-items:center;
    font-size:19px;
    font-weight:650;
    letter-spacing:-.6px;
}
.brand svg {
    width:30px;
    height:30px;
    color:#b6a8f8;
}
.brand span:last-child {
    font-size:11px;
    font-weight:500;
    border:1px solid var(--line);
    padding:2px 6px;
    border-radius:5px;
    color:var(--muted);
    letter-spacing:0;
}
.login {
    min-height:100svh;
    display:grid;
    place-items:center;
    padding:32px 20px;
    background:radial-gradient(ellipse at 50% 22%,#30204433,transparent 50%);
}
.login-card {
    width:min(100%,414px);
}
.login .brand {
    justify-content:center;
    margin-bottom:52px;
}
.login-card h1 {
    text-align:center;
    font-size:32px;
}
.login-copy {
    text-align:center;
    color:var(--muted);
    margin:12px 0 30px;
    line-height:1.75;
}
.login form {
    padding:25px;
    border:1px solid var(--line);
    border-radius:var(--radius);
    background:linear-gradient(160deg,#19171f,#121218);
    box-shadow:0 18px 50px #0003;
}
.login input {
    width:100%;
    margin-bottom:16px;
}
.login-help {
    text-align:center;
    font-size:12px;
    color:var(--dim);
    margin-top:22px;
}
.error {
    color:var(--danger);
    font-size:13px;
    margin:12px 0;
}
.login-footer {
    text-align:center;
    color:var(--dim);
    font-size:11px;
    margin-top:54px;
    letter-spacing:1.5px;
}
.shell {
    min-height:100vh;
    display:grid;
    grid-template-columns:228px minmax(0,1fr);
}
.sidebar {
    height:100vh;
    position:sticky;
    top:0;
    display:flex;
    flex-direction:column;
    padding:28px 16px 18px;
    background:var(--side);
    border-right:1px solid var(--line);
    z-index:20;
}
.sidebar .brand {
    padding:0 10px 35px;
}
.workspace {
    display:flex;
    gap:10px;
    align-items:center;
    margin:0 4px 30px;
    padding:12px 9px;
    border:1px solid var(--line);
    border-radius:9px;
    background:#15151c;
}
.workspace-mark {
    display:grid;
    place-items:center;
    width:32px;
    height:32px;
    background:#30283e;
    border:1px solid #493b5e;
    border-radius:8px;
    color:#c6b4ed;
    font-weight:600;
}
.workspace strong {
    display:block;
    font-size:12px;
}
.workspace small {
    color:var(--dim);
    font-size:10px;
}
.nav-label {
    margin:0 12px 12px;
    color:var(--dim);
    font-size:10px;
    letter-spacing:1.5px;
    text-transform:uppercase;
}
nav {
    display:grid;
    gap:5px;
}
.nav {
    display:flex;
    align-items:center;
    gap:12px;
    text-align:left;
    background:transparent;
    border-color:transparent;
    color:var(--muted);
    font-size:13px;
    padding:10px 13px;
}
.nav .nav-icon {
    font-size:17px;
    width:18px;
    text-align:center;
}
.nav[aria-current=page] {
    color:#d6ccff;
    background:var(--accent-bg);
    border-color:#393049;
}
.sidebar-bottom {
    margin-top:auto;
    display:grid;
    gap:8px;
    padding-top:26px;
}
.sidebar-foot {
    color:var(--dim);
    font-size:10px;
    padding:12px;
    border-top:1px solid var(--line);
}
.main {
    min-width:0;
}
.topbar {
    height:68px;
    border-bottom:1px solid var(--line);
    display:flex;
    align-items:center;
    justify-content:space-between;
    padding:0 38px;
}
.breadcrumbs {
    display:flex;
    gap:13px;
    align-items:center;
    color:var(--dim);
    font-size:12px;
}
.breadcrumbs strong {
    font-weight:500;
    color:var(--text);
}
.account {
    display:flex;
    align-items:center;
    gap:10px;
    color:var(--muted);
    font-size:12px;
}
.avatar {
    width:27px;
    height:27px;
    border-radius:50%;
    display:grid;
    place-items:center;
    background:linear-gradient(140deg,#594775,#2b2638);
    border:1px solid #685880;
    color:#e2d8f3;
    font-size:11px;
}
.content {
    max-width:1560px;
    margin:0 auto;
    padding:38px;
}
.heading {
    display:flex;
    justify-content:space-between;
    gap:20px;
    align-items:center;
    margin-bottom:30px;
}
.heading p {
    margin-top:7px;
    color:var(--muted);
    font-size:13px;
}
.actions {
    display:flex;
    gap:9px;
    align-items:center;
}
.connection {
    color:var(--muted);
    font-size:11px;
    display:inline-flex;
    align-items:center;
    gap:7px;
}
.connection::before {
    content:'';
    width:5px;
    height:5px;
    border-radius:50%;
    background:var(--dim);
}
.connection[data-state=ok]::before {
    background:var(--success);
}
.connection[data-state=error]::before {
    background:var(--danger);
}
.eyebrow {
    color:var(--accent);
    font-size:10px;
    letter-spacing:2px;
    margin-bottom:12px;
}
.hero {
    padding:7px 0 28px;
}
.hero h1 {
    font-size:36px;
    letter-spacing:-1.4px;
}
.hero p {
    color:var(--muted);
    margin-top:10px;
}
.stats {
    display:grid;
    grid-template-columns:repeat(5,minmax(0,1fr));
    gap:12px;
    margin:0 0 26px;
}
.stat {
    padding:20px;
    text-align:left;
    border:1px solid var(--line);
    border-radius:var(--radius);
    background:linear-gradient(140deg,#1b1822,#131319);
    min-width:0;
}
.stat:hover {
    border-color:#4c405e;
}
.stat-label {
    display:flex;
    gap:8px;
    align-items:center;
    font-size:11px;
    color:var(--muted);
}
.stat-number {
    display:block;
    font-size:30px;
    line-height:1.3;
    letter-spacing:-1px;
    margin-top:18px;
    font-weight:550;
}
.stat-foot {
    font-size:10px;
    color:var(--dim);
    display:block;
    margin-top:7px;
}
.dot {
    display:inline-block;
    width:5px;
    height:5px;
    border-radius:50%;
    background:var(--accent);
}
.dot.active {
    background:var(--success);
}
.dot.banned {
    background:var(--danger);
}
.dot.expired {
    background:#b6a08a;
}
.dot.unused {
    background:#9daacb;
}
.overview-grid {
    display:grid;
    grid-template-columns:minmax(0,1.6fr) minmax(260px,1fr);
    gap:20px;
}
.panel {
    background:var(--surface);
    border:1px solid var(--line);
    border-radius:var(--radius);
    overflow:hidden;
}
.panel-head {
    display:flex;
    align-items:center;
    justify-content:space-between;
    padding:20px 23px;
    border-bottom:1px solid var(--line);
}
.link-button {
    background:transparent;
    border:0;
    color:var(--accent);
    padding:4px 0;
    font-size:12px;
}
.panel-body {
    padding:23px;
}
.feed {
    padding:5px 23px;
}
.feed-item {
    display:flex;
    gap:12px;
    align-items:flex-start;
    padding:17px 0;
    border-bottom:1px solid #23232b;
}
.feed-item:last-child {
    border-bottom:0;
}
.feed-symbol {
    margin-top:2px;
    width:28px;
    height:28px;
    flex-shrink:0;
    display:grid;
    place-items:center;
    border:1px solid #373040;
    background:#231f2b;
    border-radius:8px;
    color:var(--accent);
    font-size:12px;
}
.feed-copy {
    flex:1;
    min-width:0;
}
.feed-copy strong {
    font-size:12px;
    font-weight:500;
}
.feed-copy p {
    color:var(--dim);
    font-size:11px;
    margin-top:5px;
    overflow-wrap:anywhere;
}
.feed time {
    color:var(--dim);
    font-size:10px;
    white-space:nowrap;
}
.distribution-item {
    display:flex;
    justify-content:space-between;
    align-items:center;
    padding:12px 0;
    font-size:12px;
    color:var(--muted);
}
.distribution-item strong {
    color:var(--text);
    font-weight:500;
}
.distribution-item span {
    display:flex;
    align-items:center;
    gap:8px;
}
.note {
    font-size:11px;
    line-height:1.7;
    color:var(--dim);
}
.toolbar {
    display:flex;
    align-items:center;
    gap:10px;
    flex-wrap:wrap;
    padding:18px 20px;
    border-bottom:1px solid var(--line);
}
.toolbar input {
    flex:1;
    min-width:180px;
}
.toolbar select {
    font-size:12px;
}
.table-wrap {
    overflow:auto;
}
table {
    width:100%;
    border-collapse:collapse;
    text-align:left;
}
thead {
    background:#101016;
}
th {
    padding:12px 20px;
    font-size:10px;
    letter-spacing:.7px;
    color:var(--dim);
    font-weight:500;
    text-transform:uppercase;
}
td {
    padding:17px 20px;
    border-top:1px solid #25252d;
    font-size:12px;
    white-space:nowrap;
}
tbody tr:hover {
    background:#181820;
}
.license-ref {
    display:block;
    background:transparent;
    border:0;
    padding:0;
    text-align:left;
    color:var(--text);
    font-weight:600;
    font-size:12px;
}
.license-ref:hover:not(:disabled) {
    background:transparent;
    color:var(--accent);
}
td small {
    display:block;
    color:var(--dim);
    font-size:10px;
    margin-top:4px;
}
.badge {
    display:inline-flex;
    gap:6px;
    align-items:center;
    padding:3px 8px;
    border:1px solid #373540;
    border-radius:6px;
    font-size:10px;
    color:#beb8d0;
    background:#24212c;
}
.badge::before {
    content:'';
    width:4px;
    height:4px;
    border-radius:50%;
    background:currentColor;
}
.badge.active {
    color:var(--success);
    background:#172921;
    border-color:#294235;
}
.badge.banned {
    color:var(--danger);
    background:#2e1c24;
    border-color:#4b2c39;
}
.badge.expired {
    color:#c4b4a0;
    background:#28231e;
    border-color:#40362b;
}
.badge.unused {
    color:#aebbd9;
    background:#1f2532;
    border-color:#333d51;
}
.row-more {
    padding:5px 10px;
    background:transparent;
    color:var(--muted);
}
.pager {
    display:flex;
    align-items:center;
    justify-content:space-between;
    gap:12px;
    padding:17px 20px;
    border-top:1px solid var(--line);
    font-size:11px;
    color:var(--muted);
}
.pager button {
    font-size:11px;
    padding:6px 11px;
}
.empty {
    text-align:center;
    padding:55px 20px;
    color:var(--muted);
}
.empty strong {
    display:block;
    color:var(--text);
    font-weight:500;
    margin-bottom:8px;
}
.empty p {
    font-size:12px;
}
.skeleton {
    height:12px;
    width:80%;
    border-radius:4px;
    background:linear-gradient(90deg,#202028,#2e2937,#202028);
    background-size:200% 100%;
    animation:shimmer 1.8s infinite;
}
.skeleton.wide {
    height:72px;
    width:100%;
    margin:15px 0;
}
.system-grid {
    display:grid;
    grid-template-columns:repeat(2,minmax(0,1fr));
    gap:15px;
}
.system-card {
    padding:24px;
    border:1px solid var(--line);
    border-radius:12px;
    background:var(--surface);
}
.system-card h2 {
    font-size:12px;
    color:var(--muted);
    font-weight:400;
    margin-bottom:15px;
}
.system-card p {
    font-size:15px;
}
.preferences {
    max-width:730px;
}
.preference-row {
    padding:24px;
    display:flex;
    gap:30px;
    justify-content:space-between;
    border-bottom:1px solid var(--line);
}
.preference-row:last-child {
    border-bottom:0;
}
.preference-row p {
    font-size:12px;
    color:var(--muted);
    margin-top:6px;
    max-width:400px;
}
.preference-row label {
    display:block;
    font-size:14px;
    color:var(--text);
}
.activity-table td:nth-child(3) {
    white-space:normal;
    overflow-wrap:anywhere;
    max-width:380px;
}
.page-error {
    padding:15px 20px;
    border:1px solid #56303d;
    background:#291b23;
    color:var(--danger);
    border-radius:9px;
    margin-bottom:20px;
    font-size:12px;
}
.bottom-note {
    display:flex;
    justify-content:space-between;
    align-items:center;
    gap:12px;
    margin-top:26px;
    padding-top:18px;
    border-top:1px solid #22222b;
    color:var(--dim);
    font-size:10px;
}
dialog {
    border:1px solid #3a3545;
    border-radius:16px;
    background:#17151e;
    color:var(--text);
    padding:0;
    max-width:calc(100vw - 32px);
    box-shadow:0 30px 100px #0008;
}
dialog::backdrop {
    background:#06060bba;
    backdrop-filter:blur(4px);
}
dialog[open] {
    animation:arrive .18s ease-out;
}
.modal {
    width:430px;
}
.modal-head {
    padding:25px 26px 0;
    display:flex;
    justify-content:space-between;
    align-items:center;
}
.modal-head h2 {
    font-size:18px;
    letter-spacing:-.3px;
}
.modal-body {
    padding:22px 26px 26px;
}
.modal-body p {
    font-size:13px;
    color:var(--muted);
    line-height:1.8;
}
.modal-body label {
    margin:15px 0;
}
.modal-actions {
    display:flex;
    justify-content:flex-end;
    gap:9px;
    padding:17px 26px;
    border-top:1px solid var(--line);
    background:#121117;
}
.close-button {
    border:0;
    background:transparent;
    color:var(--muted);
    font-size:19px;
    padding:0 6px;
}
.drawer {
    position:fixed;
    margin:0 0 0 auto;
    inset:0;
    width:480px;
    height:100dvh;
    max-height:100dvh;
    max-width:100vw;
    border-radius:16px 0 0 16px;
    border-right:0;
}
.drawer .modal-head {
    padding-top:30px;
}
.drawer .modal-body {
    padding-top:25px;
}
.details {
    margin:0;
}
.details div {
    display:grid;
    grid-template-columns:145px minmax(0,1fr);
    gap:12px;
    padding:12px 0;
    border-bottom:1px solid var(--line);
    font-size:12px;
}
.details dt {
    color:var(--muted);
}
.details dd {
    margin:0;
    overflow-wrap:anywhere;
}
.detail-actions {
    display:grid;
    gap:9px;
    margin-top:25px;
}
.detail-actions button {
    text-align:left;
    font-size:12px;
}
.key-box {
    padding:16px;
    margin:18px 0;
    background:#211d2b;
    border:1px solid #40354e;
    border-radius:9px;
    overflow-wrap:anywhere;
    font-family:ui-monospace,monospace;
    user-select:all;
    color:#d9ccff;
}
.toast-stack {
    position:fixed;
    right:24px;
    bottom:24px;
    z-index:60;
    display:grid;
    gap:10px;
    width:min(360px,calc(100vw - 48px));
    pointer-events:none;
}
.toast {
    padding:15px 18px;
    background:#211d2a;
    border:1px solid #463959;
    border-radius:10px;
    box-shadow:0 10px 30px #0005;
    font-size:12px;
    animation:arrive .18s;
}
.toast.error-toast {
    border-color:#683946;
    color:var(--danger);
}
.mobile-menu,.mobile-shade,.mobile-close {
    display:none;
}
@keyframes shimmer {
    to {
        background-position:-200% 0;
    }
}
@keyframes arrive {
    from {
        opacity:0;
        transform:translateY(8px);
    }
    to {
        opacity:1;
        transform:translateY(0);
    }
}
@media(min-width:1600px) {
    .content {
        padding:48px 50px;
    }
    .stats {
        gap:18px;
    }
    .stat {
        padding:24px;
    }
}
@media(max-width:1100px) {
    .shell {
        grid-template-columns:195px minmax(0,1fr);
    }
    .content {
        padding:28px;
    }
    .topbar {
        padding:0 28px;
    }
    .stats {
        grid-template-columns:repeat(3,minmax(0,1fr));
    }
    .overview-grid {
        grid-template-columns:1fr;
    }
    .hero h1 {
        font-size:30px;
    }
    .stat:last-child {
        grid-column:auto;
    }
    .toolbar select {
        flex:1;
    }
    .system-grid {
        grid-template-columns:1fr;
    }
}
@media(max-width:760px) {
    .shell {
        display:block;
    }
    .sidebar {
        position:fixed;
        left:0;
        top:0;
        width:250px;
        transform:translateX(-100%);
        transition:transform .2s;
    }
    .sidebar.open {
        transform:translateX(0);
    }
    .mobile-menu {
        display:block;
        padding:4px 9px;
        font-size:18px;
    }
    .mobile-close {
        display:block;
        position:absolute;
        right:13px;
        top:16px;
    }
    .mobile-shade.open {
        display:block;
        position:fixed;
        inset:0;
        background:#0009;
        z-index:19;
        border:0;
        border-radius:0;
    }
    .topbar {
        padding:0 20px;
        height:60px;
    }
    .breadcrumbs {
        gap:9px;
    }
    .account .muted {
        display:none;
    }
    .content {
        padding:24px 20px;
    }
    .heading {
        align-items:flex-start;
        gap:12px;
        flex-wrap:wrap;
        margin-bottom:22px;
    }
    .heading h1 {
        font-size:25px;
    }
    .heading .actions {
        margin-top:4px;
    }
    .hero {
        padding:0 0 23px;
    }
    .hero h1 {
        font-size:30px;
    }
    .hero p {
        font-size:12px;
    }
    .stats {
        grid-template-columns:repeat(2,minmax(0,1fr));
        gap:10px;
    }
    .stat {
        padding:16px;
    }
    .stat:first-child {
        grid-column:span 2;
    }
    .stat:first-child .stat-number {
        margin-top:10px;
    }
    .stat-number {
        font-size:27px;
    }
    .panel-head,.panel-body {
        padding:18px;
    }
    .feed {
        padding:0 18px;
    }
    .feed time {
        font-size:9px;
    }
    .toolbar {
        padding:14px;
        gap:8px;
    }
    .toolbar input {
        min-width:100%;
        font-size:12px;
    }
    .toolbar select {
        width:45%;
        min-width:0;
    }
    .pager {
        padding:14px;
        flex-wrap:wrap;
    }
    .license-table thead {
        display:none;
    }
    .license-table tbody,.license-table tr {
        display:block;
    }
    .license-table tr {
        padding:14px;
        display:grid;
        grid-template-columns:1fr 1fr;
        gap:12px;
        border-bottom:1px solid var(--line);
        position:relative;
    }
    .license-table td {
        display:block;
        padding:0;
        border:0;
        font-size:11px;
    }
    .license-table td::before {
        content:attr(data-label);
        display:block;
        font-size:9px;
        color:var(--dim);
        margin-bottom:5px;
    }
    .license-table td:first-child {
        grid-column:span 2;
    }
    .license-table td:last-child {
        position:absolute;
        top:14px;
        right:14px;
    }
    .license-table td:last-child::before {
        display:none;
    }
    .license-ref {
        font-size:13px;
    }
    .activity-table {
        min-width:600px;
    }
    .activity-table td,.activity-table th {
        padding:14px;
    }
    .preference-row {
        padding:18px;
        flex-wrap:wrap;
        gap:15px;
    }
    .preference-row select {
        width:100%;
    }
    .details div {
        grid-template-columns:125px minmax(0,1fr);
    }
    .bottom-note {
        align-items:flex-start;
        font-size:9px;
    }
    .drawer {
        border-radius:0;
        width:100%;
    }
    .modal-head,.modal-body {
        padding-left:22px;
        padding-right:22px;
    }
}
@media(prefers-reduced-motion:reduce) {
    *,*::before,*::after {
        animation:none!important;
        transition:none!important;
        scroll-behavior:auto!important;
    }
}
.icon {
    width:16px;
    height:16px;
    vertical-align:middle;
    flex-shrink:0;
}
button:not(.stat):not(.nav):has(.icon) {
    display:inline-flex;
    align-items:center;
    justify-content:center;
    gap:7px;
}
#page-title:focus { outline:none; }
`;
