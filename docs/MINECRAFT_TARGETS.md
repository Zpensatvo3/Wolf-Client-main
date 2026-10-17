# Minecraft targets

OneStep uses **one source tree and a separate Windows x64 DLL per exact Fabric version**.
The current targets are 1.21.1, 1.21.4, 1.21.8 and 1.21.11. These are development
targets until their Windows build and in-game checklist have passed; a successful
offline signature check does not establish gameplay compatibility.

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
- `src/bridge/`: version-scoped NameProtect and Sky integration. Other gameplay
  modules, including Click Aim Assist, remain native C++.

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
# Or build all four sequentially:
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

Each file keeps the `Onestep.dll` filename, preserving the launcher's loaded-module
identity. The default 1.21.1 path, release artifact name and signed release protocol
are unchanged. The launcher is **not** yet a multi-version release selector; choose
the matching artifact explicitly. No production manifest, release, credential, API
or Cloudflare resource is changed by these build targets.

The checked-in Java class resources must be rebuilt when bridge sources change:

```powershell
.\scripts\Build-NameProtectBridge.ps1 -AsmClasspath 'C:\libs\asm.jar;C:\libs\asm-tree.jar'
.\scripts\Build-SkyBridge.ps1 -AsmClasspath 'C:\libs\asm.jar;C:\libs\asm-tree.jar'
```

These use Java 17 bytecode and the ASM already supplied by Fabric. Minecraft's
existing Java/JOML/Fabric libraries remain runtime requirements; no extra client mod
or frontend framework is introduced.

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
- NameProtect verifies seven/six/eight/eight font entry points for the four targets.
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

`tests/BridgeCompatibilityTest.java` inspects local official client jars and Fabric
intermediary v2 jars without starting Minecraft. Compile it alongside bridge sources
with ASM core/tree/commons 9.9 and JOML 1.10.8 on the test classpath, then run:

```sh
java -ea -cp '<compiled-classes>:<test-libraries>' BridgeCompatibilityTest client-1.21.11.jar intermediary-1.21.11.jar 12111
```

Repeat with target IDs `12101`, `12104`, `12108`, `12111`. Windows classpaths use `;`.
The test remaps only the inspected fixtures in memory, checks real font/fog/render
contracts, exercises the transformations, and checks fog passthrough and immutability.
Client jars/mappings and test libraries are not committed or bundled into the DLL.

`tests/jni_owner_jvm_test.cpp` is a separate, optional integration test. Link it
against the JDK's `jvm` library and run it with that library available. It starts a
disposable JVM with `-Xcheck:jni` and exercises identical constructor, method, field
and fallback signatures on ArrayList/HashMap. It never attaches to Minecraft.
The ordinary offline test runner still does not start a JVM.

## Required game checks before claiming full support

For **each** target, complete an MSVC Release x64 build and test all 57 modules on
that exact Fabric version. Check targeting/cooldown/criticals, physical clicks and
manual input, weapon swaps, inventory/equipment operations, world transitions,
reload/shutdown, NameProtect, Chams animations, all Target HUD equipment icons,
player heads, Sky presets/fog and Iris behavior. Exercise special item models and
resource packs, which may not expose ordinary baked quads. Confirm that injecting
a DLL built for another target stops before hooks or modules start.

The existing config keys, module IDs and auth protocol are shared across targets.
This does not assert that game behavior or every resource-pack model is identical
between Minecraft versions. Unsupported versions (including 26.x) require an
explicit additional profile and validation; they never use an approximate fallback.

The staged 26.1 native adapter, classfile checks and remaining integration
boundaries are documented in [MINECRAFT_26_PORT.md](MINECRAFT_26_PORT.md). It is
offline-only and does not enable 26.x DLLs.
