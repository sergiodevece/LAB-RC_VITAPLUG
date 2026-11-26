import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import zipfile


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

    def test_steinberg_validator_requires_nonempty_success(self):
        from build_windows import verify_steinberg_log
        log = Path(self.temp.name) / "steinberg-validator.log"
        log.write_text("Result: 219 tests passed, 0 tests failed\n", encoding="utf-8")
        self.assertEqual(verify_steinberg_log(log), 219)
        for result in ("Result: 0 tests passed, 0 tests failed", "Result: 23 tests passed, 1 tests failed", ""):
            with self.subTest(result=result):
                log.write_text(result, encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "VST3 Validator"):
                    verify_steinberg_log(log)

    def test_release_zip_contains_installer_bundle_and_verified_checksum(self):
        from build_windows import package, sha256
        root = Path(self.temp.name)
        for relative in ("scripts/windows/install.ps1", "docs/WINDOWS_SMOKE.md", "README.md", "CHANGELOG.md",
                         "LICENSE.md", "build/dependencies/juce/LICENSE.md",
                         "build/dependencies/juce/modules/juce_audio_processors_headless/format_types/"
                         "VST3_SDK/LICENSE.txt", "evidence/benchmark.log"):
            fixture = root / relative
            fixture.parent.mkdir(parents=True, exist_ok=True)
            fixture.write_text(relative, encoding="utf-8")
        with patch("build_windows.ROOT", root):
            package(self.bundle, self.binary, root / "evidence", "PASS", "PENDIENTE")
        release = root / "dist/LAB-RC-Windows-x64-VST3.zip"
        self.assertEqual(release.with_suffix(".zip.sha256").read_text().split()[0], sha256(release))
        with zipfile.ZipFile(release) as archive:
            self.assertIsNone(archive.testzip())
            for relative in ("install.ps1", "INSTALACION.md", "README.md", "CHANGELOG.md", "INFORME.md",
                             "evidence/benchmark.log", self.binary.relative_to(root).as_posix()):
                self.assertIn(f"{release.stem}/{relative}", archive.namelist())
            receipt = json.loads(archive.read(f"{release.stem}/build-info.json"))
            self.assertEqual(receipt["binary_sha256"], sha256(self.binary))
            self.assertEqual(receipt["steinberg_validator"], "PENDIENTE")


if __name__ == "__main__":
    unittest.main()
