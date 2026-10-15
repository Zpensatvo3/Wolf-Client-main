# Aim Assist stability pass

Scope: `aim_assist` and `legit_click_aim_assist`, which share the legit engine.
The blatant engine and Triggerbot source/behaviour are not modified in this pass.
Public config v3 fields and aim-point values Head=0, Upper Body=1, Adaptive=4
remain unchanged. Invalid enums/non-finite settings now reject the entire import.

## Activation correction (2026-10-13)

The shared Entity mapping incorrectly declared `getEyeY` as `method_5751()D`.
That name is standing eye height (`()F`), not world eye Y. A missing JNI method
silently returned zero, so nearby players at normal world heights were discarded
by range/visibility checks. Corrected to `method_23320()D`, with standing eye
height mapped to `method_5751()F`; the engine checks method/angle availability
before consuming numeric values. Verified against the
[Yarn 1.21.1 Entity mappings](https://maven.fabricmc.net/docs/yarn-1.21.1+build.3/net/minecraft/entity/Entity.html#getEyeY()).

Source checks lock both names/return types; the geometry test now includes a
nearby target at world Y=64 and rejects the old zero/relative-height eye origins.
This fixes a concrete activation blocker without raising default strength or
removing friend, visibility, range, weapon or physical-click gates. Offline
tests cannot confirm behaviour in an injected Minecraft session.

Verification: Release x64 compiled successfully after the mapping/panel fix;
the freshly compiled math test (assertions enabled) passed, including elevated
world-space targeting. Config and original math tests also passed in the preceding
build. Controlled Minecraft verification is still pending.

## Tracking response

Default strength remains 42%; imported strength values are preserved. The
mechanism, rather than the default percentage, is changed. Motion carries camera
velocity through a critically damped response and compensates for observed
target angular velocity. Bearing is calculated from current target position and
eye position, not camera-relative aiming error, so our own yaw correction and
manual mouse turning do not masquerade as target speed.

Target velocity is estimated over actual bearing-update intervals, smoothed
with a 25 ms time constant and retained briefly between tick-spaced updates.
Stopped estimates decay after 75 ms; reversals brake the old estimate, and
discontinuous/implausible bearing jumps clear momentum. There is no extrapolated
aim point: output is still bounded by the current filtered-point error/box.
Integration accounts for the observed bearing being at the end of the elapsed
frame, rather than adding a second frame's movement at low FPS. A strength-scaled
lag bias permits partial assistance while keeping moving-target response stable.

The strength response is nonlinear: rate `10 + 28 * sqrt(strength_fraction)` and
yaw speed cap `120 + 360 * sqrt(strength_fraction)` degrees/second. Moving targets
get at most a further 15% rate boost; pitch remains softer. Observed-speed
compensation and carried camera speed are both bounded by manual influence.
A smoothstep engagement curve provides a soft start over 90 ms.

Zero strength, lost/occluded targets, target replacement, physical-click release,
manual override, invalid data and session/config resets clear velocity/history.
On-target arrival stops camera velocity but retains valid bearing samples for
the next strafe. This must not introduce coasting after cancellation or point
overshoot. Passing offline tests cover same-percentage smooth/strong response,
moving-target lag, FPS behaviour, wrap, tick-spaced observations, reversal and
momentum reset.

## Source-only verification

Run `scripts/Test-AimAssistSources.py`, optionally with `--parser-dir` pointing
to installed Tree-sitter libraries, and `scripts/Test-Infrastructure.ps1`.
These do not compile tests or modify existing DLLs.

Standalone C++20 tests (compiled/run with assertions enabled on 2026-10-13):

- `aimassist_math_test.cpp`: header-only geometry/motion, exactly zero movement
  at zero strength, cardinal axes/vertical aim, finite inputs, speed limits,
  FPS-independent small-error response and manual override, moving-box filter.
- `aimassist_config_test.cpp`: link `config_fields.cpp` only; existing offline
  module fixture, v3 round-trip, sparse enums, clamping, failed-import atomicity.

Both require assertions enabled. Neither injects into Minecraft.

## Controlled gameplay checks (pending)

- Disabled, zero strength, GUI/chat/inventory, lost focus, death, item use,
  non-player camera, world/player replacement: no camera rotation or stale lock.
- Legit Click activates only on real physical left hold, never Triggerbot or
  clicker-generated input. Releasing the button stops movement immediately;
  its 200 ms target-retention grace does not continue aiming.
- Target behind a new wall is rejected on the next camera update, even between
  40 ms scans. Both current and filtered aim points need clear line of sight.
- Speed II strafing: smoothed point stays inside the target's current box.
  Existing friend/alive/invisible/range/FOV checks still apply.
- Manual aim away from a target has priority, with angular-speed thresholds
  rather than per-frame thresholds. Check 40/60/144/240 FPS and a long stall.
- Strength/range/FOV/aim-point changes and disable-enable invalidate the old
  target, filtered point and expected camera angle on the client thread.
- Existing 2.5-degree switching advantage, 80 ms challenger confirmation and
  22% lock-retention FOV hysteresis remain, rather than silently retuning them.

Camera updates and runtime/JNI cleanup are now performed by render callbacks;
worker lifecycle hooks request an atomic reset only. This does not redesign
the registry's existing shared `enabled` flag or coordinate every camera module.
Enabling other aim engines simultaneously still requires a separate coordination
pass. Static parsing is not native type checking or gameplay validation.
