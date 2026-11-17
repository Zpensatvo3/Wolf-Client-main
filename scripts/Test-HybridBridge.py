"""Small, offline Java game-I/O bridge checks; never attaches to a running game."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile
from world_binding_fixture import write_native_bindings

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-only", action="store_true", help="Check sources without requiring rebuilt embedded classes.")
    args = parser.parse_args()
    javac, java = shutil.which("javac"), shutil.which("java")
    if not javac or not java:
        raise SystemExit("A JDK 17+ with java and javac on PATH is required")
    subprocess.run([shutil.which("python") or "python", str(ROOT / "scripts/Test-TriggerLifecycle.py")], check=True)
    subprocess.run([shutil.which("python") or "python", str(ROOT / "scripts/Test-TriggerTiming.py")], check=True)
    embedded = ROOT / "assets/hybrid"
    for name in (() if args.source_only else ("GameBridge", "UtilityBridge", "ItemBridge", "InventoryBridge", "WorldBridge", "ClientBridge")):
        if not (embedded / "net/onestep/hybrid" / (name + ".class")).is_file():
            raise SystemExit(f"Missing {name} resource; run scripts/Build-GameBridge.ps1")
    with tempfile.TemporaryDirectory(prefix="onestep-hybrid-") as work:
        bindings = write_native_bindings(Path(work) / "native-bindings.txt")
        classes = Path(work) / "classes"
        subprocess.run([javac, "--release", "17", "-encoding", "UTF-8", "-g:none", "-d", str(classes),
                        str(ROOT / "src/bridge/GameBridge.java"),
                        str(ROOT / "src/bridge/UtilityBridge.java"),
                        str(ROOT / "src/bridge/ItemBridge.java"),
                        str(ROOT / "src/bridge/InventoryBridge.java"),
                        str(ROOT / "src/bridge/WorldBridge.java"),
                        str(ROOT / "src/bridge/AnchorEvents.java"),
                        str(ROOT / "src/bridge/ClientBridge.java"),
                        str(ROOT / "tests/hybrid/GLFW.java"),
                        str(ROOT / "tests/hybrid/HybridBridgeTest.java"),
                        str(ROOT / "tests/hybrid/UtilityBridgeTest.java"),
                        str(ROOT / "tests/hybrid/ItemBridgeTest.java"),
                        str(ROOT / "tests/hybrid/InventoryBridgeTest.java"),
                        str(ROOT / "tests/hybrid/ContainerBridgeTest.java"),
                        str(ROOT / "tests/hybrid/WorldBridgeTest.java"),
                        str(ROOT / "tests/hybrid/ClientBridgeTest.java")], check=True)
        modes = [("source", str(classes))]
        subprocess.run([shutil.which("python") or "python", "-X", "utf8",
                        str(ROOT / "scripts/Check-GameBridgeContracts.py"), "--classes", str(classes)], check=True)
        if not args.source_only: modes.append(("embedded", os.pathsep.join(map(str, (embedded, classes)))))
        for mode, cp in modes:
            for test in ("HybridBridgeTest", "UtilityBridgeTest", "ItemBridgeTest", "InventoryBridgeTest", "ContainerBridgeTest", "WorldBridgeTest", "ClientBridgeTest"):
                subprocess.run([java, "-Xcheck:jni", "-cp", cp, "net.minecraft." + test, mode], cwd=ROOT, check=True)
        # Independent fixtures share intermediary names with the earlier suites.
        for folder, test in (("hit_crystal", "HitCrystalBridgeTest"), ("jump_reset", "JumpResetBridgeTest"), ("anchor_macro", "AnchorMacroBridgeTest"), ("anchor_legit", "AnchorLegitBridgeTest"), ("crystal_aura", "CrystalAuraBridgeTest")):
            isolated = Path(work) / folder
            extra = [str(ROOT / "tests/anchor_macro/AnchorMacroBridgeTest.java")] if folder == "anchor_legit" else []
            if folder == "crystal_aura":
                extra = [str(ROOT / "tests/crystal_aura/Event.java"), str(ROOT / "tests/crystal_aura/ClientTickEvents.java")]
            subprocess.run([javac, "--release", "17", "-encoding", "UTF-8", "-g:none", "-d", str(isolated),
                            str(ROOT / "src/bridge/WorldBridge.java"),
                            str(ROOT / "src/bridge/AnchorEvents.java"),
                            str(ROOT / "tests/hybrid/GLFW.java"),
                            str(ROOT / "tests" / folder / (test + ".java")), *extra], check=True)
            isolated_modes = [("source", str(isolated))]
            if not args.source_only:
                isolated_modes.append(("embedded", os.pathsep.join(map(str, (embedded, isolated)))))
            for mode, cp in isolated_modes:
                arguments = [str(bindings)] if folder == "crystal_aura" else []
                subprocess.run([java, "-Xcheck:jni", "-cp", cp, "net.minecraft." + test, mode, *arguments], cwd=ROOT, check=True)



if __name__ == "__main__":
    main()
