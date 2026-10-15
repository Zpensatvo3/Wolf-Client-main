# Thedorius Qt Quick UI

This folder contains the companion launcher/overlay introduced for the Qt 6
migration. It is deliberately separate from the injected JVM DLL: Qt is never
loaded into Minecraft.

## Current modes

- `Thedorius.exe --mock` uses production QML with local sample data.
- `Thedorius.exe --live` discovers Minecraft, extracts the embedded signed DLL,
  injects it with the normal Windows loader and connects to the local bridge.

The mock mode is the default while visual validation is in progress.

## Build prerequisites

- Qt 6.11.1 Commercial, statically built for MSVC 2022 x64
- CMake 3.24 or newer
- Ninja
- Visual Studio C++ build tools

Set `THEDORIUS_QT_ROOT` to the static Qt prefix, then run:

```powershell
.\scripts\Build-Thedorius.ps1 -Clean
```

For local UI development only, a shared Qt build can be used with
`-AllowSharedQt`; release packaging intentionally rejects it.

The final binary is written to `dist\Thedorius.exe`. QML, fonts, logo,
background and the already signed `Thedorius-Lite.dll` are compiled into that
executable.
