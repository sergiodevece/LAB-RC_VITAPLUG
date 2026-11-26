#!/usr/bin/env python3
"""Exercise offline bootstrap without a tar executable and reject unsafe entries."""
import hashlib
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import prepare_juce

ROOT = Path(__file__).resolve().parents[1]
PREFIX = "JUCE-8.0.17-security/"


class PrepareJuceTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "build").mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="bootstrap-tests-", dir=ROOT / "build")
        self.folder = Path(self.temporary.name)
        self.archive = self.folder / "fixture.tar.gz"
        self.destination = self.folder / "dependencies/juce"

    def tearDown(self):
        self.temporary.cleanup()

    def make_archive(self, unsafe=None, link=False):
        with tarfile.open(self.archive, "w:gz") as output:
            for name, data in ((PREFIX + "CMakeLists.txt", b"project(JUCE VERSION 8.0.17 LANGUAGES C CXX)\n"),
                               (PREFIX + "modules/fixture.txt", b"pinned local source\n")):
                info = tarfile.TarInfo(name)
                info.size = len(data)
                output.addfile(info, io.BytesIO(data))
            if unsafe:
                info = tarfile.TarInfo(PREFIX + unsafe)
                if link:
                    info.type = tarfile.SYMTYPE
                    info.linkname = "../../outside.txt"
                    output.addfile(info)
                else:
                    info.size = 7
                    output.addfile(info, io.BytesIO(b"escaped"))
        return hashlib.sha256(self.archive.read_bytes()).hexdigest()

    def test_prepares_the_pinned_source_when_external_tar_is_unavailable(self):
        digest = self.make_archive()
        with patch.object(prepare_juce, "PIN", digest), patch.object(prepare_juce, "DESTINATION", self.destination):
            with patch("subprocess.run", side_effect=FileNotFoundError("tar unavailable on this host")):
                try:
                    prepare_juce.prepare(self.archive)
                except FileNotFoundError:
                    self.fail("Local pinned JUCE preparation must not require /usr/bin/tar")
        self.assertEqual((self.destination / "modules/fixture.txt").read_bytes(), b"pinned local source\n")
        self.assertEqual((self.destination / ".labrc-archive-sha256").read_text().strip(), digest)

    def test_wrong_hash_does_not_create_a_destination(self):
        self.make_archive()
        with patch.object(prepare_juce, "DESTINATION", self.destination):
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                prepare_juce.prepare(self.archive)
        self.assertFalse(self.destination.exists())

    def test_parent_traversal_is_rejected_before_extraction(self):
        self.assert_unsafe_rejected("../outside.txt")

    def test_symlink_is_rejected_before_extraction(self):
        self.assert_unsafe_rejected("unsafe-link", link=True)

    def assert_unsafe_rejected(self, name, link=False):
        digest = self.make_archive(name, link)
        with patch.object(prepare_juce, "PIN", digest), patch.object(prepare_juce, "DESTINATION", self.destination):
            try:
                prepare_juce.prepare(self.archive)
            except ValueError:
                pass
            except Exception as error:
                self.fail(f"Bootstrap must reject unsafe entries itself: {error}")
            else:
                self.fail("Unsafe archive entry was accepted")
        self.assertFalse(self.destination.exists())
        self.assertFalse((self.folder / "dependencies/outside.txt").exists())


if __name__ == "__main__":
    unittest.main()
