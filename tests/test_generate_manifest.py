"""generate_manifest.py: the version comes from the source, only that version is indexed, and the checksums are those of the shipped bytes."""
import hashlib
import importlib.util
import json
import tempfile
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
_spec = importlib.util.spec_from_file_location("rack_generate_manifest", ROOT / "generate_manifest.py")
assert _spec is not None and _spec.loader is not None
gen = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(gen)

HEADER = "#define FIRMWARE_VERSION_MAJOR 0\n#define FIRMWARE_VERSION_MINOR 2\n#define FIRMWARE_VERSION_PATCH 7\n#define FIRMWARE_VERSION_BUILD 0\n"
IMAGE = b"image-0.2.7"


def project(folder: Path, with_hex: bool = True) -> Path:
    (folder / "src").mkdir()
    (folder / "src" / "firmware_common.h").write_text(HEADER, encoding="utf-8")
    firmware = folder / "firmware"
    firmware.mkdir()
    (firmware / "URTC_SMART_RACK_FIRMWARE_v0.2.7.bin").write_bytes(IMAGE)
    (firmware / "URTC_SMART_RACK_FIRMWARE_v0.2.6.bin").write_bytes(b"older image")     # lingers from an earlier build
    (firmware / "URTC_SMART_RACK_FIRMWARE_v0.2.7.elf").write_bytes(b"elf-0.2.7")
    if with_hex:
        (firmware / "URTC_SMART_RACK_FIRMWARE_v0.2.7.hex").write_bytes(b":00000001FF\n")
    return firmware


class GenerateManifestTests(unittest.TestCase):
    def test_the_manifest_indexes_the_version_of_the_source_only(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            firmware = project(root)
            manifest = gen.build_manifest(root, firmware, {"SOURCE_DATE_EPOCH": "1700000000"})
            self.assertEqual((manifest["project"], manifest["version"], manifest["generated_at"]), ("URTC-SMART-RACK", "0.2.7", "2023-11-14T22:13:20Z"))
            binary = manifest["files"]["bin"]
            self.assertEqual(binary["filename"], "URTC_SMART_RACK_FIRMWARE_v0.2.7.bin")
            self.assertEqual((binary["size_bytes"], binary["crc32"], binary["sha256"]),
                             (len(IMAGE), "0x%08X" % (zlib.crc32(IMAGE) & 0xFFFFFFFF), hashlib.sha256(IMAGE).hexdigest()))
            self.assertEqual(manifest["files"]["elf"]["filename"], "URTC_SMART_RACK_FIRMWARE_v0.2.7.elf")
            self.assertNotIn("crc32", manifest["files"]["hex"])

    def test_a_missing_hex_is_null_and_a_missing_bin_is_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            firmware = project(root, with_hex=False)
            self.assertIsNone(gen.build_manifest(root, firmware, {"SOURCE_DATE_EPOCH": "1"})["files"]["hex"])
            (firmware / "URTC_SMART_RACK_FIRMWARE_v0.2.7.bin").unlink()
            with self.assertRaises(SystemExit):
                gen.build_manifest(root, firmware, {})

    def test_a_header_without_the_version_is_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            project(root)
            (root / "src" / "firmware_common.h").write_text("#define FIRMWARE_VERSION_MAJOR 0\n", encoding="utf-8")
            with self.assertRaises(SystemExit):
                gen.read_version(root / "src" / "firmware_common.h")

    def test_the_time_is_reproducible_and_a_bad_epoch_is_refused(self):
        self.assertEqual(gen.build_timestamp(ROOT, {"SOURCE_DATE_EPOCH": "0"}), "1970-01-01T00:00:00Z")
        with self.assertRaises(SystemExit):
            gen.build_timestamp(ROOT, {"SOURCE_DATE_EPOCH": "tomorrow"})
        self.assertRegex(gen.build_timestamp(ROOT, {}), r"^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ$")

    def test_the_command_writes_the_file_next_to_the_images(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            firmware = project(root)
            self.assertEqual(gen.main([str(root), str(firmware)]), 0)
            written = json.loads((firmware / "firmware_manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(written["version"], "0.2.7")

    def test_the_real_header_has_a_readable_version(self):
        self.assertRegex(gen.read_version(ROOT / "src" / "firmware_common.h"), r"^\d+\.\d+\.\d+$")


if __name__ == "__main__":
    unittest.main()
