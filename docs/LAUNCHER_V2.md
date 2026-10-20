# Launcher-only protocol v2 — 20 October 2026

## Production status

Live Worker version: `b2daf1f6-1253-44ef-9cc7-d3a814fc7d37`.
Deployment: `1ac3e381-a443-4f82-8157-342893fe0981`.
Release: `launcher-v2.1-20261020-1.21.11`, Windows x64 / Fabric 1.21.11.
Exact binary hashes and sizes are recorded in `server/deployment.json`.

The owner uploaded the new DLL from their MFA-authenticated Edge admin panel.
The initial v2 approval was checked before `AUTH_V2_ONLY=true` was deployed.
The final v2.1 upload was subsequently verified against the final local SHA-256
and byte count; its D1 channel is active and the superseded v2.0 release is
revoked, with an audit record. No browser token was extracted and no
new Cloudflare API credential or private signing key was stored locally.

`0003_launcher_v2.sql` was applied additively after cloud Time Travel bookmark
`00000047-00000000-0000510a-d8ed581fe3e80699c9f326bda427d287`.
All FIVE pre-existing license records and ZERO public configs were preserved.
The current signing key, Access owner policy and MFA were not changed.

## API and trust boundaries

All v2 service endpoints use POST and bounded JSON:

- `/api/v2/launcher/login`: device-bound activation; launcher audience only.
- `/api/v2/launcher/manifest`, `/download`, `/ticket`: launcher audience only.
- `/api/v2/runtime/redeem`: one-use, 60-second ticket to a runtime lease.
- `/api/v2/runtime/renew`: runtime audience only, atomic increasing sequence.
- Existing `/api/v1/configs/*`: valid current v2 runtime lease required.
- Existing `/api/v1/auth` and `/api/v1/launcher/*`: HTTP 410; no fallback.
- `/api/v1/admin/revoke-release`: owner Access/MFA + same-origin POST with
  `{ "sha256": "<approved release SHA-256>" }`; revokes the release and its
  runtime sessions, removes channel pointers, and records an admin audit event.

Audiences are `onestep-launcher-v2`, `onestep-runtime-v2`, and
`onestep-release-v2`. They are never interchangeable. Runtime bindings include
license epoch, device key hash, HWID hash, release hash/version, Minecraft target,
PID, random per-load process nonce, session ID and sequence. D1 is authoritative
for releases and runtime sessions. KV stores the DLL bytes; the historical KV
`current` record is not used for v2 authorization.

Device proof signs the exact UTF-8 string joined with newline characters:

```
onestep-device-v2
POST
<path>
<request_type>
<hwid>
<nonce>
<timestamp as decimal>
<SHA256 of key / ticket / session_token>
<SHA256 of fields>
```

`fields` is an exact JSON string, not a reconstructed object. The server parses
it only after proof verification. This binds every operation-specific value
without cross-language JSON canonicalization ambiguity. The signed response
includes the request-message hash, nonce hash, protocol 2, audience, key ID,
expiry and appropriate runtime bindings/sequence. No permanent license key is
included in the local message-mode named-pipe handoff.

## Native behavior and failure policy

The DLL has no license input or activation path. A spoofed UI `authorized` state
does not create execution authority. No public session setter exists. A private
immutable verified snapshot provides distinct module/input/bridge/render/config
capabilities; launcher sessions have none of those capabilities. Authorization
denial is checked before Java/native fallback selection. Cleanup is not blocked
by expired authorization.

Runtime leases last at most 60 seconds and renew every 18–22 seconds. Control
requests and the streaming download have a five-second total request watchdog.
The first failed renewal revokes the lease without retry or offline grace;
disconnect detection occurs on the next scheduled check, not instantaneously.
Monotonic deadlines are capped by request-start time plus signed TTL, so UTC
skew or request latency cannot extend a lease beyond that bound. Cancellation
generations reject late successes. Background config requests and renewal are
serialized to avoid a strict-sequence race; no render-thread network lock is
introduced. Login UI state cannot reactivate a revoked runtime.

The module loop exits on permission loss. Owned queued automatic input is
cancelled and module/Java cleanup runs through the existing render shutdown
path; user-held physical keys are preserved. Minecraft is never closed or
restarted. The user must start again through the launcher after a failure.
The launcher itself may close after a successful launch; the DLL owns renewal.

## Checks and rollout

Focused checks cover real SQLite and workerd/D1 concurrent redemption/renewal,
role substitution, modified fields/path, release revocation before redemption,
signed native leases, expiry/duration, sequence replay, late completion,
spoofed UI state, message-mode local IPC, and the compact status-only screen.
Existing server regression tests also passed. Test keys are disposable synthetic
fixtures, never production credentials. The optional operator probe is not part
of user distribution and does not inspect/inject into Minecraft.

No licensed production launcher-to-runtime round-trip or in-game verification
has been claimed. Those remain user acceptance checks. A cracked DLL sample has
not been supplied, so the exact reported patch is not yet attributed.

The GitHub publication workflow now selects a successful build/run and verifies
an operator-specified digest; it only creates a binary artifact for protected
admin upload, not an unauthenticated DLL release. Distribute only the new
launcher/DLL pair. No source, PDB, MAP, probes or vault files belong in packages.

Never roll back to a v1/bearer-login Worker. A future rollback must preserve the
v2 audience/lease guards and an explicitly approved compatible binary. The
initial coexistence version is historical and is NOT an acceptable final
rollback target. Local secret-file cleanup, pepper migration and exposed-key
reissue remain separate outstanding incident tasks.

This raises the cost of specific bypasses; it is not hardware attestation,
anti-dump protection, or proof that client code cannot be modified. No driver,
stealth loader, debugger/AV disabling, paid protector or fake encryption was added.
