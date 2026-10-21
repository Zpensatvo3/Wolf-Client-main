"""Small, offline Java game-I/O bridge checks; never attaches to a running game."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-only", action="store_true", help="Check sources without requiring rebuilt embedded classes.")
    args = parser.parse_args()
    javac, java = shutil.which("javac"), shutil.which("java")
    if not javac or not java:
        raise SystemExit("A JDK 17+ with java and javac on PATH is required")
    embedded = ROOT / "assets/hybrid"
    for name in (() if args.source_only else ("GameBridge", "UtilityBridge", "ItemBridge", "InventoryBridge", "WorldBridge", "ClientBridge")):
        if not (embedded / "net/onestep/hybrid" / (name + ".class")).is_file():
            raise SystemExit(f"Missing {name} resource; run scripts/Build-GameBridge.ps1")
    with tempfile.TemporaryDirectory(prefix="onestep-hybrid-") as work:
        classes = Path(work) / "classes"
        subprocess.run([javac, "--release", "17", "-encoding", "UTF-8", "-g:none", "-d", str(classes),
                        str(ROOT / "src/bridge/GameBridge.java"),
                        str(ROOT / "src/bridge/UtilityBridge.java"),
                        str(ROOT / "src/bridge/ItemBridge.java"),
                        str(ROOT / "src/bridge/InventoryBridge.java"),
                        str(ROOT / "src/bridge/WorldBridge.java"),
                        str(ROOT / "src/bridge/ClientBridge.java"),
                        str(ROOT / "tests/hybrid/GLFW.java"),
                        str(ROOT / "tests/hybrid/HybridBridgeTest.java"),
                        str(ROOT / "tests/hybrid/UtilityBridgeTest.java"),
                        str(ROOT / "tests/hybrid/ItemBridgeTest.java"),
                        str(ROOT / "tests/hybrid/ClickerBridgeTest.java"),
                        str(ROOT / "tests/hybrid/InventoryBridgeTest.java"),
                        str(ROOT / "tests/hybrid/ContainerBridgeTest.java"),
                        str(ROOT / "tests/hybrid/WorldBridgeTest.java"),
                        str(ROOT / "tests/hybrid/ClientBridgeTest.java")], check=True)
        modes = [("source", str(classes))]
        if not args.source_only: modes.append(("embedded", os.pathsep.join(map(str, (embedded, classes)))))
        for mode, cp in modes:
            for test in ("HybridBridgeTest", "UtilityBridgeTest", "ItemBridgeTest", "ClickerBridgeTest", "InventoryBridgeTest", "ContainerBridgeTest", "WorldBridgeTest", "ClientBridgeTest"):
                subprocess.run([java, "-Xcheck:jni", "-cp", cp, "net.minecraft." + test, mode], cwd=ROOT, check=True)
        # Independent fixtures use the same intermediary names, so keep their
        # class path separate from the eight earlier bridge suites.
        crystal_classes = Path(work) / "hit-crystal"
        subprocess.run([javac, "--release", "17", "-encoding", "UTF-8", "-g:none", "-d", str(crystal_classes),
                        str(ROOT / "src/bridge/WorldBridge.java"),
                        str(ROOT / "tests/hit_crystal/HitCrystalBridgeTest.java")], check=True)
        crystal_modes = [("source", str(crystal_classes))]
        if not args.source_only: crystal_modes.append(("embedded", os.pathsep.join(map(str, (embedded, crystal_classes)))))
        for mode, cp in crystal_modes:
            subprocess.run([java, "-Xcheck:jni", "-cp", cp, "net.minecraft.HitCrystalBridgeTest", mode], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
