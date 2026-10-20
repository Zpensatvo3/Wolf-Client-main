"""Small, offline Java game-I/O bridge checks; never attaches to a running game."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    javac, java = shutil.which("javac"), shutil.which("java")
    if not javac or not java:
        raise SystemExit("A JDK 17+ with java and javac on PATH is required")
    embedded = ROOT / "assets/hybrid"
    for name in ("GameBridge", "UtilityBridge", "ItemBridge", "InventoryBridge", "WorldBridge", "ClientBridge"):
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
                        str(ROOT / "tests/hybrid/HybridBridgeTest.java"),
                        str(ROOT / "tests/hybrid/UtilityBridgeTest.java"),
                        str(ROOT / "tests/hybrid/ItemBridgeTest.java"),
                        str(ROOT / "tests/hybrid/ClickerBridgeTest.java"),
                        str(ROOT / "tests/hybrid/InventoryBridgeTest.java"),
                        str(ROOT / "tests/hybrid/ContainerBridgeTest.java"),
                        str(ROOT / "tests/hybrid/WorldBridgeTest.java"),
                        str(ROOT / "tests/hybrid/ClientBridgeTest.java")], check=True)
        for mode, cp in (("source", str(classes)), ("embedded", os.pathsep.join(map(str, (embedded, classes))))):
            for test in ("HybridBridgeTest", "UtilityBridgeTest", "ItemBridgeTest", "ClickerBridgeTest", "InventoryBridgeTest", "ContainerBridgeTest", "WorldBridgeTest", "ClientBridgeTest"):
                subprocess.run([java, "-Xcheck:jni", "-cp", cp, "net.minecraft." + test, mode], check=True)


if __name__ == "__main__":
    main()
