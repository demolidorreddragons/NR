#!/usr/bin/env python3
"""Verify the exact Neural Rendering model/source inputs before reverse engineering.

This intake check only authenticates file identity. It does not establish that
weights are semantically compatible with a graph or that a model can execute.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

EXPECTED = {
    "PS4-Neural-Engine-source-0.9.0.zip": {
        "sha256": "101ef376e4e3e3d948adf6153012bf6482f81586b57f7ebf7d2672237b2c1340",
    },
    "PS4-Neural-Engine-DLSSNR-model-310.8.3.zip": {
        "sha256": "36c005a3c51437eb55f09d201473371080b71d873567d865b18fe2a4dd92077a",
    },
    "FrameRateX - Reshade + DLSS 5 310.8.3 Files.zip": {
        "sha256": "b51278d5db51b0002f02c81c097da6921c27f99783be51cf23030a28851c449c",
    },
    "WEIGHTS_HT": {
        "sha256": "836f445d06ecd2e59bb9f17b84b91c143396fd76ccda1c9dc7fe81d5edd548f4",
        "size": 147695410,
    },
    "s00.bin": {
        "sha256": "37a293b5b781b9351a8b4b5fbeb2ff68fbd8a84891378950812ff3b99d161116",
    },
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify(directory: Path) -> int:
    missing = 0
    mismatched = 0
    for name, metadata in EXPECTED.items():
        path = directory / name
        if not path.is_file():
            print(f"MISSING  {name}")
            missing += 1
            continue
        size = path.stat().st_size
        expected_size = metadata.get("size")
        actual_hash = sha256_file(path)
        errors = []
        if expected_size is not None and size != expected_size:
            errors.append(f"size={size} expected={expected_size}")
        if actual_hash != metadata["sha256"]:
            errors.append(f"sha256={actual_hash} expected={metadata['sha256']}")
        if errors:
            print(f"INVALID  {name}: " + "; ".join(errors))
            mismatched += 1
        else:
            print(f"VERIFIED {name} ({size} bytes, sha256={actual_hash})")

    print(
        f"Summary: verified={len(EXPECTED) - missing - mismatched}, "
        f"missing={missing}, invalid={mismatched}"
    )
    if mismatched:
        return 1
    if missing:
        return 2
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "directory", nargs="?", type=Path, default=Path("."),
        help="directory containing the exact source archives/resources",
    )
    args = parser.parse_args()
    return verify(args.directory)


if __name__ == "__main__":
    raise SystemExit(main())
