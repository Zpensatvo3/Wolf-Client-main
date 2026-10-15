# Thedorius Lite

Native C++ client project targeting Minecraft Fabric 1.21.1 on Windows.

## Build

Release x64 incremental build and test signing:

```powershell
.\scripts\Build-Release.ps1
```

ImGui loader/injector ile tek EXE paketi:

```powershell
.\scripts\Build-Launcher.ps1
```

Clean rebuild:

```powershell
.\scripts\Build-Release.ps1 -Clean
```

The output is written to `build\Thedorius-Lite.dll`. The script signs it when
the `CN=Thedorius Lite Test` certificate is available in the current user's
certificate store. Use `-SkipSign` when only a local compile check is needed.

Infrastructure validation:

```powershell
.\scripts\Test-Infrastructure.ps1
```

Refresh Visual Studio filters after changing project items:

```powershell
python .\scripts\Refresh-ProjectFilters.py
```

## Source layout

```text
src/
  cache/                  Minecraft object snapshot used by module ticks
  configs/                Public config network client
  friends/                Session-only friend state and server-player scan
  gui/                    ImGui management shell, widgets and local UI state
  ui/                     Grim workspace, HUD editor and config library screens
  modules/
    client/               Click GUI and Panic
    combat/               Combat modules and their focused engines
    misc/                 Player, movement and world utility modules
    render/               Watermark and ArrayList
    visual/               ESP, radar, nametag and brightness modules
    vape/                 Independent user-supplied engine adapters (Vape UI page)
    friends/              Friends module declaration
    core/                 Module contract, registry and config serialization
  platform/jni/           JNI thread context and client-thread dispatcher
  services/
    game/                 Client context, hotbar, inventory, target and attack
    input/                Tagged Windows input coordination
```

Gameplay modules use focused implementations; the Vape family shares typed
IO adapters around independent engines. `modules.hpp` is only a compatibility umbrella.
The registry is populated through the validated descriptor table in
`module_catalog.cpp`.

Visual / Chams provides a model-based colored overlay using independent baked
wide/slim player models and vanilla limb/pose animation. It does not hook shaders,
mutate shared entity models or change Glow ESP. Visibility/color selection is
entity-level line of sight, not per-pixel world-depth masking. Skin outer layers,
armor, capes and special root poses (elytra, swimming, sleeping, mounts) are not
rendered by this first version. Its settings use the standard config visitor.
Release compilation succeeded; controlled in-game appearance remains unverified.

Triggerbot separates a native-free decision model, staged config migration,
client-thread game adaptation and tagged input tracking. Its automatic attacks
coordinate with Shield Breaker, Mace Swap and Left Clicker. Static checks and
pending offline/native scenarios are documented in `tests/TRIGGERBOT.md`.

The Configs workspace uses a searchable library and a responsive details/editor
panel. Publishing errors keep the draft intact; deletion requires confirmation.
Favourites are session-only. Offline UI/model/service tests do not contact the
public config API or modify real shared configs.

## Vape integration status

The separate Vape page registers 20 adapters, with separate `vape_` config IDs:
Aim Assist (Simple/Adaptive), Bow Aimbot, Left/Right Clicker, Sprint, Triggerbot,
Velocity, Knockback Delay, Jump Reset, WTap, Shield Breaker, Auto Mace, Hit Swap,
Hit Select, Block Hit, Reach, No Item Release, Hit Flick, Silent Aura, Crystal Aura.
This adds a new Vape Velocity; it does not restore the removed Wolf Velocity or
Scaffold modules. Existing Wolf calculations and the earlier Triggerbot fixes
have not been changed.

The user's engines are included directly from sibling `../combat cpp/`; keep
that directory next to this project. Completed missing pieces are deliberately
inside that independent package: Adaptive target dispatch, Hit Swap's mace
follow-up action, Auto Mace missing-weapon/slot ownership cases, Block Hit Lag
hold, No Item Release's C++ enum syntax, and Crystal Aura's obsidian decision
and exposure-aware damage inputs. Existing aim, timing, weapon-score, PID and
ray-intersection formulas remain independent from Wolf engines.

