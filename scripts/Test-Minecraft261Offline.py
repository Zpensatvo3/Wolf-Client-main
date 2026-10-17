"""Compile the staged 26.1 native SDK and prove it cannot enable a normal DLL build.

No network access, JVM, Minecraft, MSVC DLL/link verification or production service.
Use Test-Minecraft26Readiness.py --native-261 separately with official fixtures.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "cl" if os.name == "nt" else "g++"))
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler: parser.error("A C++20 compiler is required")
    msvc = Path(compiler).name.lower() in ("cl", "cl.exe", "clang-cl", "clang-cl.exe")
    includes = (ROOT / "include/jni", ROOT / "include/jni" / ("win32" if os.name == "nt" else "linux"))
    common = [compiler] + (["/nologo", "/std:c++20", "/EHsc", "/UNDEBUG", "/DONESTEP_MINECRAFT_VERSION=26100"] if msvc
                           else ["-std=c++20", "-pthread", "-UNDEBUG", "-DONESTEP_MINECRAFT_VERSION=26100"])
    for include in includes: common += [f"/I{include}"] if msvc else ["-I", str(include)]
    porting = "/DONESTEP_MINECRAFT_PORTING=1" if msvc else "-DONESTEP_MINECRAFT_PORTING=1"
    try:
        with tempfile.TemporaryDirectory(prefix="onestep-261-offline-") as directory:
            work = Path(directory)
            output = work / ("sdk.exe" if os.name == "nt" else "sdk")
            command = common + [porting] + ([f"/Fe:{output}"] if msvc else ["-o", str(output)])
            subprocess.run(command + [str(ROOT / "tests/minecraft261_sdk_test.cpp")], cwd=work, check=True)
            subprocess.run([str(output)], cwd=work, check=True)
            syntax = ["/Zs"] if msvc else ["-fsyntax-only"]
            for source in ("src/platform/adapter/game_adapter.cpp", "src/platform/adapter/item_model.cpp", "src/utils/version_manager.cpp"):
                subprocess.run(common + [porting] + syntax + [str(ROOT / source)], cwd=work, check=True)
            for header in ("src/modules/visual/chams_model.hpp", "src/modules/visual/sky_bindings.hpp", "src/modules/vape/game_bindings.hpp"):
                probe = work / "header.cpp"
                probe.write_text(f'#include "{(ROOT / header).as_posix()}"\n', encoding="utf-8")
                subprocess.run(common + [porting] + syntax + [str(probe)], cwd=work, check=True)
            probe = work / "guard.cpp"
            probe.write_text(f'#include "{(ROOT / "src/platform/adapter/target_version.hpp").as_posix()}"\n', encoding="utf-8")
            rejected = subprocess.run(common + syntax + [str(probe)], cwd=work, capture_output=True, text=True)
            if rejected.returncode == 0 or "incomplete port" not in rejected.stdout + rejected.stderr:
                raise ValueError("26.1 ordinary-build guard did not fail as expected")
            print("PASS: native adapter/item-model/version syntax and private binding headers")
            print("PASS: 26.1 rejected without the explicit offline porting define")
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"26.1 offline tests failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
