# Auth hardening — 20 October 2026

## Latest update: launcher-only v2 is live

Worker `b2daf1f6-1253-44ef-9cc7-d3a814fc7d37` now disables v1 activation,
launcher ticket/redemption and heartbeat paths. Use the new artifacts under
`build/launcher-v2/`, not the earlier `auth-rotation-20261020` binaries.
See `../docs/LAUNCHER_V2.md` for the current protocol, checks and deployment IDs.
The older signing-cutover notes below remain historical. Local secret-file
cleanup and controlled pepper/license reissue are still outstanding.

## Current status: new signer and DLL LIVE; local secret cleanup BLOCKED

The owner completed the Access login and confirmed reaching the admin panel.
The authenticated production System status page was inspected successfully.
The new DLL was uploaded through that browser's ordinary Access-protected form;
no Access cookie, bearer, private key or production secret was extracted.

- Release: `auth-20261020-1.21.11`, Fabric 1.21.11 x64.
- DLL: `build/auth-rotation-20261020/1.21.11/Onestep.dll`, 24,247,296 bytes.
- SHA-256: `548248b01b5f000827d1a3e90403fc8832a811acc8c380edbdc556fd6b163e5c`.
- Both the authenticated dashboard and the KV `current` record confirmed the new
  release; the server-validated hash and size matched the local build before
  signing activation. KV propagation initially returned the old record, so
  activation was deliberately delayed until that check passed.
- Active Worker version: `ff471720-3fd7-45a1-bab2-3b576415dc32` at 100%.
- Deployment: `3188282e-27c3-4879-a557-359165f3f0b2`.
- Activation: `2026-10-20T12:03:58.141077Z`.
- Signing key ID: `onestep-20261020-7615fc79`.
- Launcher: `build/auth-rotation-20261020/launcher/Onestep-Launcher.exe`,
  SHA-256 `f37a843693d5739bf76825e1fa6b2f1cd742cc21d904cad6b9a81a4f0b188361`.
  This new launcher is built locally; a public launcher installer/download was
  NOT published. Existing launcher executables must be replaced manually.

The deployed version uses the current Access-only source, NOT the obsolete
staging dashboard. Only the two new signing bindings were inherited explicitly
from staged version `51f35f2f-7b65-4cae-8846-7d806431800c`. Database, limiter,
release storage and peppers were inherited explicitly from the previous live
Access-only version `31f51779-db23-43a9-982b-52f64a618a6a`. The active binding
inventory confirms the fresh key ID and absence of `ADMIN_TOKEN_HASH` and
`ALLOW_DEVICE_CONFIGS`. No new private-key file or local API credential was made.

The old signing key is no longer trusted by the active server, and old clients
pinning its public key cannot validate new signatures. Prior cloud Worker
versions still exist for history; do not deploy them or assume key removal from
history has occurred. Existing native DLL files were preserved, not overwritten.
No Minecraft process was stopped, injected or used for a game test.
After activation, `/health` returned 200 with `status: ok`. An anonymous request
to `/api/v1/admin/overview` returned 302 to the expected Access team hostname.
Refreshing the existing authenticated admin session still displayed a healthy
database and the new release. The deployment metadata public key also matched
the native build pin; the focused diff whitespace check passed.

### Remaining work — do not claim the incident resolved

The exact old PEM/PKCS8 and encrypted/wrapping vault files were inventoried in
both reviewed checkouts below. Their contents were NOT opened. An exact-file
cleanup attempt was rejected by the execution policy BEFORE execution;
**no local secret files were removed**. Do not bypass this policy with another
tool or deletion mechanism. A human must remove these five paths under EACH
reviewed `server/` directory:

- `signing-ec.pem`
- `signing-private.pk8`
- `.credentials/worker-secrets.enc.json`
- `.credentials/wrapping-key.dpapi`
- `.credentials/legacy-licenses-2026-10-16.enc.json`

Reviewed directories:

