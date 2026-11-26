import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch


class WindowsBundleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.bundle = Path(self.temp.name) / "LAB-RC.vst3"
        self.binary = self.bundle / "Contents/x86_64-win/LAB-RC.vst3"
        self.binary.parent.mkdir(parents=True)
        self.manifest = self.bundle / "Contents/Resources/moduleinfo.json"
        self.manifest.parent.mkdir()
        self.manifest.write_text(json.dumps({
            "Name": "LAB-RC", "Version": "0.2.0", "Factory Info": {"Vendor": "VITAPLUG"},
        })[:-1] + ",}", encoding="utf-8")
        self.write_binary()

    def write_binary(self, machine=0x8664, magic=0x20B, flags=0x2000):
        image = bytearray(256)
        image[:2] = b"MZ"
        struct.pack_into("<I", image, 0x3C, 64)
        image[64:68] = b"PE\x00\x00"
        struct.pack_into("<HHIIIHHH", image, 68, machine, 1, 0, 0, 0, 112, flags, magic)
        self.binary.write_bytes(image)

    def test_accepts_amd64_dll_and_juce_manifest(self):
        from build_windows import verify_bundle
        with patch("build_windows.inspect_exports_and_imports") as inspect:
            self.assertEqual(verify_bundle(self.bundle, Path("dumpbin.exe")), self.binary)
        inspect.assert_called_once_with(Path("dumpbin.exe"), self.binary)

    def test_rejects_other_architectures_and_plain_executables(self):
        from build_windows import verify_bundle
        for machine, magic, flags in [(0xAA64, 0x20B, 0x2000), (0x8664, 0x10B, 0x2000), (0x8664, 0x20B, 0)]:
            with self.subTest(machine=machine, magic=magic, flags=flags):
                self.write_binary(machine, magic, flags)
                with self.assertRaisesRegex(ValueError, "PE32\\+ DLL AMD64"):
                    verify_bundle(self.bundle, Path("dumpbin.exe"))

    def test_rejects_wrong_plugin_version(self):
        from build_windows import verify_bundle
        text = self.manifest.read_text(encoding="utf-8").replace("0.2.0", "0.1.0")
        self.manifest.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "moduleinfo"):
            verify_bundle(self.bundle, Path("dumpbin.exe"))

    def test_rejects_missing_factory_and_dynamic_cpp_runtime(self):
        from build_windows import inspect_exports_and_imports
        for output, error in [("InitDll", "GetPluginFactory"),
                              ("GetPluginFactory\n    MSVCP140.dll", "runtime")]:
            with self.subTest(output=output), patch("build_windows.subprocess.check_output", return_value=output):
                with self.assertRaisesRegex(ValueError, error):
                    inspect_exports_and_imports(Path("dumpbin.exe"), self.binary)

    def test_pluginval_requires_runtime_version_and_success(self):
        from build_windows import verify_pluginval_log
        log = Path(self.temp.name) / "pluginval.log"
        successful = "Num plugins found: 1\nVITAPLUG: LAB-RC v0.2.0\nSUCCESS\n"
        log.write_text(successful, encoding="utf-8")
        verify_pluginval_log(log)
        for old, new in [("found: 1", "found: 0"), ("v0.2.0", "v0.1.0"), ("SUCCESS", "FAILED")]:
            with self.subTest(old=old):
                log.write_text(successful.replace(old, new), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "pluginval"):
                    verify_pluginval_log(log)


if __name__ == "__main__":
    unittest.main()