The first enabled advanced Vape module lazily installs exact Fabric 1.21.1
hooks through live JVMTI class retransformation and Fabric's ASM. No debugger
breakpoints, JVM internal offsets or JVM memory patches are used. The embedded
Java helper provides transport only; C++ engines keep decisions. Installation
requires all six hook points on five classes to match, otherwise activation is
rolled back and the settings panel shows the failure. Other Wolf modules can
still run. JNI exceptions are cleared without swallowing vanilla exceptions.

The velocity hook matches `NetworkThreadUtils.method_11074(Packet,
PacketListener, ThreadExecutor)` with its exact intermediary descriptor. The
offline fixture uses the real signature and rejects the former incorrect
`method_11054` name and generic Object descriptor. Restart Minecraft before
loading a corrected DLL after an earlier Java bridge was defined: unloading
the native DLL alone does not unload that Java helper from the game loader.

Velocity events run after vanilla's force-main-thread gate, not on Netty.
Velocity scales/cancels the actual incoming local-player packet; Knockback Delay
queues only numeric motion values; Jump Reset consumes actual velocity events.
Attack engine ticks run once per observed player age, never one decision per frame or
a catch-up attack burst. Session changes discard pending state and packet refs.
Settings imports validate local candidates before applying; enums/nonfinite
numbers are rejected and reversed ranges are normalized.

Native attack pre/post hooks coordinate Hit Select, Hit Flick, Shield Breaker,
Hit Swap, WTap and Block Hit. Weapon selection uses the supplied
ItemStackScoreUtil, including actual Density/Breach enchantments. One shared
Vape slot lease prevents preparations overwriting each other; restoration is
conditional on continued ownership. Elytra/chestplate exchange uses vanilla
SWAP transactions with an empty cursor and ownership-checked restoration.

Silent Aura sends/replaces outgoing look/movement packets without changing the
visible camera or rotating local movement physics; local input therefore stays
in camera/world space rather than applying a second artificial strafe rotation.
Hit Flick has a bounded movement-packet queue (32 packets / 300 ms), target-ID
validation, forward-key/sprint ownership and ordered flush/restore. No Item
Release guards both vanilla stopUsingItem and outgoing RELEASE_USE_ITEM.
Block Hit uses a real main/offhand shield on 1.21.1; legacy sword blocking is
not fabricated on this version.

Reach updates vanilla's crosshair with the supplied AABB/ray math, samples range
once per tick and checks block occlusion. Vape Triggerbot revalidates that ray
immediately before delivery when Vape Reach is enabled. Wolf Triggerbot keeps
its original fresh-vanilla-ray validation. Crystal Aura evaluates live armor,
toughness, resistance, blast protection, absorption and vanilla explosion
exposure, validates occupied volumes and placement support, and uses vanilla
block interaction. Its rotating scan is capped at 96 block candidates per tick.
Server acceptance/damage is never inferred from a local send.

Overlapping enabled Wolf attack modules take priority over Vape attack
automation. Within Vape, autonomous Aura/Flick/Crystal pause the basic
Triggerbot/Left Clicker; Aura takes priority over Crystal, and active Flick
sequences finish before Aura. Bow Aimbot takes priority over visible Aim Assist.
Modules are not silently toggled off. The original unused
`ignoreActivationClick` Triggerbot field is still not advertised as a feature.

Vape attack ownership now requires a live target/action or an active Flick
sequence; an idle enabled Flick/Aura/Crystal no longer blocks the basic
Triggerbot/Clicker. An idle Bow Aimbot likewise does not disable visible aim.
Mining protection reads the interaction manager's actual block-breaking state,
not a crosshair merely pointing at terrain. Player snapshots validate the Java
entity type before any player/living-only JNI call: crystal and other non-player
manual attacks pass through without entering player-only pre/post-hit logic.
Vape object construction checks constructor availability before JNI NewObject.
Crystal armor details are read only on its decision tick; rejected vanilla block
interactions no longer count as successful placements. Wolf Triggerbot
calculations are unchanged. The focused offline decision
check is `tests/vape_adapter_test.cpp`; gameplay remains unverified.

### Main Java source comparison

The recovered reference is `../6vb8h8/VapeV4.21-main/src/main/java/gg/vape/`,
not the much shorter `../combat deobf/` rewrites. Its own README disclaims being
official original source. Both directories remain read-only references; neither
is blindly copied into the DLL. The working independent engine headers remain
in `../combat cpp/`, outside the Wolf calculation engines.

