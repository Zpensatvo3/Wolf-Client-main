"""Linux headless test of the shipping GrimUI/widgets with inert platform services.

Optional --preview-dir exports real ImGui draw data as PPM images. No downloads,
Minecraft, auth requests, GPU or DLL injection. Windows builds use real Windows.h;
the small shim is confined to this Linux test's include path.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import struct
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preview-dir", type=Path)
    args = parser.parse_args()
    compiler = shutil.which("g++")
    if not compiler: parser.error("Linux GCC C++20 required")
    image_module = None
    if args.preview_dir:
        args.preview_dir.mkdir(parents=True, exist_ok=True)
        # Pillow is optional and used only for PNG export/fixture decoding,
        # never by the test runner or application in ordinary validation.
        try:
            from PIL import Image
            image_module = Image
        except ImportError:
            print("Pillow unavailable: exporting PPM with no logo texture")
        if image_module:
            logo = image_module.open(ROOT / "assets/icons/helmet.png").convert("RGBA")
            (args.preview_dir / "helmet.rgba").write_bytes(struct.pack("<ii", *logo.size) + logo.tobytes())
    sources = ["tests/grim_ui_test.cpp", "src/ui/keybinds_page.cpp", "src/configs/keybind_profile.cpp", "src/modules/core/public_config.cpp",
               "src/gui/widgets.cpp", "src/modules/core/module.cpp",
               "src/modules/core/config_fields.cpp", "src/utils/utils.cpp",
               "src/services/notifications/notification_service.cpp", "src/ui/notification_renderer.cpp",
               "include/imgui/imgui.cpp", "include/imgui/imgui_draw.cpp", "include/imgui/imgui_widgets.cpp",
               "include/imgui/imgui_tables.cpp"]
    with tempfile.TemporaryDirectory(prefix="onestep-grim-ui-") as directory:
        output = Path(directory) / "grim_ui_test"
        subprocess.run([compiler, "-std=c++20", "-UNDEBUG", "-O1", "-pthread",
                        "-Iinclude", "-Iinclude/jni", "-Iinclude/jni/linux", "-Itests/support",
                        *sources, "-o", str(output)], cwd=ROOT, check=True)
        subprocess.run([str(output), str(args.preview_dir.resolve()) if args.preview_dir else "", str(Path(directory) / "keybinds.json")],
                       cwd=ROOT, check=True)
        preferences_test = Path(directory) / "ui_preferences_test"
        subprocess.run([compiler, "-std=c++20", "-UNDEBUG", "-Iinclude", "-Itests/support",
                        "tests/ui_preferences_test.cpp", "-o", str(preferences_test)], cwd=ROOT, check=True)
        subprocess.run([str(preferences_test)], cwd=ROOT, check=True)
    if args.preview_dir and image_module:
        for name in ("combat-grid", "combat-settings", "combat-1280", "settings-detail", "keybinds-keyboard", "keybinds-assignment"):
            with image_module.open(args.preview_dir / (name + ".ppm")) as preview:
                preview.save(args.preview_dir / (name + ".png"))
        print("Preview: real ImGui draw data; game/auth services are inert fixtures")


if __name__ == "__main__":
    main()
