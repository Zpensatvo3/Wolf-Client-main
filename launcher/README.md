# Onestep Launcher

Windows x64 native launcher. Dark terminal-style UI, no browser runtime, no
installer, no administrator requirement and no VMProtect dependency.

## Use

1. Open Minecraft **Fabric 1.21.1** normally and reach the menu/world.
2. Run `build/launcher/Onestep-Launcher.exe` and enter a **client license**,
   not the admin credential.
3. After download verification, select the intended Minecraft session and
   click **Client’i başlat**. Multiple games require an explicit selection.

The launcher does not close/restart Minecraft. Loading is explicit, using the
ordinary Windows DLL loader; no driver, manual mapping or concealment is used.
It rejects another Windows user's process, non-x64 Java, non-Minecraft windows,
and games already containing `Onestep.dll`. If Minecraft runs elevated, start
both normally with the same user instead of automatically escalating privileges.
Once launched, the DLL maintains its own heartbeat; closing the launcher does
not end a valid client session. Closing during loading cancels pending work but
cannot undo an already-completed DLL load.

Keys are not saved to disk. Downloads are cached under
`%LOCALAPPDATA%\Onestep\releases\<sha256>\Onestep.dll`; incomplete `.part` files
are removed. Every launch rechecks the hash while holding a write-excluding
file handle. Old cached versions are not launched without a fresh server grant.
No PDB/MAP, admin helper or `.credentials` file belongs in user distribution.

## Protocol

- Existing CNG-device-bound license login and signed session validation.
- Private POST manifest/download endpoints; requests include device proofs.
- ECDSA-signed manifest binds version, SHA-256, size and expiry. HTTPS redirects
  are disabled on credential-bearing requests. Incomplete/oversized/hash-mismatched
  downloads are never loaded.
- D1 stores only a hash of each random 256-bit launch ticket. Tickets expire
  after 120 seconds and bind license epoch, HWID, CNG key, process ID and release.
- The local handoff pipe has a current-user/System-only DACL, first-instance
  protection, remote clients disabled and client-PID verification.
- The DLL redeems the ticket with a fresh device signature and its own file
  hash/process ID. Redemption consumes it atomically and returns a signed normal
  session. Only successful verification opens module access.
- Existing direct-injection login remains compatible; the launcher does not
  replace server verification with a local boolean. Local user-owned processes
  are not a security boundary against an attacker controlling that same account.

This is access control and authenticated delivery, not unbreakable DRM or a
source-code vault. A licensed user can copy the downloaded DLL; native code can
be analyzed. Keep the source repository private and do not ship private keys.

## Build / publish

```powershell
.\scripts\Build-Launcher.ps1
# Explicit publication; requires PowerShell 7 and the local encrypted admin vault:
.\scripts\Build-Launcher.ps1 -Publish -Version '2026.10.16-launcher.3'
```

Build outputs are isolated in `build/launcher` and `build/launcher-client`.
Production auth is mandatory in the build script. Existing DLL output folders
are not replaced. Every published binary needs a unique version identifier.

Cloudflare uses private KV `ONESTEP_RELEASES` because R2 is not enabled on this
account. The publisher caps DLLs at 24 MiB and streams/hash-checks uploads.
Only the admin endpoint can publish. KV propagation may take about a minute;
retry if a new manifest/object is not yet available in an edge location.
The access grants are in strongly consistent D1, not KV. Nothing was publicly
uploaded to GitHub and no paid storage service was enabled.

## Verification

`npm test` in `server` runs the small offline suite. An operator can build the
launcher project with `LauncherProbe=true`, a separate `TargetName`, `OutDir`
and `IntDir`, then run `scripts/Test-LauncherDeployment.ps1 -Probe <exe>`.
It uses/deletes a temporary license without touching the owner's license and
tests native login, signed manifest, real private download, file hash, ticket
redemption and replay rejection. `--ipc-only` checks the local pipe without a
license or network call. Neither probe loads anything into Minecraft.

The GUI's `--preview <absolute.png>` option renders the real drawing code into
a PNG without opening the UI or connecting to the service. Game loading and
in-game module behavior are left for the owner's interactive test.
