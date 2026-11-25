#!/usr/bin/env python3
"""Merge the two actual local Release builds and make a verified macOS package."""
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile
import zipfile
from verify_macos import bundle_info, verify_bundle

ROOT = Path(__file__).resolve().parents[1]
PACKAGE_NAME = "LAB-RC-0.1.0-macOS-universal"
FORMATS = {"AU": "LAB-RC.component", "VST3": "LAB-RC.vst3", "Standalone": "LAB-RC.app"}


def au_validation_evidence(package):
    record = ROOT / "docs/reports/installed-au.json"
    if not record.is_file():
        return {"auval": "PENDING: no installed AU validation evidence"}
    evidence = json.loads(record.read_text())
    digest = hashlib.sha256(bundle_info(package / "AU/LAB-RC.component").read_bytes()).hexdigest()
    if digest != evidence.get("binary_sha256"):
        raise ValueError("Packaged AU binary differs from the installed/validated AU evidence")
    statuses = {}
    for architecture in ("arm64", "x86_64"):
        log = ROOT / "docs/reports" / ("auval-" + architecture + ".log")
        statuses[architecture] = "PASS" if log.is_file() and "AU VALIDATION SUCCEEDED." in log.read_text(errors="replace") else "PENDING"
    return {"auval": statuses, "au_binary_sha256": digest,
            "identical_to_installed_validated_au": True,
            "user_authorized_install": bool(evidence.get("user_authorized_install")),
            "logic_opened": bool(evidence.get("logic_opened"))}


