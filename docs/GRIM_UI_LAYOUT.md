# GrimUI grid and right-side settings

The active entry remains `gui::grim::render` from `src/gui/gui.cpp`.
`src/gui/` owns Windows/OpenGL integration and shared controls; `src/ui/` owns
application pages. Both directories remain in use.

## Layout and interaction

- Feature/search results use registry order, left to right, in two columns.
  Cards are 62 pixels high, with an 8-pixel gap; narrow viewports fall back to
  one column. No module IDs, gameplay categories or config fields are changed.
- The main surface retains its 960-by-620 default, shared with Other pages.
  Settings open in a separate 304-pixel window to its right, not in a reserved
  blank strip. The group is centered/clamped including open auxiliary panels.
  On smaller displays the main surface narrows to accommodate the settings.
- The settings window has its own hit-test bounds. The transparent root never
  covers it: otherwise clicking a card could bring the root above the settings
  and swallow wheel input. Only the settings content scrolls, without scrollbars.
- Cards contain only the name and description; clicking a configurable module
  selects its right-side panel without toggling it. Enabled is its first row, above
  Keybind, with no hover background. The keybind badge has its own 8-pixel corner
  radius rather than inheriting a potentially square page style.
  Modules with no custom controls instead toggle directly, including search/Enter;
  their bindings remain managed in Other > Keybinds. Full Bright inherits the
  base's empty panel; No Jump Delay and Fast Stop explicitly opt out because
  their old renderers only contain explanatory text. Click GUI and Friends
  Manager retain their actual non-config UI controls. The legacy height is the
  default custom-options capability; informational renderers can override it.
  Panic remains an immediate unload action. The obsolete card toggle renderer
  and its animation cache were removed.
- Module-specific child IDs isolate settings scroll position. Other pages dismiss the panel. Search, keybinds and Escape
  remain available; pending autocomplete is cancelled when the field loses focus.
- Context shutdown clears GrimUI and shared widget animation state before
  destroying ImGui, avoiding stale pointers, measurements and color-picker state.

## Verified cleanup

Repository-wide symbol/call-site searches, including tests and build inputs,
found no callers, callback bindings or exported entry points for:

- `settings_stepper`, `settings_coordinate_row` and their declarations;
- `surface_style::module_card`, its private `selected_fill`/`mix` chain;
- `status_badge` and its declaration;
- `consume_geometry_reset`, `geometry_reset_pending` and their unread flag.

These were removed, along with a duplicate declaration, unused GrimUI includes
and redundant skin refresh timer. The live surface configuration remains.
Two forwarding wrappers were inlined at their sole caller: config rendering and
the direct HUD editor. Their underlying pages are unchanged.

Historical layout/energy/window geometry preference fields were deliberately
preserved: they still participate in parsing, normalization and serialization.
Compatibility widget names (`ImGui::Thedorius*`) remain used by module renderers;
they are not dead code. Authentication, texture/font ownership, theme selection,
input hooks, HUD editing, Friends and Configs were not replaced.

Other now also exposes the interactive Keybinds page; its registry/config/input
integration and compatibility policy are documented in [KEYBINDS.md](KEYBINDS.md).

## Validation and previews

```sh
python scripts/Test-Infrastructure.py
python scripts/Test-Offline.py --jobs 2
python scripts/Test-GrimUI.py
python scripts/Test-GrimUI.py --preview-dir out/ui-preview
git diff --check
```

The Linux headless test compiles the shipping GrimUI, shared widgets, module
base/keybind UI, schema renderer and Dear ImGui. Only game/platform services are
inert fixtures. It exercises column order, full-card selection, Enabled above
Keybind, modules without custom options and Panic,
long settings/wheel input, per-module scroll, search/Enter, Other pages, viewport
bounds, Escape and runtime reset. It also runs preference parser/normalization
tests without accessing user preferences. It is included in offline CI.
The same test exercises actual keyboard assignment/removal and local profile
load/save/failure handling, as described in the Keybinds document.

Optional captures rasterize actual ImGui triangles and the real font atlas to
PPM at 2x resolution, re-rasterizing the fonts at matching density and using
bilinear texel-center sampling like the OpenGL backend. A separately rendered
settings close-up is also exported; existing images are not upscaled/sharpened.
This changes preview fidelity, not the game's font or render pipeline.
If Pillow is already available, the existing helmet texture is decoded and
PNG copies are exported too. Preview output stays ignored under `out/`; no
image library is added to the application or required for ordinary tests.
Fixture license/name/settings are illustrative, not data from a live session.
These are renderer previews, **not in-game screenshots**. Windows MSVC linking,
live OpenGL/input hooks and gameplay have not been verified in this Linux run.
