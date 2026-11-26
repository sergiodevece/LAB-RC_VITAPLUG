#!/usr/bin/env python3
"""Package the explicit LAB-RC source whitelist, never build trees or dependencies."""
import hashlib
import json
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import zipfile
from verify_macos import EXPECTED_VERSION

ROOT = Path(__file__).resolve().parents[1]
NAME = f"LAB-RC-{EXPECTED_VERSION}-sources"
DIRECTORIES = ("Source", "Tests", "Assets", "cmake", "scripts", "docs")
FILES = ("README.md", "LICENSE.md", "CHANGELOG.md", "CMakeLists.txt", "CMakePresets.json")
EXCLUDED = {"build", ".git", ".superpowers", "dist", "__pycache__", ".DS_Store", "qa-review.diff"}


def source_paths():
    paths = []
    for name in DIRECTORIES:
        directory = ROOT / name
        if directory.is_symlink() or not directory.is_dir():
            raise ValueError(f"Required source directory missing or symlinked: {directory}")
        for path in directory.rglob("*"):
            relative = path.relative_to(ROOT)
            if any(part in EXCLUDED for part in relative.parts) or path.suffix in (".pyc", ".pyo"):
                continue
            if relative.parts[:2] == ("docs", "briefs") and path.suffix == ".diff":
                continue
            if path.is_symlink():
                raise ValueError(f"Source whitelist contains a symlink: {relative}")
            if path.is_file():
                paths.append(path)
    for name in FILES:
        path = ROOT / name
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"Required source document missing or symlinked: {path}")
        paths.append(path)
    return sorted(paths)


def archive_entry(archive, relative, data, executable=False):
    info = zipfile.ZipInfo(f"{NAME}/{relative}", date_time=(2026, 11, 25, 0, 0, 0))
    info.create_system = 3
    info.external_attr = (stat.S_IFREG | (0o755 if executable else 0o644)) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    archive.writestr(info, data, compresslevel=6)


def make_package():
    subprocess.run([sys.executable, "-B", str(ROOT / "scripts/verify_assets.py"),
                    "--final-touch", str(ROOT / "Assets/FinalTouch")], cwd=ROOT, check=True)
    paths = source_paths()
    destination = ROOT / "dist" / (NAME + ".zip")
    destination.parent.mkdir(exist_ok=True)
    (ROOT / "build").mkdir(exist_ok=True)
    manifest = {"product": "LAB-RC", "version": EXPECTED_VERSION, "files": [],
                "JUCE": "External pinned local JUCE 8.0.17-security; dependency not included",
                "JUCE_archive_sha256": "d37bab9f2330c50f42ba979575f6b55d5d3b23fa94221c2e57cac3abb69ac6d9"}
    with tempfile.TemporaryDirectory(prefix="labrc-source-package-", dir=ROOT / "build") as temporary:
        staging = Path(temporary) / destination.name
        with zipfile.ZipFile(staging, "w") as archive:
            for path in paths:
                relative = path.relative_to(ROOT).as_posix()
                data = path.read_bytes()
                manifest["files"].append({"path": relative, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
                archive_entry(archive, relative, data, executable=bool(path.stat().st_mode & 0o111))
            archive_entry(archive, "SOURCE_MANIFEST.json", (json.dumps(manifest, indent=2) + "\n").encode())
        with zipfile.ZipFile(staging) as archive:
            failed = archive.testzip()
            if failed:
                raise ValueError(f"Source archive CRC failure: {failed}")
        staging.replace(destination)
    with destination.open("rb") as source:
        digest = hashlib.file_digest(source, "sha256").hexdigest() if hasattr(hashlib, "file_digest") else hashlib.sha256(source.read()).hexdigest()
    destination.with_suffix(".zip.sha256").write_text(f"{digest}  {destination.name}\n")
    print(f"Source package: {destination}\nFiles: {len(paths)} plus SOURCE_MANIFEST.json\nSHA-256: {digest}")


if __name__ == "__main__":
    try:
        make_package()
    except (OSError, ValueError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        print(f"package_sources: {error}", file=sys.stderr)
        sys.exit(1)
