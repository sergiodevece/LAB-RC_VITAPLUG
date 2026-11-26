#!/usr/bin/env python3
"""Inspect LAB-RC Mach-O slices, deployment commands and local signatures."""
import argparse
from pathlib import Path
import plistlib
import re
import subprocess
import sys

EXPECTED_ID = "com.vitaplug.labrc"
EXPECTED_VERSION = "0.2.0"
MINIMUMS = {"arm64": "11.0", "x86_64": "10.15"}


def command(*args):
    return subprocess.check_output(list(map(str, args)), text=True, stderr=subprocess.STDOUT).strip()


def bundle_info(bundle: Path):
    with (bundle / "Contents" / "Info.plist").open("rb") as source:
        info = plistlib.load(source)
    if info.get("CFBundleIdentifier") != EXPECTED_ID or info.get("CFBundleShortVersionString") != EXPECTED_VERSION:
        raise ValueError(f"Unexpected bundle identity/version: {bundle}")
    executable = info.get("CFBundleExecutable")
    if executable != "LAB-RC":
        raise ValueError(f"Unexpected executable name: {executable}")
    if bundle.suffix == ".component":
        components = info.get("AudioComponents", [])
        if len(components) != 1 or any(components[0].get(k) != v for k, v in
                {"type": "aufx", "subtype": "LbRc", "manufacturer": "Vtpl"}.items()):
            raise ValueError("Unexpected AU component codes")
    return bundle / "Contents" / "MacOS" / executable


def deployment(binary: Path, architecture: str):
    commands = command("/usr/bin/otool", "-arch", architecture, "-l", binary)
    values = []
    for block in re.split(r"Load command \d+", commands):
        if "cmd LC_BUILD_VERSION" in block:
            match = re.search(r"\bminos\s+(\d+(?:\.\d+)+)", block)
        elif "cmd LC_VERSION_MIN_MACOSX" in block:
            match = re.search(r"\bversion\s+(\d+(?:\.\d+)+)", block)
        else:
            continue
        if match:
            values.append(match.group(1))
    if len(values) != 1:
        raise ValueError(f"Expected one macOS deployment command for {architecture}: {values}")
    return values[0]


def verify_bundle(bundle: Path, architectures, signed=True):
    binary = bundle_info(bundle)
    actual_architectures = set(command("/usr/bin/lipo", "-archs", binary).split())
    if actual_architectures != set(architectures):
        raise ValueError(f"Wrong architectures {actual_architectures}; expected {set(architectures)}")
    if len(actual_architectures) == 2:
        info = plistlib.loads((bundle / "Contents/Info.plist").read_bytes())
        if info.get("LSMinimumSystemVersionByArchitecture") != MINIMUMS:
            raise ValueError("Universal bundle lacks the matching per-architecture minimum macOS versions")
    details = {"bundle": str(bundle), "architectures": {}}
    for architecture in sorted(actual_architectures):
        minimum = deployment(binary, architecture)
        if tuple(map(int, minimum.split("."))) != tuple(map(int, MINIMUMS[architecture].split("."))):
            raise ValueError(f"Wrong {architecture} deployment target {minimum}; expected {MINIMUMS[architecture]}")
        details["architectures"][architecture] = {"minimum_macos": minimum}
    if signed:
        command("/usr/bin/codesign", "--verify", "--strict", "--all-architectures", bundle)
        signature = command("/usr/bin/codesign", "-dv", "--verbose=2", bundle)
        if "Signature=adhoc" not in signature:
            raise ValueError("Expected an explicitly local ad-hoc signature")
        details["signature"] = "ad-hoc; not notarized"
    return details


if __name__ == "__main__":
    import json
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--architectures", nargs="+", choices=tuple(MINIMUMS), default=list(MINIMUMS))
    args = parser.parse_args()
    try:
        print(json.dumps(verify_bundle(args.bundle, args.architectures), indent=2))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"verify_macos: {error}", file=sys.stderr)
        sys.exit(1)
