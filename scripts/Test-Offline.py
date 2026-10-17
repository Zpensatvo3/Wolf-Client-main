"""Compile and run offline C++20 tests; does not attach to Minecraft or start a JVM."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
TESTS = {
    "profile_model": ["tests/profile_model_test.cpp"],
    "menu_background": ["tests/menu_background_test.cpp"],
    "menu_background_renderer": ["tests/menu_background_renderer_test.cpp", "src/ui/menu_background.cpp",
                                 "include/imgui/imgui.cpp", "include/imgui/imgui_draw.cpp", "include/imgui/imgui_widgets.cpp", "include/imgui/imgui_tables.cpp"],
    "auth_policy": ["tests/auth_policy_test.cpp"],
    "module_runtime": ["tests/module_runtime_test.cpp"],
    "vape_adapter": ["tests/vape_adapter_test.cpp"],
    "aimassist_math": ["tests/aimassist_math_test.cpp"],
    "trigger_engine": ["tests/trigger_engine_test.cpp", "src/modules/combat/trigger_engine.cpp"],
    "trigger_input": ["tests/trigger_input_test.cpp", "src/services/game/attack_coordinator.cpp"],
    "crystal_aura_engine": ["tests/crystal_aura_engine_test.cpp"],
    "anchor_timing": ["tests/anchor_timing_test.cpp"],
    "crystal_aura_ownership": ["tests/crystal_aura_ownership_test.cpp", "src/services/game/attack_coordinator.cpp"],
    "jump_reset_engine": ["tests/jump_reset_engine_test.cpp"],
    "no_jump_delay": ["tests/no_jump_delay_test.cpp"],
    "fast_stop_engine": ["tests/fast_stop_engine_test.cpp"],
    "inv_walk_engine": ["tests/inv_walk_engine_test.cpp"],
    "pot_refill_engine": ["tests/pot_refill_engine_test.cpp"],
    "anti_bot_engine": ["tests/anti_bot_engine_test.cpp"],
    "notification_model": ["tests/notification_model_test.cpp", "src/services/notifications/notification_service.cpp"],
    "notification_renderer": ["tests/notification_renderer_test.cpp", "src/ui/notification_renderer.cpp",
                              "include/imgui/imgui.cpp", "include/imgui/imgui_draw.cpp", "include/imgui/imgui_widgets.cpp", "include/imgui/imgui_tables.cpp"],
    "item_esp_geometry": ["tests/item_esp_geometry_test.cpp"],
    "config_fields": ["tests/config_fields_test.cpp", "src/modules/core/config_fields.cpp"],
    "aimassist_config": ["tests/aimassist_config_test.cpp", "src/modules/core/config_fields.cpp"],
    "jump_reset_config": ["tests/jump_reset_config_test.cpp", "src/modules/core/config_fields.cpp"],
    "trigger_config": ["tests/trigger_config_test.cpp", "src/modules/core/config_fields.cpp", "src/modules/combat/trigger_engine.cpp"],
    "vape_config": ["tests/vape_config_test.cpp", "src/modules/core/config_fields.cpp"],
    "sdk_mappings": ["tests/sdk_mappings_test.cpp"],
    "settings_store": ["tests/settings_store_test.cpp"],
    "utility_settings": ["tests/utility_settings_test.cpp", "src/modules/core/config_fields.cpp"],
    "remaining_settings": ["tests/remaining_settings_test.cpp", "src/modules/core/config_fields.cpp"],
    "settings_renderer": ["tests/settings_renderer_test.cpp"],
    "public_config": ["tests/public_config_test.cpp", "src/modules/core/public_config.cpp", "src/modules/core/config_fields.cpp"],
    "snapshot_session": ["tests/snapshot_session_test.cpp"],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "cl" if os.name == "nt" else "g++"))
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--test", action="append", choices=TESTS, help="Run only these tests (repeatable).")
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error("C++ compiler not found; set --compiler or CXX (MSVC: use Developer PowerShell).")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    msvc = Path(compiler).name.lower() in ("cl", "cl.exe", "clang-cl", "clang-cl.exe")
    with tempfile.TemporaryDirectory(prefix="onestep-offline-") as directory:
        def run_test(item):
            name, sources = item
            work = Path(directory) / name
            work.mkdir()
            output = work / (name + (".exe" if os.name == "nt" else ""))
            command = [compiler]
            if msvc:
                command += ["/nologo", "/std:c++20", "/EHsc", "/UNDEBUG", "/W4", f"/Fe:{output}"]
            else:
                command += ["-std=c++20", "-UNDEBUG", "-Wall", "-Wextra", "-pedantic", "-pthread", "-o", str(output)]
            if name in ("settings_renderer", "notification_renderer", "menu_background", "menu_background_renderer"):
                command += [f"/I{ROOT / 'include'}"] if msvc else ["-I", str(ROOT / "include")]
            if name == "sdk_mappings":
                platform = "win32" if os.name == "nt" else "linux"
                includes = [ROOT / "include/jni", ROOT / "include/jni" / platform]
                for include in includes:
                    command += [f"/I{include}"] if msvc else ["-I", str(include)]
                sdk = ROOT / "src/sdk/minecraft/fabric_1_21_1"
                headers = [sdk / name for name in json.loads((sdk / "manifest.json").read_text(encoding="utf-8"))["headers"]]
                headers += [ROOT / name for name in (
                    "src/services/game/overlay_context.hpp",
                    "src/services/game/loaded_chunks.hpp", "src/modules/visual/item_esp.hpp",
                    "src/modules/misc/inv_walk.hpp",
                    "src/services/game/anti_bot_service.hpp", "src/modules/combat/anti_bot.hpp",
                    "src/modules/visual/storage_esp.hpp", "src/modules/visual/block_esp.hpp")]
                # Each domain must compile without a later umbrella completing it.
                for header in headers:
                    probe = work / "header_check.cpp"
                    probe.write_text(f'#include "{header.as_posix()}"\n', encoding="utf-8")
                    if header.name == "overlay_context.hpp":
                        probe.write_text(probe.read_text(encoding="utf-8") +
                            "void check_overlay(services::game::overlay_session& session, "
                            "const maps::ClientWorld& world, const maps::ClientPlayerEntity& player) { "
                            "session.observe({world, player}); session.accepts(session.ticket()); session.reset(); }\n",
                            encoding="utf-8")
                    syntax = [compiler]
                    syntax += ["/nologo", "/std:c++20", "/EHsc", "/Zs"] if msvc else ["-std=c++20", "-fsyntax-only", "-pthread"]
                    for include in includes:
                        syntax += [f"/I{include}"] if msvc else ["-I", str(include)]
                    checked = subprocess.run(syntax + [str(probe)], cwd=work, capture_output=True, text=True)
                    if checked.returncode:
                        return name, False, f"Standalone SDK header failed: {header}\n" + checked.stdout + checked.stderr
            command += [str(ROOT / source) for source in sources]
            built = subprocess.run(command, cwd=work, capture_output=True, text=True)
            if built.returncode:
                return name, False, built.stdout + built.stderr
            ran = subprocess.run([str(output)], cwd=work, capture_output=True, text=True)
            return name, ran.returncode == 0, built.stdout + built.stderr + ran.stdout + ran.stderr

        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            selected = {name: TESTS[name] for name in args.test} if args.test else TESTS
            results = list(pool.map(run_test, selected.items()))
        for name, passed, detail in results:
            print(f"{'PASS' if passed else 'FAIL'} {name}")
            if detail.strip():
                print(detail.rstrip())
        return 0 if all(passed for _, passed, _ in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
