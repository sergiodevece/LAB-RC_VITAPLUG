#!/usr/bin/env python3
"""Prepare only the pinned, already local JUCE archive. Never uses the network."""
import argparse
import hashlib
from pathlib import Path, PurePosixPath
import shutil
import sys
import tarfile
import tempfile

PIN = "d37bab9f2330c50f42ba979575f6b55d5d3b23fa94221c2e57cac3abb69ac6d9"
ROOT = Path(__file__).resolve().parents[1]
DESTINATION = ROOT / "build" / "dependencies" / "juce"


def extract_archive(archive: Path, staging: Path) -> None:
    """Extract regular files/directories only, with all paths validated first."""
    with tarfile.open(archive, "r:gz") as source:
        entries = []
        seen = set()
        for member in source.getmembers():
            name = PurePosixPath(member.name)
            if (name.is_absolute() or not name.parts
                    or name.parts[0] != "JUCE-8.0.17-security"
                    or ".." in name.parts or "\\" in member.name
                    or any(":" in part or "\x00" in part for part in name.parts)):
                raise ValueError(f"Unsafe JUCE archive path: {member.name}")
            if not (member.isdir() or member.isfile()):
                raise ValueError(f"Unsupported JUCE archive entry type: {member.name}")
            relative = Path(*name.parts[1:])
            if not name.parts[1:]:
                continue
            key = relative.as_posix().casefold()
            if key in seen:
                raise ValueError(f"Duplicate JUCE archive path: {member.name}")
            seen.add(key)
            destination = staging / relative
            if not destination.resolve().is_relative_to(staging.resolve()):
                raise ValueError(f"JUCE archive escapes its destination: {member.name}")
            entries.append((member, destination))
        for member, destination in entries:
            if member.isdir():
                destination.mkdir(parents=True, exist_ok=True)
            else:
                destination.parent.mkdir(parents=True, exist_ok=True)
                data = source.extractfile(member)
                if data is None:
                    raise ValueError(f"Missing archive data: {member.name}")
                with data, destination.open("xb") as output:
                    shutil.copyfileobj(data, output)
                destination.chmod(member.mode & 0o777)


def prepare(archive: Path) -> None:
    archive = archive.resolve(strict=True)
    with archive.open("rb") as source:
        actual = hashlib.file_digest(source, "sha256").hexdigest() if hasattr(hashlib, "file_digest") else hashlib.sha256(source.read()).hexdigest()
    if actual != PIN:
        raise ValueError(f"JUCE SHA-256 mismatch: {actual}; expected {PIN}")
    if any(path.is_symlink() for path in (DESTINATION, *DESTINATION.parents)):
        raise ValueError("Refusing a symlink at the JUCE destination or its parents")
    marker = DESTINATION / ".labrc-archive-sha256"
    if DESTINATION.exists():
        if marker.is_file() and marker.read_text().strip() == PIN:
            if "project(JUCE VERSION 8.0.17 LANGUAGES C CXX)" not in (DESTINATION / "CMakeLists.txt").read_text():
                raise ValueError("Prepared source no longer has the pinned JUCE version")
            print(f"JUCE already prepared: {DESTINATION}")
            return
        raise ValueError("JUCE destination already exists without matching provenance; leave it intact and inspect it")
    DESTINATION.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=".juce-stage-", dir=DESTINATION.parent))
    try:
        extract_archive(archive, staging)
        cmake = (staging / "CMakeLists.txt").read_text()
        if "project(JUCE VERSION 8.0.17 LANGUAGES C CXX)" not in cmake:
            raise ValueError("Pinned archive has an unexpected JUCE version")
        (staging / ".labrc-archive-sha256").write_text(PIN + "\n")
        staging.rename(DESTINATION)
    finally:
        if staging.exists():
            shutil.rmtree(staging)
    print(f"Prepared JUCE 8.0.17-security: {DESTINATION}\nSHA-256 {PIN}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True, help="Existing local juce-8.0.17-security.tar.gz")
    args = parser.parse_args()
    try:
        prepare(args.archive)
    except (OSError, ValueError, tarfile.TarError) as error:
        print(f"prepare_juce: {error}", file=sys.stderr)
        sys.exit(1)
