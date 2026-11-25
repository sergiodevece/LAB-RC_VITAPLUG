#!/usr/bin/env python3
"""Local, offline CMake build/test driver. Never installs plugins or opens a host."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    print("+", " ".join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", choices=("arm64", "x86_64", "universal", "dsp", "sanitize"), default="arm64")
    parser.add_argument("--juce-archive", type=Path, help="Existing local pinned JUCE archive; no downloads")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--screenshots", action="store_true")
    parser.add_argument("--package", action="store_true", help="Requires --arch universal")
    args = parser.parse_args()
    if args.jobs < 1 or (args.package and args.arch != "universal"):
        parser.error("--jobs must be positive; --package requires --arch universal")
    if args.juce_archive:
        run(sys.executable, "-B", ROOT / "scripts/prepare_juce.py", "--archive", args.juce_archive)
    if not (ROOT / "build/dependencies/juce/.labrc-archive-sha256").is_file():
        parser.error("JUCE missing: provide --juce-archive pointing to the already local pinned archive")
    run(sys.executable, "-B", ROOT / "scripts/verify_assets.py")
    run("cmake", "--version")
    architectures = ("arm64", "x86_64") if args.arch == "universal" else (args.arch,)
    for architecture in architectures:
        preset = "mac-" + architecture
        run("cmake", "--preset", preset)
        run("cmake", "--build", "--preset", preset, "--parallel", args.jobs)
        run("ctest", "--preset", preset)
    if args.screenshots:
        if args.arch not in ("arm64", "universal"):
            parser.error("--screenshots is supported after the arm64 editor build")
        run(ROOT / "build/mac-arm64/tools/labrc_render_ui", "--output", ROOT / "build/evidence/screenshots")
        run(sys.executable, "-B", ROOT / "scripts/check_screenshots.py", ROOT / "build/evidence/screenshots")
    if args.package:
        run(sys.executable, "-B", ROOT / "scripts/package_macos.py")


if __name__ == "__main__":
    try:
        main()
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"build_macos: {error}", file=sys.stderr)
        sys.exit(1)
