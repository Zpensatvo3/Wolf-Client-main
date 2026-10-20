"""Compile the native auth configuration guard locally, without network access."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}


def main():
    props = ET.parse(ROOT / "server/AuthDeployment.props")
    domain = props.find(".//m:AuthServerDomain", NS).text
    public = props.find(".//m:AuthPublicKeySpkiBase64", NS).text
    assert domain == "onestep-auth.gogogamesas.workers.dev"
    project = ET.parse(ROOT / "OneStep.vcxproj")
    defaults = project.findall(".//m:AuthRequired", NS)
    assert defaults and defaults[0].text == "true", "Production auth must fail closed if deployment props are missing"
    for name in ["OneStep.vcxproj", "launcher/launcher.vcxproj", "src/auth/auth.cpp", "src/auth/config.hpp"]:
        text = (ROOT / name).read_text(encoding="utf-8")
        assert "WOLF_AUTH_" not in text, name
        assert "ONESTEP_AUTH_" in text, name
    compiler = shutil.which(os.environ.get("CXX", "cl" if os.name == "nt" else "g++"))
    if not compiler:
        raise SystemExit("A C++20 compiler is required for auth configuration validation")
    msvc = Path(compiler).name.lower() in {"cl", "cl.exe", "clang-cl", "clang-cl.exe"}
    with tempfile.TemporaryDirectory(prefix="onestep-auth-config-") as temporary:
        source = Path(temporary) / "config_probe.cpp"
        source.write_text('#include "src/auth/config.hpp"\nint main() {}\n', encoding="utf-8")
        required = {"ONESTEP_AUTH_REQUIRED": "1"}
        complete = {**required, "ONESTEP_AUTH_SERVER_DOMAIN": f'"{domain}"', "ONESTEP_AUTH_PUBLIC_KEY_SPKI_BASE64": f'"{public}"'}
        cases = [("production metadata", complete, True), ("missing default metadata", {}, False),
                 ("default requires authentication", {k:v for k,v in complete.items() if k != "ONESTEP_AUTH_REQUIRED"}, True),
                 ("unmarked bypass rejected", {"ONESTEP_AUTH_REQUIRED":"0"}, False),
                 ("isolated test probe", {"ONESTEP_AUTH_REQUIRED":"0", "ONESTEP_AUTH_INSECURE_TEST_BUILD":"1"}, True),
                 ("release bypass rejected", {"ONESTEP_AUTH_REQUIRED":"0", "ONESTEP_AUTH_INSECURE_TEST_BUILD":"1", "NDEBUG":"1"}, False),
                 ("invalid auth define", {**complete,"ONESTEP_AUTH_REQUIRED":"2"}, False),
                 ("missing production metadata", required, False),
                 ("missing domain", {k: v for k, v in complete.items() if k != "ONESTEP_AUTH_SERVER_DOMAIN"}, False),
                 ("missing public key", {k: v for k, v in complete.items() if k != "ONESTEP_AUTH_PUBLIC_KEY_SPKI_BASE64"}, False)]
        for invalid in ["wolf-auth-api.gogogamesas.workers.dev", "WOLF-AUTH-API.GOGOGAMESAS.WORKERS.DEV", "https://auth.example.com", "auth.example.com/path", "auth..example.com", "-auth.example.com", "auth-.example.com"]:
            cases.append(("reject invalid/legacy domain", {**complete, "ONESTEP_AUTH_SERVER_DOMAIN": f'"{invalid}"'}, False))
        for title, definitions, success in cases:
            command = [compiler] + (["/nologo", "/std:c++20", "/Zs", f"/I{ROOT}"] if msvc else ["-std=c++20", "-fsyntax-only", "-I", str(ROOT)])
            command += [("/D" if msvc else "-D") + key + "=" + value for key, value in definitions.items()]
            result = subprocess.run(command + [str(source)], capture_output=True, text=True, cwd=temporary)
            if (result.returncode == 0) != success:
                raise SystemExit(f"FAIL {title}\n{result.stdout}{result.stderr}")
        print(f"Auth configuration passed: {len(cases)} compiler scenarios; explicit OneStep domain, no Wolf fallback, fail-closed production metadata.")


if __name__ == "__main__":
    main()