- `C:/Users/zpens/Downloads/Wolf-Client-main/Wolf-Client-main/server/`
- `C:/Users/zpens/OneDrive/Dokumenty/Wolf-Client-main1/Wolf-Client-main/server/`

Public verification material is not secret and must remain pinned in clients.
Other backups, OneDrive version history, recycle bins and checkouts were not
exhaustively scanned. Deleting these files alone cannot revoke an exposed key.
`KEY_PEPPER` and `HWID_PEPPER` remain unchanged to preserve existing hashes;
their controlled migration and exposed customer-license reissue are still
outstanding. Do not call local-secret elimination or incident recovery complete.
No live licensed launcher/game round-trip has been tested after cutover yet.

## Historical execution notes (superseded by current status above)

The following records describe intermediate states and the original operator
checklist. Statements about pending login/signing and unapplied migrations below
are HISTORICAL, not current instructions. Do not repeat an applied migration or
restore an old signer/admin-token version from these notes.

### Earlier Access-only deployment, before signing cutover

Production Worker version `31f51779-db23-43a9-982b-52f64a618a6a` is now live
(deployment `128d53d1-5631-4d8d-b967-fe6e2a94645a`). Admin API and dashboard use
verified Cloudflare Access identity ONLY, with independent MFA at the edge.
The legacy `ADMIN_TOKEN_HASH` binding is absent from the deployed version;
there is no bearer/password fallback. Browser API requests include same-origin
Access cookies; writes and release uploads require the matching Origin and reject
cross-site fetches. Logout uses Cloudflare's actual Access logout URL.

The nonce table/index migration is applied. All FOUR license records, hashes,
expiry dates and device bindings remain unchanged. FOUR recoverable encrypted
license copies were cleared after obtaining the cloud Time Travel bookmark
`00000040-00000000-0000510a-edc02fae08f123351203caae8e16833f`.
No plaintext database backup was written locally. Cloud history can retain the
prior encrypted copies within its retention window.

The signing key, local legacy vaults, OneDrive copies and shipped native releases
have NOT yet been cut over or removed. The new private key remains in an inactive
Cloudflare version, never exported to this computer.
Treat the audit's exposed credentials as compromised until rotation is complete.
Changing helper scripts does not stop another program from opening old DPAPI
files directly. Never call the incident resolved merely because the source changed.

Zero Trust is now enabled. The `OneStep Admin` self-hosted application protects
`onestep-auth.gogogamesas.workers.dev/admin` and `/api/v1/admin`, including their
subpaths, under one audience. The only Allow policy matches
**gogogamesas@gmail.com**, through the existing account-members-only Cloudflare
identity provider. Application and independent MFA sessions are both 30 minutes;
TOTP, biometrics and security keys are allowed. Global MFA enforcement remains
off; the admin application explicitly requires MFA. Human authenticator
enrollment and an authorized browser login have NOT yet been verified.

Application ID: `66e9a254-7842-4092-97ba-e88f328c9215`.
Policy ID: `f305cb66-d4e3-4322-89bb-905216fd0cd1`.
The initial setup missed the separate App Launcher application. The user reached
`#/NoAuth` with the explicit "enable the Access App Launcher" message. That
missing prerequisite was corrected by creating an `app_launcher` application:
`f4ead4bd-3ecf-43a6-a024-832576359281`, policy
`859909c3-1fe7-4193-85a6-fe4f1068e1ff`. Only `gogogamesas@gmail.com` may enter,
using the same account-members-only Cloudflare identity provider and 30-minute
session. The admin application, audience, policy and MFA requirements were NOT
modified. Cloudflare's enrollment portal must remain reachable before a user's
first MFA enrollment; it does not grant access to the admin API without admin MFA.
Open the team root without the stale `#/NoAuth` fragment, or use
`https://small-disk-6c42.cloudflareaccess.com/AddMfaDevice` for enrollment.
Portal creation/policy persistence is verified; the human's successful login and
authenticator enrollment still require their own confirmation. Never collect
their authenticator QR code, setup secret or OTP in chat.
Public issuer and audience are recorded in `wrangler.jsonc`; they are not secrets.
Anonymous requests to the admin document, its assets and admin API now redirect
to Access. `/health` still returns 200. Game endpoints are outside the protected
destinations. The later Access-only deployment and migration are recorded above.

