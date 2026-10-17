# Minecraft 26.x port: staged 26.1 native adapter

Status: **26.1 native SDK batch implemented, not an enabled or supported DLL target**.
All 57 module registrations, config IDs and gameplay decisions remain intact.
Gameplay modules have not been converted to Java. Complete 26.1 integration before
starting the later exact releases; do not treat `26.x` as one compatible ABI.

## Implemented native batch

- `src/platform/adapter/runtime_names.hpp` selects explicit compile-time names
  at binding sites. It is an identity macro for **all four 1.21.x targets** and
  never changes generic JNI lookup/caching or external Java/library names.
- `src/sdk/minecraft/fabric_26_1/official_names.hpp` contains 667 generated,
  explicit spelling entries, not copied SDK/module implementations or runtime
  fuzzy lookups. Historical declarations retained for older SDK branches use
  their verified historical spellings; **667 is not a count of working 26.1 APIs**.
  Missing table entries fail compilation instead of using intermediary names.
- Native SDK classes, member names, raw JNI descriptors and native transformer
  class lookup sites now use the selected spelling. Existing Java reflection and
  ASM sources are **not yet ported**; native class lookup alone is insufficient.
- Inventory actions use `handleContainerInput` and the game's actual named
  `ContainerInput.PICKUP`, `QUICK_MOVE` and `SWAP` enum fields. No custom packet
  IDs, gameplay decision rules or config settings were changed.
- Camera FOV is read through `Camera.getFov()`, without attempting the removed
  GameRenderer call on 26.1.
- Item extraction reads relocated quad/transform classes, the quad's nested
  `MaterialInfo` sprite/tint metadata and the layer's bounded `IntList` tints.
  It applies the layer's local matrix **before** the existing GUI centering and
  item rotation. This order was checked against official 26.1 class bytecode
  and the decompiled transform bodies, not guessed from the field name.
- The exact ray-query method is private static in 1.21.11 and 26.1. The staged
  1.21.11 binding's previous instance declaration was corrected as well; 1.21.1,
  1.21.4 and 1.21.8 retain their existing renderer-based query path.
- 91 critical compiled 26.1 bindings match actual official classfiles, including
  owner hierarchy, descriptors, constructors and static flags. This is **not**
  proof that every remaining SDK declaration, module or game behavior is valid.

### Explicit safety gates

The SDK target ID is `26100` and is accepted **only** with
`ONESTEP_MINECRAFT_PORTING=1` for offline compilation. Normal 1.21.x builds still
recognize only their existing four versions. `src/app/runtime.cpp` has an
additional compile-time rejection of 26.1 application builds, even with the
porting define. Neither MSBuild, the Release script nor the Windows CI matrix
offers a 26.1 DLL yet. Do not remove these gates before Java bridge and application
integration are complete; bypassing them does not produce a compatible client.

### Tests for this batch

```sh
# No game fixtures, downloads, JVM or Windows required (C++20 compiler needed):
python scripts/Test-Minecraft261Offline.py
python -m unittest discover -s tests -p test_minecraft26_readiness.py -v
# Compare actual staged SDK metadata to the verified official 26.1 jar:
python scripts/Test-Minecraft26Readiness.py --cache-dir /path/to/cache --native-261 --output /path/to/native-261.json
```

Unlike discovery mode, `--native-261` returns a failure if **any** selected native
binding is absent or has the wrong static flag. Success still does not enable a
DLL target. `tests/minecraft261_sdk_test.cpp` checks exact version rejection,
Mojang descriptors and finite/bounded affine item transforms. Native adapter,
item-reader and version-manager translation units plus private Chams/Sky/Vape
binding headers are syntax-checked. The normal 35-case offline suite remains
separate and unchanged.

To reproduce the generated spelling table, provide local `client-<version>.txt`
Mojang ProGuard mappings and `intermediary-<version>.jar` Fabric v2 mappings for
1.21.1, 1.21.4, 1.21.8 and 1.21.11 in one external cache, then run:

```sh
python scripts/Generate-Minecraft261Names.py --cache-dir /path/to/mapping-cache
# Only when intentionally updating the generated header:
python scripts/Generate-Minecraft261Names.py --cache-dir /path/to/mapping-cache --write
```

No game jars, complete vendor mappings, compiler dependencies or secrets are
committed. The first command is a reproducibility check, not an implicit rewrite.

### Remaining before enabling 26.1

Audit/update existing NameProtect/Sky reflection, font/fog transformations, Fabric
render callbacks and shader uniforms for Mojang names and Java 25; rebuild their
embedded class resources and test both old and new contracts. These are existing
integration bridges, **not a migration of gameplay decisions into Java**. Complete
the wider SDK/active-call audit, Windows MSVC build/link validation and the game
checklist before claiming working modules or in-game visual compatibility.

## Verified public release fixtures

Inspected on 2026-10-17 using Mojang's version manifest and official client jars,
with size and SHA-1 checks against the manifest's download metadata. The fixtures
stay in an external development cache, not the repository or shipping DLL.

