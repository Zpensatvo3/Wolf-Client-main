"""Validate the embedded View Model transformer against a local client classfile, without running Minecraft."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--client-jar", type=Path, required=True)
parser.add_argument("--target", choices=("12101", "12104", "12108", "12111", "26100", "26101", "26102"), default="12111")
parser.add_argument("--asm-classpath", required=True, help="ASM core and tree jars, separated by the platform path separator")
args = parser.parse_args()
if not args.client_jar.is_file(): parser.error("Local client jar not found")
java, javac = shutil.which("java"), shutil.which("javac")
if not java or not javac: parser.error("A JDK is required")
classpath = os.pathsep.join([str(ROOT / "assets/viewmodel"), args.asm_classpath])
with tempfile.TemporaryDirectory(prefix="onestep-viewmodel-test-") as directory:
    subprocess.run([javac, "--release", "17", "-cp", classpath, "-d", directory,
                    str(ROOT / "tests/ViewModelTransformerTest.java")], check=True)
    subprocess.run([java, "-ea", "-Xverify:all", "-cp", os.pathsep.join([directory, classpath]),
                    "ViewModelTransformerTest", str(args.client_jar.resolve()), args.target], check=True)