The deployed Worker also cryptographically verifies Access JWTs itself on admin
routes, rather than trusting an email header or depending only on hostname
coverage. Human authorized production login is still pending. Do not call the
incident resolved yet.

Do not remove the Access application or roll back to the old bearer-only server
as a routine recovery step. Preserve the Access identity guard and MFA on rollback.

### Signing rotation staged, not active

The operator approved rebuilding native clients and retiring the old signing key.
A new P-256 pair was generated inside the connected Cloudflare management tool's
execution environment. The private key was sent directly into a Worker version's
secret binding: no new private-key file, vault export or plaintext chat output was
created on this computer. The pair passed a sign/verify self-check before upload.

- Staged Worker version: `51f35f2f-7b65-4cae-8846-7d806431800c`.
- Signing key ID: `onestep-20261020-7615fc79`.
- `AuthDeployment.props` now pins the NEW PUBLIC key for the approved builds.
- Native Release x64 builds completed for Fabric 1.21.11 and the launcher, with
  the new public key confirmed in BOTH binaries. Isolated outputs and hashes are
  recorded in `deployment.json`; no existing DLL was overwritten. These files
  are NOT published and will not authenticate against the still-old live signer.
- At staging, production still served `10c8095c-776c-4d8d-b205-d96d04573f59`.
- The operator chose Access-only administration. The original staged version
  `51f35f2f-7b65-4cae-8846-7d806431800c` still contains the OLD admin hash and the
  previous two-layer dashboard. Do NOT deploy that version. A future cutover must
  use the CURRENT Access-only source, omit `ADMIN_TOKEN_HASH`, inherit only the
  new signing bindings explicitly from that staged version, and inherit other
  required resources from the current live version. The newer Workers version
  API supports explicit `inherit.version_id`; script upload does not.
- Signing activation is blocked on publishing the newly built native DLL.
  The existing Wrangler CLI login has expired and was NOT replaced with a new
  local API credential. A file upload form is available in the Access-protected
  dashboard's System status page. The human must complete Cloudflare login/MFA;
  perform release publication and signing cutover together after that login.
  Do not switch the signer while the launcher still downloads the old DLL.
- Do not deploy any other version before promotion without reviewing inherited
  secret bindings: `inherit` defaults to the latest uploaded version, not the
  currently deployed version.

The existing `server/signing-ec.pem`, `server/signing-private.pk8` and
`server/.credentials/` files were detected by filename/metadata only; their
private contents were NOT read. They have NOT yet been removed or migrated.
They remain an exposure until secrets are retired and old copies are cleaned up.
The same two private-key filenames and the encrypted/wrapping vault files also
exist in `C:/Users/zpens/OneDrive/Dokumenty/Wolf-Client-main1/Wolf-Client-main/server/`.
That was a metadata-only check; no old vault was opened. No v2 operator vault was
found at the default `C:/Users/zpens/AppData/Local/OneStep/OperatorVault/` path.
Other checkouts, OneDrive version history and backups have not been exhaustively
inventoried. Do not claim that no local secret remains.

Public verification keys, key IDs, issuer domains and audience IDs are not
credentials. A public verification key must be pinned in the launcher/DLL; fetching
an untrusted replacement key from the same server would weaken authentication.
Private signing keys, admin bearer tokens and production peppers must not be
written into source files, local vaults or build logs for this rotation.
This cloud-only storage requirement supersedes the earlier recommendation to
import production secrets into a password-encrypted local operator vault. Do NOT
run that import for the current rotation. The helper and its synthetic tests are
legacy recovery tooling, not the selected production storage workflow.

