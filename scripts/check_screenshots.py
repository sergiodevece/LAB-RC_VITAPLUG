#!/usr/bin/env python3
"""Verify every Final Touch renderer PNG and exclude stale appended images."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib


def check_png(path: Path, expected_size):
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"Invalid PNG signature: {path}")
    position = 8
    size = None
    while position + 12 <= len(data):
        length, = struct.unpack_from(">I", data, position)
        chunk_end = position + 12 + length
        if chunk_end > len(data):
            raise ValueError(f"Truncated PNG chunk: {path}")
        kind = data[position + 4:position + 8]
        payload = data[position + 8:position + 8 + length]
        crc, = struct.unpack_from(">I", data, position + 8 + length)
        if zlib.crc32(kind + payload) & 0xffffffff != crc:
            raise ValueError(f"Invalid PNG CRC: {path}")
        if kind == b"IHDR":
            size = struct.unpack_from(">II", payload)
        position = chunk_end
        if kind == b"IEND":
            if position != len(data):
                raise ValueError(f"PNG has {len(data) - position} trailing bytes, possibly an old appended snapshot: {path}")
            if expected_size is not None and size != expected_size:
                raise ValueError(f"PNG dimensions {size}; expected {expected_size}: {path}")
            if size is None or min(size) < 600:
                raise ValueError(f"PNG lacks a full-size snapshot: {path}")
            return {"file": path.name, "width": size[0], "height": size[1],
                    "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    raise ValueError(f"PNG has no complete IEND chunk: {path}")


def check_directory(folder: Path):
    result = []
    for room in ("kentucky", "center-field", "live-room"):
        for suffix, size in (("min", (1024, 684)), ("default", (1280, 854)),
                             ("max", (1586, 1058)), ("default@2x", (2560, 1708))):
            result.append(check_png(folder / f"{room}-{suffix}.png", size))
    result.append(check_png(folder / "controls-detail.png", None))
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps({"status": "PASS", "screenshots": check_directory(args.directory)}, indent=2))
    except (OSError, ValueError, struct.error) as error:
        print(f"check_screenshots: {error}", file=sys.stderr)
        sys.exit(1)
