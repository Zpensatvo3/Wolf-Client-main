"""Verify the real Anchor tick/use bytecode hooks without Minecraft or native injection."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--asm-classpath", required=True, help="ASM and ASM Tree 9.9+ jars, separated by the platform classpath delimiter")
parser.add_argument("--source-only", action="store_true")
args = parser.parse_args()
java, javac = shutil.which("java"), shutil.which("javac")
if not java or not javac:
    raise SystemExit("JDK 17+ is required")
with tempfile.TemporaryDirectory(prefix="onestep-anchor-hooks-") as work:
    classes = Path(work) / "classes"
    subprocess.run([javac, "--release", "17", "-encoding", "UTF-8", "-g:none", "-cp", args.asm_classpath,
                    "-d", str(classes), str(ROOT / "src/bridge/AnchorEvents.java"),
                    str(ROOT / "src/bridge/AnchorTransformer.java"), str(ROOT / "tests/anchor_hooks/AnchorHooksTest.java")], check=True)
    modes = [("source", os.pathsep.join((str(classes), args.asm_classpath)))]
    if not args.source_only:
        modes.append(("embedded", os.pathsep.join((str(ROOT / "assets/hybrid"), str(classes), args.asm_classpath))))
    for mode, cp in modes:
        subprocess.run([java, "-Xverify:all", "-cp", cp, "net.onestep.tests.AnchorHooksTest", mode], check=True)
