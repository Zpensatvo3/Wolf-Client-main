# OneStep Launcher v2

Windows x64 native launcher for the current **Fabric 1.21.11** release. No
administrator requirement, browser runtime, driver or VMProtect dependency.

## Use

1. Open Minecraft Fabric 1.21.11 normally.
2. Run `build/launcher-v2/launcher/Onestep-Launcher.exe` and enter a client license.
3. Wait for the signed manifest and verified DLL download, select the intended
   Minecraft window, and choose Launch Client.

DLL-only license login is removed. The launcher uses ordinary explicit Windows
DLL loading, not a manual mapper or hidden loader. It never closes or restarts
Minecraft. The launcher may close after successful loading; the DLL owns renewal.

Runtime permission lasts at most 60 seconds and renews every 18–22 seconds.
The first failed renewal stops the client modules, not Minecraft. There is no
offline grace or automatic reopening after a late response. Start again from
the launcher after a failure. A five-second request watchdog also applies to the
streaming download: very slow downloads can fail rather than extend that limit.

Client licenses are not saved by the launcher. A persistent non-exportable
Windows CNG device key proves possession; the server signing key remains only
in Cloudflare. Public verification keys embedded in binaries are not secrets.
Downloaded DLLs are cached under
`%LOCALAPPDATA%/Onestep/releases/<sha256>/Onestep.dll`. A cached file still needs
a valid launcher session, approved release and fresh one-use runtime ticket.
Hashes reported by client code are not hardware attestation.

## Build and package

`scripts/Build-Launcher.ps1` now builds a 1.21.11 DLL and launcher.
The current isolated verified outputs are under `build/launcher-v2/`.
`scripts/Package-LauncherRelease.ps1` creates a ZIP containing exactly the
launcher and DLL, refusing an existing output and rejecting extra archive entries.
Do not ship source, MAP/PDB, local vaults or the optional test probe.

Publication is through the MFA-protected admin DLL upload form. The GitHub
workflow prepares a selected hash-verified artifact; it does not create a public
unauthenticated DLL download. Both binaries must be updated together.

The protocol, deployment record and remaining incident tasks are documented in
`docs/LAUNCHER_V2.md` and `server/deployment.json`. Old v1 entrypoints return
HTTP 410. Do not restore direct DLL login as a rollback workaround.
