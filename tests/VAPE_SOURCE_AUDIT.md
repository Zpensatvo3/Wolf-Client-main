# Vape source audit — 2026-10-15

Reference root: `../../6vb8h8/VapeV4.21-main/src/main/java/gg/vape/`.
Its README describes a recovery project, not guaranteed original vendor code.
`../../combat deobf/` omits substantial state/input/event behavior and is not a
faithful authoritative reference. Reference Java and duplicate reference C++
directories were not edited.

## Source-checked corrections

| Area | Main Java reference | Correction in working C++ / adapter |
| --- | --- | --- |
| Simple aim cadence | `module/combat/aimassist/AimAssistRotationWorkerThread.java`, `AimAssistRotationSubModule.java` | 1 ms bounded numeric solver, buffer order, target retention, full reset |
| Mouse units | `rotation/PlayerMouseRotationApplier.java` | raw mouse delta becomes degrees with .15, not 1.0 |
| Adaptive aim | `module/combat/aimassist/AimAssistTargetingSubModule.java` | separate controller; no accidental Simple dispatch; rate/bias/fractional history and airborne inputs |
| Trigger cooldown | `module/combat/Triggerbot.java`, `utils/AttackCooldownUtil.java` | query vanilla at `-extra + early`; do not invent a weapon-independent threshold |
| Trigger consumption | `input/AttackKeyController.java` and vanilla doAttack behavior | a decision is not delivery/consumption; fresh target checks remain mandatory |
| Silent aim | `module/combat/SilentAura.java`, `rotation/AdaptiveRotationController.java` | remaining-error clamp, target-change reset, jitter inside real bounds, real shield preparation checks |
| Crystal state | `module/combat/crystalaura/CrystalAuraTargetSubModule.java`, `CrystalAuraPlacementSubModule.java` | wait for observed world changes; bounded retry; offhand; cleanup on every slot exit |
| Crystal damage | `module/combat/CrystalAura.java` | entity-position distance and ordinary Protection as well as Blast Protection |
| Jump ordering | live 1.21.1 velocity callback and independent `JumpReset` | pre-decision/post-velocity application; cancel airborne stale intent and fix one-tick delay |
| All advanced modules | event-driven `onTick` and native transport | render-thread fallback pump with shared age guard; precise snapshot failure messages |

## Checks and limits

- `vape_adapter_test.cpp`: one short offline binary; Simple/Adaptive movement,
  reset/mining behavior, Aura cooldown, Crystal safety/offhand/protection,
  Jump Reset cancellation/delay, Trigger offset direction and commit spacing.
- `VapeBridgeSmoke.java`: one offline transformer/ABI check. No game attachment.
- `Test-Infrastructure.ps1`: registry and shared service boundaries.
- Release x64 compile; no injection, process termination, server interaction or
  gameplay automation as part of verification.

This is NOT a claim that all 20 modules reproduce every recovered Java feature.
The Java target filter UI, complete high-speed adaptive snap/noise behavior,
Crystal optimization/predicted entity removal and the full rotation/controller
stack are not copied wholesale. Existing safe target restrictions and bounded
state remain. Never replace an unverified source path with claimed parity.

The left/right clicker formulas and important Wolf Triggerbot ray/backoff logic
are unchanged. Existing configs keep their IDs and supplied settings. ABI 2 adds
post-velocity dispatch; an old defined helper requires a user-controlled game
restart before loading the new DLL. The build does not touch the running game.
