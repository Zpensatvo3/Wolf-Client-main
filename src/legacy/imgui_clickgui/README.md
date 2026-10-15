# Legacy ImGui ClickGUI snapshot

This directory is a source snapshot taken before the Qt Quick migration.
It is intentionally excluded from `custom-base.vcxproj` and must not be
compiled beside the active files under `src/gui`.

The active ImGui implementation remains in place until the QML mock and the
named-pipe handshake are verified. World-space overlays and the development
fallback continue to use the active ImGui sources.
