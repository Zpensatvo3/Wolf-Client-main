# Minecraft targets and builds

OneStep uses **one source tree and a separate Windows x64 DLL per exact Fabric version**.
The current targets are 1.21.1, 1.21.4, 1.21.8, 1.21.11, 26.1, 26.1.1 and 26.1.2. These are development
targets until their Windows build and in-game checklist have passed; a successful
offline signature check does not establish gameplay compatibility.

This is the canonical target/build guide. The
[26.x audit](MINECRAFT_26_PORT.md) retains version-specific evidence and fixture
pins; [hybrid migration](HYBRID_MIGRATION.md) describes the 1.21.11 Java I/O
exceptions. The catalog currently has 61 modules; historical audit counts are
dated results, not the current test inventory.

## Architecture

- `src/platform/adapter/target_version.hpp`: supported targets, exact version parsing
  and the binary's immutable target (`ONESTEP_MINECRAFT_VERSION`).
- `src/platform/adapter/bindings_profile.hpp`: only JNI names/types that differ.
- `src/sdk/minecraft/common/`: shared JNI wrappers and normalized accessors for
  equipment, movement input, textures and item data. No copied module trees.
- `src/platform/adapter/game_adapter.*`: shared game operations and version-specific
  crosshair queries. Unknown versions cannot select the 1.21.1 adapter implicitly.
- `src/platform/adapter/item_model.*`: reads legacy baked models or modern item
  render states into the same native mesh representation. HUD drawing remains C++.
- `src/platform/jni/member_id_cache.hpp`: JNI IDs are cached per binding **and
  owning class**, with two hot owners per thread. Identical record/enum methods,
  fields and constructors on unrelated classes cannot share an incompatible ID.
  JNI resolution runs outside the cache lock to allow Java class initialization.
- `src/bridge/`: version-scoped NameProtect/Sky/View Model integration and the
  documented 1.21.11 hybrid I/O groups. C++ still owns module decisions/UI;
  this is not a conversion of all modules to Java.

The runtime reads the Minecraft mod version from Fabric Loader metadata before
registering modules, initializing auth, or installing rendering hooks. An unknown
version, a prerelease, or a version different from the DLL target stops initialization
with an explanation. Window titles are not a source of version compatibility.

## Build

In a Windows Developer PowerShell with the existing auth build properties configured:

```powershell
.\scripts\Build-Release.ps1 -MinecraftVersion 1.21.1 -SkipSign
.\scripts\Build-Release.ps1 -MinecraftVersion 1.21.4 -SkipSign
.\scripts\Build-Release.ps1 -MinecraftVersion 1.21.8 -SkipSign
.\scripts\Build-Release.ps1 -MinecraftVersion 1.21.11 -SkipSign
.\scripts\Build-Release.ps1 -MinecraftVersion 26.1 -SkipSign
.\scripts\Build-Release.ps1 -MinecraftVersion 26.1.1 -SkipSign
.\scripts\Build-Release.ps1 -MinecraftVersion 26.1.2 -SkipSign
# Or build all seven sequentially:
.\scripts\Build-Release.ps1 -AllMinecraftVersions -SkipSign
```

MSBuild also accepts `/p:MinecraftVersion=1.21.8`. Both Debug and Release isolate
their intermediate objects/PCHs by target version. Invalid target values fail the build.

| Fabric | DLL | Yarn reference |
| --- | --- | --- |
| 1.21.1 | `build/Onestep.dll` | `1.21.1+build.3` |
| 1.21.4 | `build/minecraft/1.21.4/Onestep.dll` | `1.21.4+build.8` |
| 1.21.8 | `build/minecraft/1.21.8/Onestep.dll` | `1.21.8+build.1` |
| 1.21.11 | `build/minecraft/1.21.11/Onestep.dll` | `1.21.11+build.6` |
| 26.1 | `build/minecraft/26.1/Onestep.dll` | Mojang names; Java 25 required |
| 26.1.1 | `build/minecraft/26.1.1/Onestep.dll` | Shared 26.1 profile; Java 25 required |
| 26.1.2 | `build/minecraft/26.1.2/Onestep.dll` | Shared 26.1 profile; Java 25 required |

The three exact 26.1 targets share an audited SDK/bridge layout, not separate
module implementations. Both patch jars passed all 740 SDK/JNI checks; 274
referenced client classes (including parents/interfaces) were byte-identical to
26.1. Each binary still accepts only its own exact game version, not the family.

Each file keeps the `Onestep.dll` filename, preserving the launcher's loaded-module
identity. The default 1.21.1 path, release artifact name and signed release protocol
are unchanged. The launcher is **not** a multi-version release selector. The
recorded v2 release targets 1.21.11; a development artifact is not automatically
an approved production release. See the [launcher guide](../launcher/README.md).
No production manifest, release, credential, API or Cloudflare resource is
changed by adding these build targets.

The checked-in Java class resources must be rebuilt when bridge sources change:

```powershell
.\scripts\Build-NameProtectBridge.ps1 -AsmClasspath 'C:\libs\asm.jar;C:\libs\asm-tree.jar'
.\scripts\Build-SkyBridge.ps1 -AsmClasspath 'C:\libs\asm.jar;C:\libs\asm-tree.jar'
.\scripts\Build-ViewModelBridge.ps1 -AsmClasspath 'C:\libs\asm.jar;C:\libs\asm-tree.jar'
```

