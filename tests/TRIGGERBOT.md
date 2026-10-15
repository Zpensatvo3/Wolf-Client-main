# Triggerbot verification

The refactor keeps public config v3 and the `triggerbot` id. No DLL or executable
is built by the static checks below. Native/gameplay validation remains pending
until compiling and controlled in-game testing are explicitly requested.

## Latest offline verification (2026-10-13)

- Release x64 DLL compilation succeeded after delivery validation was added.
- Fresh standalone engine, input-ledger/ownership and config test executables
  all passed with assertions enabled. Engine coverage includes the original
  knockback invalidation/backoff and its interaction with delivery rejection.
- Infrastructure checks passed; 30 non-macro C++ source files passed syntax
  parsing. JNI mapping macros were checked by the actual native DLL build.
- Controlled gameplay remains pending. Offline tests cannot reproduce Windows
  delivery timing, a moving Minecraft entity or server-side damage. A target
  can still move after input has already reached vanilla; this guard does not
  claim to retract consumed input or eliminate every possible miss.

## Static checks (no compilation)

```powershell
python .\scripts\Test-TriggerbotSources.py
.\scripts\Test-Infrastructure.ps1
```

Pass `--parser-dir <directory>` to additionally parse C++ syntax using preinstalled
`tree_sitter` and `tree_sitter_cpp`. This is syntax-tree inspection, not compilation
or type/link verification.

## Prepared executable tests (do not run a stale binary)

After permission to compile, build these as standalone C++20 tests **without
NDEBUG**. All fixtures are offline; none inject input or call the public API.

| Test | Additional production sources | Coverage |
| --- | --- | --- |
| `trigger_engine_test.cpp` | `trigger_engine.cpp` | Fake time/randomness, frame-rate independence, target/session/settings/weapon cancellation, ACK, timeout/backoff, smart/only/off criticals, normalization |
| `trigger_config_test.cpp` | `trigger_engine.cpp`, `config_fields.cpp` | Existing module fixture, v3 round trip, canonical/legacy precedence, invalid enums and legacy filters, atomic import, diagnostics |
| `trigger_input_test.cpp` | `attack_coordinator.cpp` | Pure click ledger, send-count classification, raw/window duplicate identity, stale events, physical hold protection, single-owner arbitration |

## Controlled native acceptance checks (pending)

- Check disabled, GUI/chat/inventory, focus loss, death, item use, world changes,
  friend/invisible/dead targets and reacquisition. No new automated click is
  allowed after cancellation. Already delivered vanilla input is not reversible.
- Exercise On Click with raw input and legacy mouse messages. A synthetic press
  must never activate the physical-hold requirement; verify release/repress on
  focus return, UP/DOWN repair and SendInput rejection/partial results.
- Exercise Shield Breaker/Mace Swap/Left Clicker separately and together. During
  Triggerbot ownership only its automatic timing may send hits; manual clicks
  pass through. Verify axe priority, no available axe, post-swap cooldown/crit
  reevaluation, failed preparation rollback and manual wheel/number-key changes.
- Use Off, Smart and Only criticals on ground/ascending/descending/in water/on a
  ladder/while sprinting/with blindness. Smart's airborne wait is bounded at
  150 ms; Only must never send a noncritical automatic click.
- Introduce a long frame stall. A pending click is not resent until timeout plus
  250 ms backoff and fresh reaction. No accumulated click burst may occur.
- Take incoming hits while an automatic click awaits confirmation. Airborne or
  critical-state changes alone must not cancel it. When knockback briefly loses
  the crosshair target, cancellation must retain the original confirmation
  deadline plus 250 ms (and at least 250 ms from cancellation), followed by a
  fresh reaction interval. Repeated loss/reacquisition, late cooldown consumption
  and physical clicks during backoff must not trigger a rapid retry.
- Strafe out of the crosshair at Speed II between submission and delivery.
  Both cached target identity and a fresh vanilla ray query must still match
  before forwarding the automatic press; the query uses vanilla reach and
  does not change aim or the crosshair cache. Repeat while holding left click:
  reject before UP, and reject between UP/DOWN. Cancellation restores held
  state without adding an attack-key press. A stale restoration must not affect
  a newer ticket. Left Clicker must not mistake any synthetic pulse for hardware.
- Delivery rejection must retain the earlier knockback fix: original confirmation
  deadline plus at least 250 ms and a fresh reaction before another attempt.
- Check English/Turkish and narrow inspector layout, themes and the quiet status
  line. Delivery/cooldown confirmation is local observation, not server damage ACK.

## Known boundary

The existing module registry still shares its `enabled` flag between the worker
and UI. This refactor confines Triggerbot runtime to the render thread and uses
atomic reset requests; it does not migrate the entire client's lifecycle/thread
architecture. Native SendInput/GLFW behavior requires the acceptance checks above.
