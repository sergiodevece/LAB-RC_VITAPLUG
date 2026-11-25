#!/usr/bin/env python3
"""Gate 0 independiente, sin dependencias; no escribe en el kit."""
from argparse import ArgumentParser
from array import array
import hashlib
import json
import math
from pathlib import Path
import struct
import sys


def read_float_wav(path):
    raw = path.read_bytes()
    if raw[:4] != b"RIFF" or raw[8:12] != b"WAVE":
        raise ValueError(f"RIFF/WAVE inválido: {path}")
    offset, fmt, data = 12, None, None
    while offset + 8 <= len(raw):
        tag, length = struct.unpack_from("<4sI", raw, offset)
        payload = raw[offset + 8:offset + 8 + length]
        if len(payload) != length:
            raise ValueError(f"Chunk truncado: {path}")
        if tag == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", payload)
        elif tag == b"data":
            data = payload
        offset += 8 + length + (length & 1)
    if fmt is None or data is None:
        raise ValueError(f"WAV sin fmt/data: {path}")
    encoding, channels, rate, byte_rate, align, bits = fmt
    if encoding != 3 or bits != 32 or align != channels * 4 or byte_rate != rate * align:
        raise ValueError(f"No es IEEE float32 canónico: {path}")
    if len(data) % align:
        raise ValueError(f"Frame incompleto: {path}")
    samples = array("f")
    samples.frombytes(data)
    if sys.byteorder != "little":
        samples.byteswap()
    if not all(map(math.isfinite, samples)):
        raise ValueError(f"NaN/Inf: {path}")
    return rate, channels, len(data) // align, samples


def verify(root):
    manifest = json.loads((root / "manifest.json").read_text())
    pngs, wavs = 0, 0
    for entry in manifest["files"]:
        path = root / entry["path"]
        raw = path.read_bytes()
        if len(raw) != entry["bytes"] or hashlib.sha256(raw).hexdigest() != entry["sha256"]:
            raise ValueError(f"SHA256/tamaño no coincide: {entry['path']}")
        if path.suffix == ".png":
            if raw[:8] != b"\x89PNG\r\n\x1a\n" or raw[12:16] != b"IHDR":
                raise ValueError(f"PNG inválido: {path}")
            if list(struct.unpack_from(">II", raw, 16)) != entry["dimensions"]:
                raise ValueError(f"Dimensiones PNG: {path}")
            pngs += 1
        elif path.suffix == ".wav":
            rate, channels, frames, _ = read_float_wav(path)
            order = ["LL", "LR", "RL", "RR"] if channels == 4 else ["mic_L", "mic_R"]
            if not (rate == entry["sample_rate"] == 96000 and channels == entry["channels"]
                    and frames == entry["frames"] and frames / rate == entry["duration_s"]
                    and entry["dtype"] == "float32" and entry["channel_order"] == order):
                raise ValueError(f"Metadatos WAV: {path}")
            wavs += 1
        print(f"PASS {entry['path']}")
    errors = {}
    for path in sorted((root / "assets/ir/v05").glob("*.wav")):
        _, _, frames, reference = read_float_wav(path)
        _, _, matrix_frames, matrix = read_float_wav(root / "assets/ir/v06" / (path.stem + "_matrix.wav"))
        if frames != matrix_frames:
            raise ValueError(f"Frames v05/v06: {path}")
        error = max(abs(float(matrix[4 * i + c]) + float(matrix[4 * i + c + 2])
                        - float(reference[2 * i + c])) for i in range(frames) for c in range(2))
        if error >= 4e-9:
            raise ValueError(f"Regresión mono {path.stem}: {error}")
        errors[path.stem] = error
        print(f"PASS mono {path.stem}: {error:.12g}")
    if (len(manifest["files"]), pngs, wavs, len(errors)) != (32, 4, 18, 9):
        raise ValueError("Inventario incompleto")
    summary = {"status": "PASS", "hashes": 32, "pngs": pngs, "wavs": wavs,
               "mono_regressions": errors, "max_mono_error": max(errors.values()),
               "method": "independent Python stdlib; original numpy/scipy verifier not executed successfully"}
    print(json.dumps(summary, indent=2))
    return summary


if __name__ == "__main__":
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1] / "Assets/ImplementationKit")
    args = parser.parse_args()
    try:
        verify(args.root)
    except (OSError, ValueError, KeyError, struct.error) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