def make_package(merge_only=False, au_only=False):
    subprocess.run([sys.executable, "-B", str(ROOT / "scripts/verify_assets.py")], cwd=ROOT, check=True)
    package_name = "LAB-RC-0.1.0-AU-universal" if au_only else PACKAGE_NAME
    destination = ROOT / "build/universal" if merge_only else ROOT / "dist" / package_name
    if destination.is_symlink() or (destination.exists() and not (destination / "build-verification.json").is_file()):
        raise ValueError(f"Refusing to replace an unrecognized package: {destination}")
    staging = Path(tempfile.mkdtemp(prefix="labrc-package-", dir=ROOT / "build"))
    package = staging / package_name
    package.mkdir()
    try:
        evidence = []
        for kind, name in ({"AU": FORMATS["AU"]} if au_only else FORMATS).items():
            arm = ROOT / "build/mac-arm64/LabRC_artefacts/Release" / kind / name
            intel = ROOT / "build/mac-x86_64/LabRC_artefacts/Release" / kind / name
            verify_bundle(arm, ["arm64"], signed=False)
            verify_bundle(intel, ["x86_64"], signed=False)
            merged = package / kind / name
            merged.parent.mkdir()
            shutil.copytree(arm, merged)
            subprocess.run(["/usr/bin/lipo", "-create", str(bundle_info(arm)), str(bundle_info(intel)),
                            "-output", str(bundle_info(merged))], check=True)
            info_file = merged / "Contents/Info.plist"
            info = plistlib.loads(info_file.read_bytes())
            info["LSMinimumSystemVersion"] = "10.15"
            info["LSMinimumSystemVersionByArchitecture"] = {"arm64": "11.0", "x86_64": "10.15"}
            info_file.write_bytes(plistlib.dumps(info))
            subprocess.run(["/usr/bin/codesign", "--force", "--sign", "-", "--timestamp=none", str(merged)], check=True)
            result = verify_bundle(merged, ["arm64", "x86_64"])
            result["bundle"] = str(Path(kind) / name)
            evidence.append(result)
        if merge_only:
            (package / "build-verification.json").write_text(json.dumps({
                "version": "0.1.0", "candidate": True, "bundles": evidence,
                "status": "Merged locally for validation; no distribution ZIP created",
            }, indent=2) + "\n")
            if destination.exists():
                shutil.rmtree(destination)
            package.rename(destination)
            print(f"Universal validation candidate: {destination}")
            print(json.dumps(evidence, indent=2))
            return
        scripts = package / "scripts"
        scripts.mkdir()
        for name in ("install_macos.sh", "uninstall_macos.sh", "bundle_tool.py"):
            shutil.copy2(ROOT / "scripts" / name, scripts / name)
        for name in ("README.md", "CHANGELOG.md", "LICENSE.md"):
            if (ROOT / name).is_file():
                shutil.copy2(ROOT / name, package / name)
        for pattern in ("docs/PROVENANCE.md", "docs/ACCEPTANCE.md", "docs/SMOKE_LOGIC.md",
                        "docs/TEST_REPORT.md", "docs/reports/*.md",
                        "docs/reports/*.log", "docs/reports/*.json", "docs/screenshots/*.png"):
            for source in ROOT.glob(pattern):
                target = package / source.relative_to(ROOT)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, target)
        shutil.copy2(ROOT / "Assets/ImplementationKit/manifest.json", package / "asset-manifest.json")
        juce_root = ROOT / "build/dependencies/juce"
        licenses = {
            "JUCE/LICENSE.md": "LICENSE.md",
            "AudioUnitSDK/LICENSE.txt": "modules/juce_audio_plugin_client/AU/AudioUnitSDK/LICENSE.txt",
            "VST3_SDK/LICENSE.txt": "modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt",
        }
        for relative, source in licenses.items():
            target = package / "ThirdPartyLicenses" / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(juce_root / source, target)
        (package / "ThirdPartyLicenses/ORIGIN.json").write_text(json.dumps({
            "JUCE": "8.0.17-security; already local source archive",
            "archive_sha256": "d37bab9f2330c50f42ba979575f6b55d5d3b23fa94221c2e57cac3abb69ac6d9",
            "source": "Already local juce-8.0.17-security.tar.gz with the verified SHA-256 above",
            "upstream_tag_status": "Not checked over the network; provenance is the pinned local archive",
            "license_state": "JUCE AGPLv3/commercial; commercial entitlement not established here",
            "bundled_sdks": {"AudioUnitSDK": "Apache-2.0", "VST3_SDK": "MIT"},
            "pluginval": "Validation tool only, not included or linked in this package",
        }, indent=2) + "\n")
        (package / "build-verification.json").write_text(json.dumps({
            "version": "0.1.0", "bundles": evidence,
            "format_scope": "AU universal macOS" if au_only else "AU, macOS VST3 and Standalone",
            "windows_vst3": "PENDING: no Windows build or smoke in this package",
            "logic_smoke": "PENDING: human host validation required",
            **au_validation_evidence(package),
            "ui_finish": "Not approved; current functional UI provided for physical host smoke",
            "signing": "Local ad-hoc only; no Developer ID signature or notarization",
        }, indent=2) + "\n")
        (package / "INSTALL.txt").write_text(
            "LAB-RC 0.1.0 / Manza's Rooms & Chambers\n"
            "AU universal arm64 macOS 11.0 / x86_64 macOS 10.15.\n"
            + ("This package contains the AU and its install/remove scripts.\n" if au_only
               else "VST3 macOS and Standalone are auxiliary QA products.\n") +
            "Local ad-hoc signature only; no notarization or Windows product.\n"
            "Install only when the owner has authorized it:\n"
            "  bash scripts/install_macos.sh AU/LAB-RC.component --dry-run\n"
            "  bash scripts/install_macos.sh AU/LAB-RC.component\n"
            "Remove only this AU:\n"
            "  bash scripts/uninstall_macos.sh\n"
            "No sudo, global Gatekeeper change, or automatic host launch.\n"
            "Current functional UI is provided for smoke; artistic finish is not approved.\n"
            "Human listening, host recall/automation and UI approval remain required.\n")
        checksums = []
        for path in sorted(p for p in package.rglob("*") if p.is_file()):
            checksums.append(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(package)}")
        (package / "SHA256SUMS.txt").write_text("\n".join(checksums) + "\n")
        destination.parent.mkdir(exist_ok=True)
        if destination.exists():
            shutil.rmtree(destination)
        package.rename(destination)
        archive = destination.parent / (destination.name + ".zip")
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as output:
            for path in sorted(destination.rglob("*")):
                output.write(path, Path(package_name) / path.relative_to(destination))
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        archive.with_suffix(".zip.sha256").write_text(f"{digest}  {archive.name}\n")
        print(f"Package: {archive}\nSHA-256: {digest}")
    finally:
        shutil.rmtree(staging)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--merge-only", action="store_true",
                        help="Only create a build/universal validation candidate; no dist ZIP")
    parser.add_argument("--au-only", action="store_true", help="Prioritize the AU universal and scripts ZIP")
    args = parser.parse_args()
    if args.merge_only and args.au_only:
        parser.error("Use --merge-only for the validation candidate or --au-only for the AU package")
    try:
        make_package(merge_only=args.merge_only, au_only=args.au_only)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"package_macos: {error}", file=sys.stderr)
        sys.exit(1)