`KEY_PEPPER` and `HWID_PEPPER` were NOT rotated: replacing them without a migration
would invalidate license/device hashes and config ownership. No customer license
was revoked or reissued as part of staging. At the preflight inspection, D1 held
four license records and four recoverable encrypted copies; those copies were
subsequently cleared by the migration recorded above. The earlier preflight Time Travel bookmark is
`0000003e-00000000-0000510a-8827027c722e3d3f7185e2bae86d33e2`.
Retrieve a fresh bookmark immediately before any migration; do not export private
database backups to this development workspace.

Rotation checks: 19 server tests and 17 native compiler-guard scenarios passed.
The first workerd runtime check timed out while native compilation was running;
it was not counted as passing. The single post-build retry PASSED in workerd,
including real SQLite D1, Durable Object concurrency and signed authentication.
The Access-only change also passed all 19 server tests, one workerd runtime test,
both minified/unminified bundle checks, and a real headless Edge integration test
covering automatic login, pages, mutations, XSS rendering, mobile layout and logout.
The browser test exposed a queued dialog-close race; guarding reopened dialogs
fixed it and the integration test then passed. No production identity/token was
used in those tests. Live anonymous admin requests redirect to Access; health is
200 and a malformed game auth request is 400 without an Access redirect.

## Implemented

- Native auth defaults to required. Missing production metadata fails compilation;
  MSBuild rejects `AuthRequired=false`. Only explicitly marked non-release test
  probes may opt out. These are build mistakes prevented, NOT an unpatchable DLL.
- Both the admin document/assets and ALL admin API paths, including release
  uploads, require verified Cloudflare Access JWTs. No admin bearer is accepted
  as an identity credential; mutating requests additionally require same origin.
  `jose` verifies RS256 signature, issuer, audience, expiry,
  issue time and the configured human email. Missing configuration fails closed.
- License creation uses 128 random bits. Custom weak keys are rejected. The full
  key is returned ONCE; only its one-way hash is stored. `reveal-key` returns 410.
  Worker code no longer decrypts or creates recoverable license copies.
- Device signatures are mandatory. Raw legacy HWID matching is removed.
- D1 atomically consumes each signed auth nonce for five minutes. Replayed and
  concurrently duplicated requests cannot both succeed.
- Configs always require an active device-bound license session, even if an old
  `ALLOW_DEVICE_CONFIGS=true` variable remains on the deployment.
- Unknown/wrong-device/banned/expired license errors have one response format.
- Signed payloads carry a `kid`. Setting an explicit `SIGNING_KEY_ID` rejects
  no-kid/other-ID sessions; only the current public key is accepted. Never put a
  compromised key into a grace-period verification ring.
- Normal scripts no longer unlock the in-project legacy DPAPI vault, copy its
  admin/license credentials or create owner licenses during a health check.
  New operator storage is outside the repo/OneDrive, password-protected with
  PBKDF2-SHA256 + AES-256-GCM and restricted Windows ACLs.

## Operator cutover, in order

1. **Contain access first.** Use a trusted operator account/device, not an agent
   session. Enable Cloudflare Access and inspect existing apps/policies/IdPs.
   Create one application protecting `/admin` (and assets/subpaths) AND
   `/api/v1/admin` (all subpaths, including `/release`) on the Worker hostname.
   Both destinations must share the AUD used by the Worker. If using a Worker
   destination, public overrides must be limited to the required game auth,
   launcher, config and health routes; never bypass admin paths. Do not protect
   the entire game API accidentally. Allow only the stated email; configure
   independent MFA or identity-provider MFA and a 30-minute application session. Email OTP alone
   is not claimed to be MFA. Inspect policies before enabling; no broad Bypass.
2. Store the actual public `ADMIN_ACCESS_TEAM_DOMAIN` (HTTPS team origin) and
   `ADMIN_ACCESS_AUD` in `wrangler.jsonc` vars. Keep the verified email there too.
   `npm run deploy` refuses missing Access metadata; do not bypass the guard.
