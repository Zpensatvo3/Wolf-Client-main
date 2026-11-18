"""Guard the MSBuild/sccache integration; execution is also tested on Windows."""
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}


class BuildCacheTest(unittest.TestCase):
    def test_cache_mode_disables_pch_and_compiler_batching_only_when_selected(self):
        project = ET.parse(ROOT / "OneStep.vcxproj").getroot()
        release = next(group for group in project.findall("m:ItemDefinitionGroup", NS)
                       if group.get("Condition") == "'$(Configuration)|$(Platform)'=='Release|x64'")
        compile_options = release.find("m:ClCompile", NS)
        for name, normal, cached in (("PrecompiledHeader", "Use", "NotUsing"),
                                     ("MultiProcessorCompilation", "true", "false")):
            options = compile_options.findall("m:" + name, NS)
            self.assertTrue(any(option.text == normal and not option.get("Condition") for option in options))
            self.assertTrue(any(option.text == cached and option.get("Condition") == "'$(CompilerCache)' == 'true'"
                                for option in options))
        pch_source = next(item for item in project.findall(".//m:ClCompile", NS)
                          if item.get("Include") == r"src\pch.cpp")
        self.assertIn("'$(CompilerCache)' != 'true'", pch_source[0].get("Condition"))

    def test_both_workflows_use_cache_without_forcing_rebuild(self):
        for filename in ("build.yml", "build-1.21.11.yml"):
            with self.subTest(workflow=filename):
                workflow = (ROOT / ".github/workflows" / filename).read_text(encoding="utf-8")
                command = next(line for line in workflow.splitlines() if "run: ./scripts/Build-Release.ps1" in line)
                self.assertIn("-CompilerCache", command)
                self.assertIn("-Timing", command)
                self.assertNotIn("-Clean", command)
                self.assertIn("SCCACHE_GHA_ENABLED: 'true'", workflow)
                self.assertIn("mozilla-actions/sccache-action@v0.0.11", workflow)
                self.assertIn("./scripts/Test-CompilerCache.ps1", workflow)
                self.assertIn("needs: offline-tests", workflow)


if __name__ == "__main__":
    unittest.main()