| Release | Java / classfile | Baseline signatures present | Unmatched baseline bindings |
| --- | --- | --- | --- |
| 26.1 | 25 / 69 | 69 / 80 | 11 |
| 26.1.1 | 25 / 69 | 69 / 80 | 11 |
| 26.1.2 | 25 / 69 | 69 / 80 | 11 |
| 26.2 | 25 / 69 | 69 / 80 | 11 |
| 26.3 | 25 / 69 | 66 / 80 | 14 |

These counts cover **only the 80 critical JNI declarations** currently emitted by
`tests/minecraft_bindings_dump.cpp` for the 1.21.11 target. They are not a full SDK,
Java bridge, loader, renderer, behavioral or gameplay audit. An exact signature
match does not imply unchanged semantics. A missing baseline path may mean a
moved class rather than removed functionality. No 26.x game or Windows DLL was run.

26.x jars use Mojang names (for example `net/minecraft/client/Minecraft`), not the
old intermediary `net/minecraft/class_310` namespace. Fabric's Yarn API returns no
26.1 Yarn version, and the intermediary API returns the identity version `0.0.0`.
Do not reuse 1.21.11 names or generate a fake 26.x Yarn profile.

## Confirmed migration boundaries

- **Namespace:** migrate all active class/member descriptors, including raw JNI
  lookups and Java reflection/ASM targets outside the SDK. Do not change global
  JNI resolution for the existing 1.21.x targets as a shortcut.
- **Inventory:** `MultiPlayerGameMode.handleInventoryMouseClick(..., ClickType,
  Player)` no longer matches. The 26.1 jar declares `handleContainerInput(...,
  ContainerInput, Player)`. Verify enum meanings, packet behavior and every
  inventory module before replacing this call; a name substitution is insufficient.
- **FOV:** the old `GameRenderer.getFov(Camera, float, boolean)` is absent. The
  26.1 camera exposes `getFov()` and additional FOV update/calculation methods.
  Verify timing and projection semantics before adapting native overlays.
- **Item models:** `BakedQuad` moved under `client/resources/model/geometry` and
  `ItemTransform` under `client/resources/model/cuboid`. The layer's old
  `transform` field no longer matches; `itemTransform` and `localTransform` exist.
  Tint storage is `IntList`, not `int[]`. Preserve existing icon projection,
  shading and transforms while adapting extraction.
- **26.3 rendering:** texture types move from `com/mojang/blaze3d` to
  `com/mojang/renderpearl`; the old GL class path no longer matches. Layer quads
  use `ItemQuads` rather than the earlier list preparation method. Audit the
  render backend, OpenGL hook and GPU reads separately; library presence alone
  does not establish that a backend is selected or compatible.
- **Bridges:** audit NameProtect entry points, Sky callbacks, fog data and ASM
  classfile-69 handling against actual 26.x Fabric dependencies. Existing
  1.21.x transformer counts are not proof of 26.x compatibility.
- **Runtime:** Java 25 is required by the official releases. This does not mean
  gameplay logic must move from C++ into Java. Validate native attach/JVMTI,
  classloader lifecycle and unload/reload on that runtime.

## Reproduce the audit

Requires Python 3.11+ and a C++20 compiler, but no running JVM or Minecraft.
Use an external cache/report path, not production resources:

```sh
python scripts/Test-Minecraft26Readiness.py --cache-dir /path/to/cache --download --output /path/to/26-readiness.json
# Later: same verified fixtures, no network access
python scripts/Test-Minecraft26Readiness.py --cache-dir /path/to/cache --output /path/to/26-readiness.json
# Synthetic regression tests, with no fixtures or network
python -m unittest discover -s tests -p test_minecraft26_readiness.py -v
```

`--target 26.1` restricts the audit; repeat the option for additional exact releases.
The tool compiles the **existing 1.21.11** binding dump with owner-qualified names,
joins Fabric intermediary and Mojang ProGuard mappings, verifies each source
binding against the actual baseline class hierarchy, and compares named members
against the target jar's classes, superclasses and interfaces. Constructors are
not inherited; method/field kinds, descriptors and static flags are checked.
Unresolved or ambiguous baseline mappings fail the audit, not silently disappear.
Possible class relocations are hints only, never automatic runtime mappings.

Exit zero means the report was successfully generated, **not that a 26.x target
is ready**. JSON explicitly includes `supported: false`. No build targets, DLLs,
launcher manifests or remote resources are generated or changed by this tool.

## Implementation and acceptance order

1. Implement an isolated Mojang-named 26.1 SDK/profile and raw-JNI adaptation;
   preserve shared C++ module decisions/config IDs and the existing 1.21.x SDK.
2. Resolve inventory, FOV, item models and version-specific Java bridge bindings.
   Test actual classfiles plus representative enum/record semantics, not just
   symbol spellings. Do not skip broken modules to declare the target working.
3. Enable an exact DLL target only when its native source/bridge checks pass.
   Keep an explicit runtime version check; do not allow a 26.x wildcard fallback.
4. Build with Windows MSVC x64, then test all 57 modules and the existing game
   checklist in `MINECRAFT_TARGETS.md` on 26.1 with Java 25.
5. Repeat for 26.1.1, 26.1.2, 26.2 and the additional 26.3 render differences.

No automatic deployment, Git push, license/auth change, or module Java rewrite
is part of this native port batch.
