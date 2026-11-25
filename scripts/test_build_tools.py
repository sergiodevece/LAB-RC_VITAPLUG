#!/usr/bin/env python3
"""Filesystem behavior checks. Every fixture lives under this workspace's build/."""
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BundleToolTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "build").mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="bundle-tool-tests-", dir=ROOT / "build")
        self.folder = Path(self.temporary.name)
        self.components = self.folder / "Audio with spaces" / "Components"
        self.source = self.folder / "source" / "LAB-RC.component"
        contents = self.source / "Contents"
        (contents / "MacOS").mkdir(parents=True)
        self.info = {
            "CFBundleIdentifier": "com.vitaplug.labrc", "CFBundleExecutable": "LAB-RC",
            "CFBundlePackageType": "BNDL", "CFBundleVersion": "0.1.0",
            "CFBundleShortVersionString": "0.1.0",
        }
        (contents / "Info.plist").write_bytes(plistlib.dumps(self.info))
        shutil.copyfile("/usr/bin/true", contents / "MacOS" / "LAB-RC")
        (contents / "MacOS" / "LAB-RC").chmod(0o755)
        subprocess.run(["/usr/bin/codesign", "--force", "--sign", "-", str(self.source)],
                       check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def tearDown(self):
        self.temporary.cleanup()

    def run_tool(self, name, *arguments):
        return subprocess.run(["/bin/bash", str(ROOT / "scripts" / name), *map(str, arguments)],
                              cwd=ROOT, capture_output=True, text=True)

    def install(self, *extra):
        return self.run_tool("install_macos.sh", self.source, "--components-dir", self.components, *extra)

    def uninstall(self, *extra):
        return self.run_tool("uninstall_macos.sh", "--components-dir", self.components, *extra)

    def test_install_and_remove_only_labrc_with_quoted_paths(self):
        self.components.mkdir(parents=True)
        unrelated = self.components / "Other.component"
        unrelated.mkdir()
        (unrelated / "keep.txt").write_text("keep")
        result = self.install()
        self.assertEqual(result.returncode, 0, result.stderr)
        target = self.components / "LAB-RC.component"
        self.assertTrue((target / "Contents" / "MacOS" / "LAB-RC").is_file())
        result = self.uninstall()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(target.exists())
        self.assertEqual((unrelated / "keep.txt").read_text(), "keep")

    def test_dry_run_never_creates_destination(self):
        result = self.install("--dry-run")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(self.components.exists())

    def test_foreign_bundle_is_preserved_on_install_and_uninstall(self):
        target = self.components / "LAB-RC.component"
        (target / "Contents").mkdir(parents=True)
        info = target / "Contents" / "Info.plist"
        info.write_bytes(plistlib.dumps({"CFBundleIdentifier": "example.foreign"}))
        self.assertNotEqual(self.install().returncode, 0)
        self.assertNotEqual(self.uninstall().returncode, 0)
        self.assertEqual(plistlib.loads(info.read_bytes())["CFBundleIdentifier"], "example.foreign")

    def test_symlink_target_is_rejected_without_following_it(self):
        self.components.mkdir(parents=True)
        target = self.components / "LAB-RC.component"
        target.symlink_to(self.source, target_is_directory=True)
        self.assertNotEqual(self.install().returncode, 0)
        self.assertNotEqual(self.uninstall().returncode, 0)
        self.assertTrue(target.is_symlink())
        self.assertTrue((self.source / "Contents" / "Info.plist").is_file())

    def test_invalid_destination_is_rejected_without_mutation(self):
        invalid = self.folder / "not-components"
        result = self.run_tool("install_macos.sh", self.source, "--components-dir", invalid)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(invalid.exists())


if __name__ == "__main__":
    unittest.main()
