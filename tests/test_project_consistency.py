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


def host_sources():
    """The .c files of the host build: the suites of tools/host_tests.json, which the shared executor (tools/run_host_tests.py) builds and runs."""
    data = json.loads(read("tools/host_tests.json"))
    found = set()
    for suite in data["suites"]:
        found.add(suite["test"])
        for entry in suite["sources"]:
            if entry.startswith("glob:"):
                found.update(p.relative_to(ROOT).as_posix() for p in ROOT.glob(entry[len("glob:"):]))
            else:
                found.add(entry)
    return found


class BuildScriptTests(unittest.TestCase):
    def test_both_scripts_call_the_shared_executor_with_the_one_list_of_suites(self):
        for script in ("build_firmware.sh", "build_firmware.bat"):
            text = read(script)
            self.assertRegex(text, r"run_host_tests\.py", script)
            self.assertIn("--coverage-floor", text, script)
            self.assertNotRegex(text, r"(?m)^[^#\n]*tests[/\\]test_[a-z0-9_]+\.c", f"{script} lists a test file itself")
        for name in host_sources():
            self.assertTrue((ROOT / name).is_file(), f"tools/host_tests.json lists {name}, which does not exist")
        self.assertFalse((ROOT / "tools" / "host_sources.txt").exists())
        self.assertFalse((ROOT / "tools" / "gcov_floor.py").exists())

    def test_every_module_and_test_file_is_in_the_host_build(self):
        listed = host_sources()
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

    def test_both_scripts_write_the_manifest_after_the_images(self):
        for script in ("build_firmware.sh", "build_firmware.bat"):
            text = read(script)
            self.assertIn("generate_manifest.py", text, script)
            self.assertGreater(text.index("generate_manifest.py"), text.index("URTC_SMART_RACK_FIRMWARE_v"), f"{script} writes the manifest before the images")

    def test_the_scripts_only_read_the_version(self):
        for script in ("build_firmware.sh", "build_firmware.bat"):
            text = read(script)
            self.assertNotIn("bump_manifest_version", text, script)
            self.assertNotRegex(text, r"(?m)^[^#\n]*\bpython3? bump_version\.py", script)
            self.assertIn("FIRMWARE_VERSION_PATCH", text, script)


class PublishedNumbersTests(unittest.TestCase):
    def test_assertion_count_in_manifest_and_readmes(self):
        count = 0
        for path in (ROOT / "tests").glob("test_*.c"):
            count += path.read_text(encoding="utf-8", errors="replace").count("TEST_ASSERT(")
        self.assertGreater(count, 0)

        for name in README_FILES:
            numbers = re.findall(r"(\d+)[^`\d\n]{0,24}`TEST_ASSERT`", read(name))
            self.assertTrue(numbers, f"{name} must state how many TEST_ASSERT checks back it")
            self.assertEqual(set(numbers), {str(count)}, f"{name} quotes a stale assertion count")


class PcbFieldsTests(unittest.TestCase):
    """The manifest carries the chip and the hardware identifier of the board this firmware is for. Empty means the board does not exist yet (pending); a value is
    only accepted in the exact shape the bootloader compares, and a manifest that claims evidence from a board must have them."""

    def manifest(self):
        return json.loads(read("urtc.project.json"))

    def test_the_manifest_has_both_fields_as_text(self):
        pcb = self.manifest().get("pcb")
        self.assertIsInstance(pcb, dict, "urtc.project.json needs a pcb block (empty values mean pending)")
        self.assertEqual(set(pcb), {"chip", "hardware_id"})
        for name, value in pcb.items():
            self.assertIsInstance(value, str, f"pcb.{name} must be text")
            self.assertEqual(value, value.strip(), f"pcb.{name} has spaces around it")

    def test_a_value_has_the_shape_the_bootloader_compares_and_empty_means_pending(self):
        pcb = self.manifest()["pcb"]
        if pcb["chip"]:
            self.assertRegex(pcb["chip"], r"^[A-Z0-9][A-Za-z0-9_-]{3,31}$", "pcb.chip is the part number, e.g. STM32G431CBU6")
        if pcb["hardware_id"]:
            self.assertRegex(pcb["hardware_id"], r"^0x[0-9A-F]{8}$", "pcb.hardware_id is 0x followed by 8 upper-case hexadecimal digits")
            self.assertNotEqual(int(pcb["hardware_id"], 16), 0, "a hardware identifier of 0 means none")
            self.assertNotEqual(int(pcb["hardware_id"], 16), 0xFFFFFFFF, "an erased flash reads as 0xFFFFFFFF, which is not an identifier")

    def test_evidence_from_a_board_needs_both_fields(self):
        manifest = self.manifest()
        if manifest["evidence"] in ("bench", "field", "hardware"):
            self.assertTrue(manifest["pcb"]["chip"] and manifest["pcb"]["hardware_id"], "evidence from a board needs the chip and the hardware identifier of that board")

    def test_the_fields_stay_empty_while_no_board_exists(self):
        manifest = self.manifest()
        if manifest["evidence"] == "simulator":
            self.assertEqual(manifest["pcb"], {"chip": "", "hardware_id": ""}, "a simulator-only repository does not know a board yet: fill the fields when the board exists and change the evidence with a record")


if __name__ == "__main__":
    unittest.main()
