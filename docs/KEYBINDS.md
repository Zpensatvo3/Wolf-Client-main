# Other > Keybinds

## UI and behavior

`src/ui/grim_ui.cpp` adds one Other page; `keybinds_page.cpp` renders actual
ImGui hit targets, not a keyboard image. The ANSI 104-key layout includes
function keys, navigation/arrows and a physically separated numpad. Key widths,
tall numpad keys and gaps are specified once in `platform/input/key_codes.hpp`.
The keyboard scales to the available width; the module picker below scrolls
without a visible scrollbar. Hover, selection and assigned states use the
current accent, with bounded animation arrays and reduced-motion support.

The picker obtains module names/categories from the existing registry through
`keybinds_model.hpp`. Search is case-insensitive; categories are dynamic. The
capability is independent of settings count. Modules without custom settings
remain available. Click GUI opts out because its shortcut is fixed Right Shift.
Keybind-only module cards toggle directly without opening an empty settings
panel; this does not exclude them from the keyboard's module picker.

Click a key, then a module row to assign or move its binding. A matching row
removes that assignment; its X removes any current binding, including imported
mouse/legacy bindings. Existing shared-key semantics remain: a physical rising
edge toggles every assigned module, subject to existing focus/auth/GUI gates.
Binding changes do not toggle module enable state.

Reserved keys are selectable for inspection, but cannot receive new bindings:

- Escape clears the old capture widget or closes the menu using existing rules.
- Right Shift opens/closes the menu; End performs emergency unload.
- Windows keys remain operating-system shortcuts.

Imported reserved assignments can still be removed. No forced migration removes
existing bindings. Shared assignments appear in the selected-key summary and
matching module rows. The search field has a trailing focusable magnifier;
the category label and arrow share one background. Successful save status and
redundant keyboard/instruction captions are hidden; save errors remain visible.

## One binding engine, existing config fields

All writes use `modules::module::set_keybind`; the existing worker loop and
`key_edge` still perform activation. The binding value is atomic because the UI
writes while the worker samples it. A revision counter signals persistence,
not input activation. Module-panel capture, public config imports and the new
page all update the same value. GUI typing remains gated, including a key held
through menu close or a binding changed while held.

Existing Windows VK values 0..255 remain unchanged. **269 (`0x10d`) is the only
additional value**, separating numpad Enter (E0 1C) from main Enter (1C).
The same mapping is used for old capture controls and HUD key labels. Keypad
digits/operators and navigation use the existing physical scan tracking,
including NumLock-off input; End uses its extended scan after hooking so numpad
1 with NumLock off cannot trigger emergency unload. Letter bindings retain
logical Windows VK semantics; labels depict ANSI US, not an OS-layout detector.

Public configs keep their existing `modules/id/enabled/keybind/settings` format.
The reader additionally accepts 269; no module IDs or settings are renamed.
Older DLLs that only accept 0..255 will reject a public config containing 269;
use an updated DLL or remove that binding before sharing with older builds.

`public_config.cpp` also provides binding-only export/validated apply using the
same module IDs, `keybind` fields and JSON parser. This intentionally does not
load gameplay settings or enabled state. The prior config service is a remote
public library, not a local startup profile, so `configs/keybind_profile.cpp`
adds the missing local load/save lifecycle without creating another keybind
store inside modules or another dispatch engine.

Storage: `%LOCALAPPDATA%\onestep\keybinds.json`, shared by DLL targets for this
Windows account. Load occurs after registry creation and before hooks/ticks.
Unknown IDs are ignored; invalid/duplicate entries reject the entire load before
any binding changes. Missing files preserve defaults. Invalid files remain
untouched until an actual subsequent binding edit.

New-page actions save immediately; old capture/public imports use a 400 ms
debounce and render-shutdown flush. Temporary-file replacement preserves the
previous profile on save failure (MoveFileExW on Windows). Errors appear in the
page and failed immediate saves produce an error toast, not false success.
No cloud requests, extra dependencies or automatic uploads are introduced.

## Validation

```sh
python scripts/Test-Infrastructure.py
python scripts/Test-Offline.py --jobs 2
python scripts/Test-GrimUI.py
python scripts/Test-GrimUI.py --preview-dir out/ui-preview
git diff --check
```

`key_codes_test.cpp` checks 104 unique, nonoverlapping key rectangles, bounds,
VK/scan mapping, modifiers, both Enter keys, NumLock-off keys, reserved-key
policy and existing input-edge gating. The real ImGui headless test exercises
sidebar navigation, selection, assignment/move/removal/shared bindings,
every physical key hit target, module-list wheel scrolling, search/category
model, option-less modules, reserved-key removal, actual file
save/reload, public config roundtrip, corrupt-file rejection and I/O failure
preservation. It checks viewport bounds at 900, 1024 and 1600 pixels wide.
No user profiles or network services are accessed by these tests.

Optional captures are real ImGui draw-data previews with fixture modules,
not in-game screenshots. Windows MSVC linking, WndProc physical-key delivery,
NumLock/side-modifier behavior and startup persistence in Minecraft still require
Windows build and in-game testing; the Linux tests do not verify those paths.

## Integration files

- UI: `src/ui/grim_ui.cpp`, new `keybinds_page.hpp/.cpp` and `keybinds_model.hpp`.
- Input/lifecycle: new `src/platform/input/key_codes.hpp`,
  `src/app/module_loop.cpp`, `src/app/runtime.cpp`, `src/gui/gui.cpp`.
- Existing module/config contracts: `src/modules/core/module.hpp/.cpp`,
  `public_config.hpp/.cpp`, `src/modules/client/click_gui.hpp` capability and
  `src/modules/render/arraylist.cpp` binding labels.
- Local persistence: new `src/configs/keybind_profile.hpp/.cpp`.
- Build: `custom-base.vcxproj` and matching `.filters`.
- Validation: new `tests/key_codes_test.cpp`, `scripts/Test-Offline.py`, and
  extension of the existing local `tests/grim_ui_test.cpp` / `scripts/Test-GrimUI.py`.
- Documentation: `README.md`, `docs/GRIM_UI_LAYOUT.md` and this document.

Pre-existing local GrimUI grid/settings cleanup and its test infrastructure are
preserved; this list describes the additional Keybinds integration.
