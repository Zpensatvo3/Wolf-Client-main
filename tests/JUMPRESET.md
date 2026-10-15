# Jump Reset settings redesign

The stable `jump_reset` id and public config v3 fields `mode`, `chance`,
`jump_delay_ms`, `only_combat` are preserved. Default values remain game binding,
100% activation chance, 0 ms delay and living-attacker filtering. Mode 1 remains
a synthetic system Space pulse, not a physical user press or both input paths.

The panel uses shared theme-aware controls in two sections: Timing and
Input / conditions. Presets use a three-button row at 280+ px only when all
localized labels fit; otherwise a single combo. The compact inspector reserves
320 px for controls. Inline explanatory/status paragraphs were removed at the
user's request; preset tooltips show only chance/delay. Immediate = 100%/0 ms,
Balanced = 85%/15 ms, Controlled =
65%/35 ms; each enables living-attacker filtering but preserves the chosen
input route. Presets are derived, not new serialized config fields. Restore
Defaults explicitly restores all four settings, including the input route.

Chance is application probability per detected damage event, not guaranteed
knockback reduction or successful jumping. Game Binding uses the configured
Minecraft key; System Space uses fixed Space. Colours use existing UI styling.

## Gameplay/runtime correction

User explicitly chose to retain the two real routes (game binding and system
Space), not replace the second with direct jumping or invent a packet mode.
Both now use a native-free one-damage decision model. Runtime/game binding changes
execute on the client/render thread; worker lifecycle hooks request atomic reset
and tombstone owned native Space only. Session, settings, menu/focus, death,
item-use and invalid movement conditions invalidate pending decisions.

The game-binding route applies the configured jump key. The Space route requires
that jump actually be bound to Space; otherwise it sends nothing and explains
the mismatch. Hardware keyboard/scancode/mouse state is tracked separately from
synthetic events. Cleanup restores a real held binding rather than unconditionally
setting it false. Queued Space events have request IDs, delivery validation and
stale-event rejection; genuine Space holds cannot be released by their cleanup.

Each rising hurt edge gets at most one chance decision. Fresh living-attacker
sources can arrive before/after the animation within a 105 ms pairing window;
source generations are consumed rather than reused for environmental hits.
Unknown combat sources fail closed rather than guessing from a stale attacker.
Creation retains the 85 ms ground-detection grace, but actual dispatch requires
the player to be grounded with vanilla jumping cooldown ready. Flight-enabled
players are excluded to avoid Space toggling flight. Pending decisions expire
after 170 ms; a source first observed after its confirmation deadline is rejected.

Owned input is released after delivery plus an observed player-age advance,
not an arbitrary 62 ms timer. Native delivery first seeds its own age baseline
(a tick before delayed delivery cannot qualify). Game binding seeds that baseline
immediately. A 200 ms safety timeout stops stalled input; the
140 ms retry gate remains, and timeout/cancellation never rerolls the same hit.
Native key-up failures pause new jumps and retry cleanup. Focus loss and shutdown
release input while the client-side guard is available. Internal diagnostic
codes retain read/rebind/send/timeout/cleanup failures without panel paragraphs
or per-frame notifications.

## Source-only checks

- `scripts/Test-JumpResetSources.py` (optional Tree-sitter `--parser-dir`).
- `scripts/Test-Infrastructure.ps1`.
- Prepared `tests/jump_reset_config_test.cpp`: link `config_fields.cpp` after
  permission to compile, with assertions enabled. Covers v3 round-trip, invalid
  modes/types, atomic import, clamping, presets and route preservation.
- Prepared `tests/jump_reset_engine_test.cpp`: header-only engine and key ledger,
  with assertions enabled; fake clock/chance, one attempt per damage, grounded
  dispatch, delayed source, expiry, tick-based release, cancellation, hardware
  hold preservation and stale Space events.

Release x64 and both offline test executables compiled on 2026-10-13; tests
passed with assertions enabled. Release x64 compiled again after compact-panel
cleanup. Controlled gameplay/visual validation remains pending; source parsing
is not native type checking or a UI preview.

## Native/UI acceptance checks (pending)

- Turkish/English, narrow inspector and each theme; open inline combos, scrolling,
  tooltip visibility, profile selection, custom-value highlighting and defaults.
- A rejected config must leave live settings unchanged. Imported or edited
  settings invalidate pending damage decisions; current hurt animation is seeded
  rather than interpreted as a fresh event under new settings.
- Client reads one mutex-protected settings snapshot. The final dispatch is
  serialized against setting publication, so an obsolete snapshot cannot send
  another jump after the new revision commits.
- Chance 0, changing input route/filter, restoring defaults and disable-enable
  during pending/owned input. Already consumed vanilla input is not reversible.
- Both routes at 20/60/144/240 FPS, long frame stalls, native rejection, remapped
  jump keys, physically pressing/releasing a binding during an owned pulse,
  GUI/focus changes and world/player replacement. No second jump or stuck key.
- Combat-source packets before/after hurt, no source on the client, nonliving
  damage after combat, failed chance and a second hit during cooldown/hold.
  Observational source pairing is bounded, not a server damage acknowledgment.
- This does not install an early movement-tick/network hook. A hit observed only
  after knockback already put the player airborne cannot force a legal grounded
  jump; the decision waits only within its expiry window. No guaranteed knockback
  reduction is claimed. Native gameplay verification is still required.
