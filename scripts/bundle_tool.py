#!/usr/bin/env python3
"""Explicit per-user AU install/remove; touches only LAB-RC.component. No sudo."""
import argparse
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile

BUNDLE_NAME = "LAB-RC.component"
BUNDLE_ID = "com.vitaplug.labrc"


def reject_symlink_path(path: Path) -> None:
    if any(part.is_symlink() for part in (path, *path.parents)):
        raise ValueError(f"Refusing symlink path: {path}")


def check_bundle(bundle: Path, signature: bool = False) -> None:
    reject_symlink_path(bundle)
    if bundle.name != BUNDLE_NAME or not bundle.is_dir():
        raise ValueError(f"Expected an existing {BUNDLE_NAME}: {bundle}")
    info_file = bundle / "Contents" / "Info.plist"
    reject_symlink_path(info_file)
    with info_file.open("rb") as source:
        info = plistlib.load(source)
    if info.get("CFBundleIdentifier") != BUNDLE_ID:
        raise ValueError(f"Refusing foreign bundle at {bundle}")
    if any(path.is_symlink() for path in bundle.rglob("*")):
        raise ValueError(f"Refusing symlinks inside {bundle}")
    if signature:
        subprocess.run(["/usr/bin/codesign", "--verify", "--strict", "--all-architectures", str(bundle)], check=True)


def components_path(argument: Path) -> Path:
    path = argument.expanduser().absolute()
    reject_symlink_path(path)
    if path.name != "Components":
        raise ValueError("Destination must be a Components directory; target bundle name is fixed")
    return path


def install(source: Path, components: Path, dry_run: bool) -> None:
    source = source.expanduser().absolute()
    check_bundle(source, signature=True)
    target = components / BUNDLE_NAME
    if target.exists() or target.is_symlink():
        check_bundle(target)
    if source == target:
        raise ValueError("Source and destination are identical")
    if dry_run:
        print(f"DRY RUN: install {source} -> {target}")
        return
    components.mkdir(parents=True, exist_ok=True)
    reject_symlink_path(components)
    staging = Path(tempfile.mkdtemp(prefix=".LAB-RC-stage-", dir=components))
    backup = staging / "previous" / BUNDLE_NAME
    staged_bundle = staging / BUNDLE_NAME
    try:
        shutil.copytree(source, staged_bundle)
        check_bundle(staged_bundle, signature=True)
        if target.exists():
            backup.parent.mkdir()
            target.rename(backup)
        try:
            staged_bundle.rename(target)
        except OSError:
            if backup.exists():
                backup.rename(target)
            raise
    finally:
        # This directory was created by us in Components and has a unique name.
        shutil.rmtree(staging)
    print(f"Installed {target}")


def uninstall(components: Path, dry_run: bool) -> None:
    target = components / BUNDLE_NAME
    if not target.exists() and not target.is_symlink():
        print(f"Already absent: {target}")
        return
    check_bundle(target)
    if dry_run:
        print(f"DRY RUN: remove {target}")
        return
    shutil.rmtree(target)
    print(f"Removed {target}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="action", required=True)
    for action in ("install", "uninstall"):
        command = subparsers.add_parser(action)
        if action == "install":
            command.add_argument("source", type=Path)
        command.add_argument("--components-dir", type=Path,
                             default=Path.home() / "Library/Audio/Plug-Ins/Components")
        command.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    try:
        components = components_path(args.components_dir)
        if args.action == "install":
            install(args.source, components, args.dry_run)
        else:
            uninstall(components, args.dry_run)
    except (OSError, ValueError, plistlib.InvalidFileException, subprocess.CalledProcessError) as error:
        print(f"LAB-RC {args.action}: {error}", file=sys.stderr)
        sys.exit(1)
