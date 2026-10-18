"""Checks that keep the build scripts and the published numbers honest.

- The Linux and Windows build scripts must compile the same host test sources:
  a module added to one list and forgotten in the other broke the Windows
  build once, silently, because only the Linux script was ever run.
- Every module under src/ and every test file under tests/ must be part of that
  list, so a new file cannot be left out of the build.
- The number of test assertions quoted in the manifest and in the READMEs must be
  the number of `TEST_ASSERT(` checks that really exist in tests/.
"""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
README_FILES = (
    "README.md", "README_spa.md", "README_fra.md", "README_ita.md",
    "README_deu.md", "README_zho.md", "README_jpn.md", "README_por.md",
)


def read(name):
    return (ROOT / name).read_text(encoding="utf-8", errors="replace")


def host_sources_of(script):
    """The .c files given to the host compiler in one build script."""
    text = read(script).replace("\\\r\n", " ").replace("\\\n", " ")
    for line in text.splitlines():
        if re.search(r"\b(gcc|HOST_CC)\b.*-o\s+\S*host_tests", line):
            return {Path(token.replace("\\", "/")).as_posix() for token in line.split() if token.endswith(".c")}
    raise AssertionError(f"{script} has no host test compile command")


class BuildScriptTests(unittest.TestCase):
    def test_both_scripts_compile_the_same_host_sources(self):
        self.assertEqual(host_sources_of("build_firmware.sh"), host_sources_of("build_firmware.bat"))

    def test_every_module_and_test_file_is_in_the_host_build(self):
        listed = host_sources_of("build_firmware.sh")
        modules = {f"src/{p.name}" for p in (ROOT / "src").glob("*.c")} - {
            "src/main.c", "src/startup_stm32g4_minimal.c",
        }
        tests = {f"tests/{p.name}" for p in (ROOT / "tests").glob("test_*.c")}
        self.assertEqual(modules - listed, set(), "modules missing from the host build")
        self.assertEqual(tests - listed, set(), "test files missing from the host build")

    def test_the_arm_build_compiles_every_module(self):
        for script in ("build_firmware.sh", "build_firmware.bat"):
            text = read(script)
            self.assertRegex(text, r"src[/\\]\*\.c", f"{script} must compile every src/*.c for the target")

    def test_the_bat_honours_the_ci_switch_and_restores_the_version(self):
        text = read("build_firmware.bat")
        self.assertIn('"%URTC_CI%"=="1" goto :version_from_header', text)
        self.assertIn("version_backup", text)


class PublishedNumbersTests(unittest.TestCase):
    def test_assertion_count_in_manifest_and_readmes(self):
        count = 0
        for path in (ROOT / "tests").glob("test_*.c"):
            count += path.read_text(encoding="utf-8", errors="replace").count("TEST_ASSERT(")
        self.assertGreater(count, 0)

        manifest = json.loads(read("urtc.project.json"))
        quoted = re.findall(r"(\d+) host-native test assertions", manifest["notes"])
        self.assertEqual(quoted, [str(count)], "manifest notes must quote the real assertion count")

        for name in README_FILES:
            numbers = re.findall(r"(\d+)[^`\d\n]{0,24}`TEST_ASSERT`", read(name))
            self.assertTrue(numbers, f"{name} must state how many TEST_ASSERT checks back it")
            self.assertEqual(set(numbers), {str(count)}, f"{name} quotes a stale assertion count")


if __name__ == "__main__":
    unittest.main()