These use Java 17 bytecode and the ASM already supplied by Fabric. Minecraft's
existing Java/JOML/Fabric libraries remain runtime requirements; no extra client mod
or frontend framework is introduced.

Rebuild the six hybrid classes with `scripts/Build-GameBridge.ps1` when their
Java sources change. Those bridges use JDK reflection rather than ASM.
Optional code signing still looks up the historical
`CN=Thedorius Lite Test` certificate in the user's certificate store; that name
has not been cosmetically renamed. Protector/device-key limitations are
documented in [AUTH_HARDENING](../server/AUTH_HARDENING.md#vmprotect-isteğe-bağlı-doğrulanmış-koruma-iddiası-yok).

## Actual differences handled

- FOV returns float after 1.21.1; the render tick counter field changes in 1.21.4.
- Jump input becomes a `PlayerInput` record; movement axes later become a vector.
- `fallDistance` becomes double, and sword classification uses the sword tag when
  the old `SwordItem` class no longer exists.
- Armor/offhand access uses the inventory equipment contract after the old lists
  disappear. Inventory actions still go through vanilla interaction methods.
- Texture IDs come from `GpuTexture`/`GlTexture`; 1.21.11 skins use texture assets.
- 1.21.11 authlib profiles use `name()`/`id()` record accessors.
- Fresh crosshair queries move to `ClientPlayerEntity` in 1.21.11.
- Chams uses an owned player render state after 1.21.1, without modifying the
  renderer's shared state or replacing the game's renderer.
- Item icons read GUI render layers, tint arrays, changed transformations and
  packed UVs, then retain the HUD's existing native projection/drawing.
- NameProtect verifies seven/six/eight/eight/eight/eight/eight font entry points
  for the seven targets in the order listed above.
- Sky reads legacy fog uniforms, the 1.21.4 Fog record, or newer GPU uniform slices.
  1.21.11 uses the new Fabric rendering package and per-frame sky state; fog color
  is filtered at a verified return site. The fog helper lives in the game's loader,
  contains no native methods and is disabled on shutdown. Its separate JVMTI
  environment removes its own transformation when shutting down.

## Offline validation

```sh
python scripts/Test-Infrastructure.py
python scripts/Test-Offline.py --jobs 2
python scripts/Test-Offline.py --test minecraft_version --test sdk_mappings --minecraft-version 1.21.4 --jobs 2
python scripts/Test-Offline.py --test minecraft_version --test sdk_mappings --minecraft-version 1.21.8 --jobs 2
python scripts/Test-Offline.py --test minecraft_version --test sdk_mappings --minecraft-version 1.21.11 --jobs 2
```

`scripts/Test-MinecraftMappings.py --mapping-dir <cache>` compiles the real wrappers
for each target and compares critical JNI names/descriptors with local Yarn v2 jars
named `yarn-1.21.1.jar`, `yarn-1.21.4.jar`, etc. Obtain them from Fabric Maven using
the pinned Yarn references above and the `v2` classifier. This verifies symbol and
descriptor availability, not Java inheritance or runtime behavior.

`scripts/Test-MinecraftBridges.py` compiles the bridge tests and checks both source
and embedded resources against local official clients, Fabric intermediary v2
mappings and Fabric rendering jars. It needs ASM 9.9 core/tree/commons/analysis,
JOML 1.10.8 and a JDK (21 for the old targets, 25 for the three 26.1 targets):

```sh
python scripts/Test-MinecraftBridges.py --fixture-dir /path/to/client-cache --library-dir /path/to/test-libraries --java-home /path/to/jdk25
```

Repeat `--target 1.21.11` (or another exact version) to limit the run. The default
checks all seven versions in both source and embedded modes (14 runs). Tests inspect
real font/fog/render contracts, analyze the transformations and exercise inert
JVM fixtures for text filtering, fog outputs and Sky event/stop behavior. They
never run Minecraft or native rendering. Pinned rendering jar versions and the
required fixture filenames are listed in [MINECRAFT_26_PORT.md](MINECRAFT_26_PORT.md).
Client jars, mappings and test libraries are not committed or bundled into the DLL.

`tests/jni_owner_jvm_test.cpp` is a separate, optional integration test. Link it
against the JDK's `jvm` library and run it with that library available. It starts a
disposable JVM with `-Xcheck:jni` and exercises identical constructor, method, field
and fallback signatures on ArrayList/HashMap. It never attaches to Minecraft.
The ordinary offline test runner still does not start a JVM.

## Required game checks before claiming full support

For **each** target, complete an MSVC Release x64 build and test all registered modules on
that exact Fabric version. Check targeting/cooldown/criticals, physical clicks and
manual input, weapon swaps, inventory/equipment operations, world transitions,
reload/shutdown, NameProtect, Chams animations, all Target HUD equipment icons,
player heads, Sky presets/fog and Iris behavior. Exercise special item models and
resource packs, which may not expose ordinary baked quads. Confirm that injecting
a DLL built for another target stops before hooks or modules start.

The existing config keys, module IDs and auth protocol are shared across targets.
This does not assert that game behavior or every resource-pack model is identical
between Minecraft versions. Unsupported versions (including 26.1.3 and later) require
explicit additional validation and an exact target; they never use an approximate fallback.

The 26.1 SDK/JNI audit, NameProtect/Sky bridges and exact offline test results are
documented in [MINECRAFT_26_PORT.md](MINECRAFT_26_PORT.md). The three exact 26.1 build
targets are enabled for manual Windows validation; their Windows builds and game
checklists have not been run in this Linux environment.
