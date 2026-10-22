"""Run the live Java Crystal Aura against isolated game/client-tick fixtures."""
from pathlib import Path
import os
import subprocess
import tempfile
from world_binding_fixture import write_native_bindings

ROOT = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="onestep-crystal-aura-") as work:
    bindings = write_native_bindings(Path(work) / "native-bindings.txt")
    sources = [ROOT / "src/bridge/WorldBridge.java", ROOT / "src/bridge/AnchorEvents.java",
               ROOT / "tests/hybrid/GLFW.java", *sorted((ROOT / "tests/crystal_aura").glob("*.java"))]
    subprocess.run(["javac", "--release", "17", "-encoding", "UTF-8", "-g:none", "-d", work,
                    *map(str, sources)], cwd=ROOT, check=True)
    for label, cp in (("source", work), ("embedded", os.pathsep.join((str(ROOT / "assets/hybrid"), work)))):
        subprocess.run(["java", "-Xverify:all", "-cp", cp, "net.minecraft.CrystalAuraBridgeTest", label, str(bindings)], cwd=ROOT, check=True)
