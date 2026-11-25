#!/usr/bin/env python3
"""Run an already local pluginval against a built VST3 path, without installing it."""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pluginval", type=Path, required=True)
    parser.add_argument("--bundle", type=Path, required=True, help="An actual built macOS .vst3 bundle")
    parser.add_argument("--arch", choices=("arm64", "x86_64"), default="arm64")
    parser.add_argument("--strictness", type=int, choices=range(1, 11), default=5)
    args = parser.parse_args()
    validator = args.pluginval.expanduser().resolve(strict=True)
    bundle = args.bundle.expanduser().resolve(strict=True)
    if bundle.suffix != ".vst3" or not (bundle / "Contents/MacOS/LAB-RC").is_file():
        parser.error("Expected an actual LAB-RC macOS VST3 bundle; AU registration is separate")
    evidence = ROOT / "build/evidence" / ("pluginval-" + args.arch)
    evidence.mkdir(parents=True, exist_ok=True)
    stdout = evidence / "process.log"
    command = ["/usr/bin/arch", "-" + args.arch, str(validator),
               "--strictness-level", str(args.strictness), "--random-seed", "314159",
               "--sample-rates", "44100,48000,88200,96000,176400,192000",
               "--block-sizes", "1,32,64,127,128,256,512,1024,2048",
               "--timeout-ms", "120000", "--output-dir", str(evidence),
               "--output-filename", "LAB-RC", "--validate", str(bundle)]
    print("Validating", bundle, "using", args.arch, "strictness", args.strictness, flush=True)
    with stdout.open("w") as output:
        result = subprocess.run(command, cwd=ROOT, stdout=output, stderr=subprocess.STDOUT)
    text = stdout.read_text(errors="replace")
    print("\n".join(text.splitlines()[-35:]))
    print("pluginval process exit:", result.returncode, "log:", stdout)
    if result.returncode:
        return 1
    if "Num plugins found: 1" not in text:
        raise ValueError("pluginval returned success without evidence of one discovered plugin")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"validate_macos: {error}", file=sys.stderr)
        sys.exit(1)