3. **Superseded for this rotation by the cloud-only storage requirement above.**
   Do not create a new local production-secrets vault. The following manual import
   instructions describe the previous recovery workflow only, not a required step:
   Save old admin/owner-license credentials separately in a password manager
   before removing any vault. To import production secret recovery material,
   run `scripts/Import-LegacyAuthVault.ps1 -AcknowledgeExposure` manually in a
   trusted PowerShell 7 session. It asks for an independent 16+ character
   passphrase; do NOT send it in chat, arguments, logs or environment variables.
   The default destination is `%LOCALAPPDATA%\OneStep\OperatorVault`.
   The script imports only `secrets`, not admin_token or owner_license. Source
   files stay untouched by default. Removal requires both
   `-RemoveLegacyAfterVerification -CredentialsSavedSeparately` plus confirmation.
   Importing into this Windows account is not isolation from an actively
   compromised account: use a separate operator account and keep it locked.
4. Back up D1 in a restricted operator location. Apply
   `migrations/0002_auth_hardening.sql` to the intended existing database. It adds
   nonce storage and irreversibly clears `key_encrypted` AFTER backup; hashes,
   license IDs, expiries and config owners remain unchanged. This migration was
   not executed remotely. Do not include private backups in Git or OneDrive.
5. Inspect `PRAGMA table_info(license_keys)`. Only if a historical `hwid` column
   exists, remove raw values with the transaction below. Already-hashed licenses
   retain their binding. Unmigrated raw-only licenses require reactivation;
   bans/expiry must not be lifted. Do not run against an unreviewed legacy schema.
6. **Completed with the operator's Access-only choice:** the active Worker has
   no `ADMIN_TOKEN_HASH` binding or separate bearer login. Do not create a new
   admin bearer. The hardened Worker and nonce migration are live. Wrong-user,
   invalid JWT, missing JWT, CSRF and replay checks passed in isolated tests;
   anonymous production requests were rejected by Access. The permitted human's
   first production login/MFA enrollment is still required before publication.
7. **Coordinated signing-key cutover:** generate a fresh P-256 pair outside the
   development account. Deploy the corresponding public key in new launcher/DLL
   builds via a trusted distribution channel. Atomically replace Worker private
   and public signing secrets together and set a fresh `SIGNING_KEY_ID`.
   Revoke old sessions/tickets as appropriate. Existing binaries pin the old
   public key and therefore require updating. Never promise uninterrupted old
   binaries after rotating a compromised trust key. Do not retain the old key
   for 45 days. A kid change alone does NOT revoke a stolen private key.
8. Plan KEY_PEPPER/HWID_PEPPER migration separately: blind replacement breaks
   stored hashes/config ownership. Do NOT change these values casually. Remove
   stale local PEMs and cloud version-history/backups only after verified recovery
   and rotation; deleting files alone cannot undo exposure.
   Any owner/customer license whose plaintext was exposed needs controlled
   reissue too; rotating the admin bearer does not invalidate stolen licenses.

### Conditional legacy-HWID cleanup (operator-reviewed schema only)

```sql
BEGIN TRANSACTION;
UPDATE license_keys
SET hwid_hash = NULL, hwid_hint = NULL, device_key_hash = NULL,
    status = CASE WHEN status IN ('banned','expired') THEN status ELSE 'unused' END,
    license_epoch = COALESCE(license_epoch, 1) + 1
WHERE hwid IS NOT NULL AND hwid <> ''
  AND (hwid_hash IS NULL OR length(hwid_hash) <> 64 OR hwid_hash GLOB '*[^0-9a-f]*');
UPDATE license_keys SET hwid = NULL WHERE hwid IS NOT NULL;
COMMIT;
```

## Focused checks (no production credentials / no Minecraft)

`npm test`, `npm run test:runtime`, `npm run test:bundle` in `server/`;
`python scripts/Test-AuthConfiguration.py` and
`scripts/Test-AuthVault.ps1` from the project root. Tests use synthetic keys and
isolated SQLite/JWKS; they do not run the audit's live-vault PoCs.

Native patching cannot be eliminated on an attacker-controlled machine. Local
integrity checks/obfuscation can only raise cost; they do not replace server
authorization or operator credential isolation.