- Simple Aim Assist now runs a bounded 1 ms **numeric** solver on the client
  thread instead of mistaking the Java worker interval for a 50 ms game tick.
  Direct camera writes include vanilla's .15 mouse-to-degree conversion.
  Target retention and complete history reset avoid repeated startup stalls.
- Adaptive Aim Assist now has separate rate/bias/feed-forward and fractional
  mouse state. It no longer sends its predictive point back through Simple.
  Airborne compensation follows upward eye motion; horizontal lag is bounded.
- Triggerbot reads vanilla cooldown at `-extraDelayTicks + earlyHitTicks`.
  A tick is not a fixed 5% of every weapon's cooldown. One sample survives an
  unconsumed attempt; only observed local cooldown consumption commits a hit.
  `Minecraft.doAttack()`'s return is not treated as an entity-attack receipt.
  New module defaults follow Java's automatic activation; imported settings
  remain authoritative. Miss swings remain opt-in and do not claim damage.
- Silent Aura resets on target changes, clamps jitter inside the target box,
  bounds rotation steps and only treats shields as bypassable when an actual
  axe/preparation path exists. It still has independent C++ control, not a claim
  of feature-complete parity with Java's full rotation subsystem.
- Crystal Aura supports offhand crystals, includes normal Protection along with
  Blast Protection, and uses entity-position explosion distance. Accepted local
  actions enter bounded waits for observed obsidian/crystal spawn/removal, rather
  than repeating a placement/hit on every tick before world updates arrive.
  Slot cleanup also runs on failed construction/interaction paths. No entities
  are pre-deleted and no server acceptance is inferred from a local action.
- Jump Reset applies immediate jumps **after** vanilla velocity, so the incoming
  packet cannot overwrite the jump. Delayed intents expire upon leaving ground;
  their configured delay no longer adds an extra tick.
- Advanced decisions also have a client/render-thread pump after settings
  publication. The shared player-age guard prevents Java callbacks and this
  pump from issuing duplicate ticks. Packet/attack interception still requires
  all hook checks. Snapshot failures show actionable errors only, never idle spam.

The Java bridge ABI is now 2 (adds a post-velocity callback). If ABI 1 is already
defined in the game loader, **restart Minecraft when ready before loading this
DLL**; native unload cannot replace that helper. Builds never close the game.
The left/right clicker calculations and Wolf gameplay source remain untouched.
Offline decisions, transformer ABI and Release compilation are checks only;
in-game verification of all module combinations is still required. See
`tests/VAPE_SOURCE_AUDIT.md` for the reference paths and remaining parity limits.

Shutdown first quiesces Java callbacks on the client thread, releases owned
inputs/slots, deactivates native dispatch and restores transformed classes
before unloading native code. If class restoration is refused, Java dispatch
stays disabled so transformed call sites cannot enter an unloaded DLL.

Build now also compiles the small Java helper (JDK 17+ javac on PATH), embedding
both class files in resources. Pinned ASM 9.7.1 compile dependencies are fetched
from Fabric Maven into `build/vape-deps`; runtime uses Fabric's existing ASM.
Infrastructure checks, Release compilation and one offline transformer ABI
smoke check (`tests/VapeBridgeSmoke.java`) passed. No injection, gameplay or
server/anticheat validation was performed; controlled in-game testing remains
the user's responsibility.

## Development rules

- Keep project headers out of the precompiled header.
- Do not call `SendInput` directly from a module; use `input_service`.
- Do not write `selectedSlot` directly; use `hotbar_service`.
- Do not duplicate JNI environment or exception helpers; use `jni_context`.
- Queue work that must run on Minecraft's render/client thread through
  `client_dispatcher`.
- Add a new module declaration to its category folder and add one stable entry
  to `module_catalog.cpp`.
- Public config format remains version 3. Stable module IDs must not be renamed.

## Current target

- Platform: Windows x64
- Game: Fabric 1.21.1
- Renderer: OpenGL
- Language: C++20
- Build: Visual Studio Build Tools / MSBuild
- Output: `Thedorius-Lite.dll`

This repository is intended for controlled development and testing. Server
rules and applicable platform policies remain the user's responsibility.
