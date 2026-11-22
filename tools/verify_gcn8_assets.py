#!/usr/bin/env python3
"""Independent verifier for the six GFX8 binaries extracted from a PKG."""
import hashlib
from pathlib import Path
import sys

EXPECTED = {
    "compute_smoke_add.gcn.bin": "bd6deda8f6fc7cc04f78b441864a0b57d63f259f8d048d6ac09747f0cc6988e5",
    "e4m3_decode.gcn.bin": "08ed8e40dbab45038312f3d4db49a2dc49f209e7b393bc2c497a4b42ccb70c60",
    "e4m3_encode.gcn.bin": "86b8c0a49fc46d513a27f344b1515cc7ab38353878215b29238d27d74e4d0142",
    "gemm_fp16_accum.gcn.bin": "5bf7129019cab1e40a67b572dba41167c3e5346bfe3360ded939f84894008048",
    "gemm_fp32.gcn.bin": "ca0325130b9b77294a3fd7c32957d076f90741584901570a2c1be5521a099412",
    "gemm_fp32_wave64_tiled.gcn.bin": "5dd8cba2710502d5c1f930e4e0ddbe779a56b168900f1040c0e95b28c0f3f8bc",
}

def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify_gcn8_assets.py EXTRACTED_GCN8_DIR")
    root = Path(sys.argv[1])
    manifest = (root / "SHA256SUMS").read_text(encoding="ascii").splitlines()
    declared = {}
    for line in manifest:
        digest, name = line.split("  ", 1)
        if name in declared:
            raise SystemExit(f"duplicate GCN8 manifest entry: {name}")
        declared[name] = digest
    if set(declared) != set(EXPECTED):
        raise SystemExit("GCN8 manifest entry set mismatch")
    for name, expected in EXPECTED.items():
        path = root / name
        data = path.read_bytes()
        actual = hashlib.sha256(data).hexdigest()
        if actual != expected or declared[name] != expected:
            raise SystemExit(f"GCN8 asset verification failed: {name}")
        print(f"GCN8_ASSET_PASS name={name} bytes={len(data)} sha256={actual}")
    print("GCN8_ASSETS=6_OF_6_PASS")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
