#!/usr/bin/env python3
"""Build and package LAB-RC with the native Windows x64 MSVC toolchain."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = re.search(r"^project\(LabRC VERSION ([\d.]+) ", (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"),
                    re.MULTILINE).group(1)
BUNDLE_NAME = "LAB-RC.vst3"
PACKAGE_NAME = "LAB-RC-Windows-x64-VST3"


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def inspect_exports_and_imports(dumpbin, binary):
    output = subprocess.check_output([str(dumpbin), "/exports", "/dependents", str(binary)], text=True,
                                     errors="replace", stderr=subprocess.STDOUT)
    if not re.search(r"\bGetPluginFactory\b", output):
        raise ValueError("El binario no exporta GetPluginFactory")
    if re.search(r"\b(?:VCRUNTIME|MSVCP|CONCRT)\d+\S*\.dll\b", output, re.IGNORECASE):
        raise ValueError("El binario depende del runtime de Visual C++ dinámico")
    return output


def verify_bundle(bundle, dumpbin):
    binary = bundle / "Contents/x86_64-win" / BUNDLE_NAME
    with binary.open("rb") as stream:
        dos = stream.read(64)
        if len(dos) != 64 or dos[:2] != b"MZ":
            raise ValueError("El binario no es un PE32+ DLL AMD64")
        stream.seek(struct.unpack_from("<I", dos, 0x3C)[0])
        pe = stream.read(26)
    if (len(pe) != 26 or pe[:4] != b"PE\x00\x00" or struct.unpack_from("<H", pe, 4)[0] != 0x8664
            or not struct.unpack_from("<H", pe, 22)[0] & 0x2000 or pe[24:] != b"\x0b\x02"):
        raise ValueError("El binario no es un PE32+ DLL AMD64")
    manifest = bundle / "Contents/Resources/moduleinfo.json"
    raw = manifest.read_text(encoding="utf-8-sig")
    # JUCE writes JSON5 trailing commas in the generated manifest.
    metadata = json.loads(re.sub(r'("(?:\\.|[^"\\])*")|,(?=\s*[}\]])',
                                 lambda match: match.group(1) or "", raw))
    if (metadata.get("Name"), metadata.get("Version"), metadata.get("Factory Info", {}).get("Vendor")) != (
            "LAB-RC", VERSION, "VITAPLUG"):
        raise ValueError("Identidad o versión incorrecta en moduleinfo.json")
    inspect_exports_and_imports(dumpbin, binary)
    return binary


def find_dumpbin(build):
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    match = re.search(r"^CMAKE_LINKER:FILEPATH=(.*)$", cache, re.MULTILINE)
    if match:
        sibling = Path(match.group(1)).with_name("dumpbin.exe")
        if sibling.is_file():
            return sibling
    installed = shutil.which("dumpbin.exe")
    if installed:
        return Path(installed)
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)"))
    vswhere /= "Microsoft Visual Studio/Installer/vswhere.exe"
    if vswhere.is_file():
        found = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires",
                                         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-find",
                                         r"VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe"], text=True).splitlines()
        if found and Path(found[0]).is_file():
            return Path(found[0])
    raise ValueError("No se encontró dumpbin.exe. Instala las herramientas C++ x64 de Visual Studio 2022.")


def run_logged(command, log):
    with log.open("w", encoding="utf-8") as record:
        with subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, errors="replace") as process:
            for line in process.stdout:
                record.write(line)
                print(line, end="", flush=True)
            if process.wait():
                raise subprocess.CalledProcessError(process.returncode, command)


def verify_pluginval_log(log):
    lines = {line.strip() for line in log.read_text(encoding="utf-8").splitlines()}
    if not {"Num plugins found: 1", f"VITAPLUG: LAB-RC v{VERSION}", "SUCCESS"}.issubset(lines):
        raise ValueError(f"pluginval no confirmó el éxito para LAB-RC {VERSION}: {log}")


def verify_steinberg_log(log):
    results = re.findall(r"^Result:\s+(\d+) tests passed,\s+(\d+) tests failed\s*$",
                         log.read_text(encoding="utf-8"), re.MULTILINE)
    if len(results) != 1 or int(results[0][0]) == 0 or int(results[0][1]) != 0:
        raise ValueError(f"VST3 Validator no confirmó pruebas satisfactorias: {log}")
    return int(results[0][0])


def package(bundle, binary, evidence, pluginval_status, steinberg_status):
    destination = ROOT / "dist"
    destination.mkdir(exist_ok=True)
    output = destination / f"{PACKAGE_NAME}.zip"
    with tempfile.TemporaryDirectory(prefix="labrc-windows-", dir=destination) as temporary:
        stage = Path(temporary)
        shutil.copytree(bundle, stage / BUNDLE_NAME)
        shutil.copytree(evidence, stage / "evidence")
        shutil.copy2(ROOT / "scripts/windows/install.ps1", stage / "install.ps1")
        shutil.copy2(ROOT / "docs/WINDOWS_SMOKE.md", stage / "INSTALACION.md")
        shutil.copy2(ROOT / "README.md", stage)
        shutil.copy2(ROOT / "CHANGELOG.md", stage)
        shutil.copy2(ROOT / "LICENSE.md", stage)
        juce = ROOT / "build/dependencies/juce"
        shutil.copy2(juce / "LICENSE.md", stage / "JUCE-LICENSE.md")
        sdk = juce / "modules/juce_audio_processors_headless/format_types/VST3_SDK"
        shutil.copy2(sdk / "LICENSE.txt", stage / "VST3-SDK-LICENSE.txt")
        binary_hash = sha256(binary)
        (stage / "build-info.json").write_text(json.dumps({
            "version": VERSION, "architecture": "Windows x64", "configuration": "Release",
            "host_os": platform.platform(), "host_architecture": platform.machine(),
            "toolchain": "Visual Studio 2022 / MSVC", "binary_sha256": binary_hash,
            "ctest": "PASS", "pluginval": pluginval_status, "steinberg_validator": steinberg_status,
            "daw_smoke": "PENDING",
        }, indent=2) + "\n", encoding="utf-8")
        (stage / "INFORME.md").write_text(
            f"# LAB-RC {VERSION} · Windows x64 Release\n\n"
            f"- Entorno de compilación: {platform.platform()} ({platform.machine()}).\n"
            "- MSVC de Visual Studio 2022. Versión del compilador en el registro de configuración.\n"
            "- Integridad de IR y gráficos, CTest Release y formato PE32+ AMD64: PASS.\n"
            f"- pluginval: {pluginval_status}.\n"
            f"- Steinberg VST3 Validator: {steinberg_status}.\n"
            "- CPU y memoria: consultar evidence/benchmark.log con las mediciones del runner.\n"
            "- Comparación entre plataformas: el informe independiente acompaña la entrega si se ejecuta.\n"
            "- Smoke en un DAW Windows 10/11 y comprobación visual HiDPI: pendientes.\n"
            f"- SHA-256 del binario: {binary_hash}.\n", encoding="utf-8")
        hashes = "".join(f"{sha256(path)}  {path.relative_to(stage).as_posix()}\n"
                         for path in sorted(stage.rglob("*")) if path.is_file())
        (stage / "SHA256SUMS.txt").write_text(hashes, encoding="utf-8")
        staged_zip = stage / output.name
        with zipfile.ZipFile(staged_zip, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(stage.rglob("*")):
                if path.is_file() and path != staged_zip:
                    archive.write(path, Path(PACKAGE_NAME) / path.relative_to(stage))
        with zipfile.ZipFile(staged_zip) as archive:
            if failed := archive.testzip():
                raise ValueError(f"Error de CRC en {failed}")
        staged_zip.replace(output)
    output.with_suffix(".zip.sha256").write_text(f"{sha256(output)}  {output.name}\n", encoding="ascii")
    return output


def build_windows(juce_archive, pluginval, steinberg_validator):
    for tool in ("cmake", "ctest"):
        if not shutil.which(tool):
            raise ValueError(f"No se encontró {tool} en PATH")
    if pluginval is not None:
        pluginval = pluginval.resolve(strict=True)
    if steinberg_validator is not None:
        steinberg_validator = steinberg_validator.resolve(strict=True)
    juce_archive = juce_archive.resolve(strict=True)
    build = ROOT / "build/windows-x64"
    build.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix="validation-", dir=build))
    print(f"Registros de compilación y validación: {evidence}", flush=True)
    run_logged([sys.executable, "-B", "scripts/prepare_juce.py", "--archive", str(juce_archive)],
               evidence / "juce.log")
    run_logged([sys.executable, "-B", "scripts/verify_assets.py", "--final-touch", "Assets/FinalTouch"],
               evidence / "integrity.log")
    run_logged(["cmake", "-S", ".", "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64",
                "-DBUILD_TESTING=ON", "-DLABRC_BUILD_PLUGIN=ON"], evidence / "configure.log")
    run_logged(["cmake", "--build", str(build), "--config", "Release", "--target", "LabRC_VST3",
                "labrc_tests", "labrc_final_touch_regression", "--parallel", "4"], evidence / "build.log")
    run_logged(["ctest", "--test-dir", str(build), "-C", "Release", "--output-on-failure",
                "--no-tests=error"], evidence / "ctest.log")
    run_logged([str(build / "tests/Release/labrc_tests.exe"), "--benchmark"],
               evidence / "benchmark.log")
    bundle = build / "LabRC_artefacts/Release/VST3" / BUNDLE_NAME
    dumpbin = find_dumpbin(build)
    binary = verify_bundle(bundle, dumpbin)
    run_logged([str(dumpbin), "/headers", "/exports", "/dependents", str(binary)], evidence / "pe.log")
    pluginval_status = "PENDIENTE (pluginval.exe no disponible)"
    if pluginval is not None:
        log = evidence / "pluginval.log"
        run_logged([str(pluginval), "--strictness-level", "5", "--random-seed", "314159",
                    "--sample-rates", "44100,48000,88200,96000,176400,192000",
                    "--block-sizes", "1,32,64,127,128,256,512,1024,2048",
                    "--timeout-ms", "120000", "--validate", str(bundle)], log)
        verify_pluginval_log(log)
        pluginval_status = "PASS (nivel 5)"
    steinberg_status = "PENDIENTE (validator.exe no disponible)"
    if steinberg_validator is not None:
        log = evidence / "steinberg-validator.log"
        run_logged([str(steinberg_validator), str(bundle)], log)
        steinberg_status = f"PASS ({verify_steinberg_log(log)} pruebas)"
    release = package(bundle, binary, evidence, pluginval_status, steinberg_status)
    print(f"VST3 compilado y empaquetado: {release}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--juce-archive", type=Path, required=True, help="Archivo JUCE local fijado por SHA-256")
    parser.add_argument("--pluginval", type=Path, help="pluginval.exe local, opcional")
    parser.add_argument("--steinberg-validator", type=Path, help="VST3 Validator x64 de Steinberg, opcional")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("La compilación VST3 Windows x64 requiere un entorno Windows con MSVC x64.")
    try:
        build_windows(args.juce_archive, args.pluginval, args.steinberg_validator)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"LAB-RC Windows: {error}\n")


if __name__ == "__main__":
    main()
