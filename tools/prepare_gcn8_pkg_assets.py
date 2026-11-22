#!/usr/bin/env python3
"""Decode and verify pinned GFX8 binaries from the successful shader build artifact."""
from __future__ import annotations
import base64
import hashlib
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "gcn8"
DEST = Path(sys.argv[1]) if len(sys.argv) == 2 else None
ASSETS = {
    "compute_smoke_add.gcn.bin": (88, "bd6deda8f6fc7cc04f78b441864a0b57d63f259f8d048d6ac09747f0cc6988e5"),
    "e4m3_decode.gcn.bin": (856, "08ed8e40dbab45038312f3d4db49a2dc49f209e7b393bc2c497a4b42ccb70c60"),
    "e4m3_encode.gcn.bin": (540, "86b8c0a49fc46d513a27f344b1515cc7ab38353878215b29238d27d74e4d0142"),
    "gemm_fp16_accum.gcn.bin": (364, "5bf7129019cab1e40a67b572dba41167c3e5346bfe3360ded939f84894008048"),
    "gemm_fp32.gcn.bin": (292, "ca0325130b9b77294a3fd7c32957d076f90741584901570a2c1be5521a099412"),
    "gemm_fp32_wave64_tiled.gcn.bin": (948, "5dd8cba2710502d5c1f930e4e0ddbe779a56b168900f1040c0e95b28c0f3f8bc"),
}

def main() -> int:
    if DEST is None:
        raise SystemExit("usage: prepare_gcn8_pkg_assets.py DEST_DIR")
    DEST.mkdir(parents=True, exist_ok=False)
    manifest = []
    for name, (expected_size, expected_hash) in ASSETS.items():
        encoded = (SOURCE / (name + ".b64")).read_text(encoding="ascii").strip()
        data = base64.b64decode(encoded, validate=True)
        digest = hashlib.sha256(data).hexdigest()
        if len(data) != expected_size or digest != expected_hash:
            raise SystemExit(f"GCN8 asset mismatch: {name}: bytes={len(data)} sha256={digest}")
        (DEST / name).write_bytes(data)
        manifest.append(f"{digest}  {name}")
        print(f"GCN8_ASSET_VERIFIED name={name} bytes={len(data)} sha256={digest}")
    (DEST / "SHA256SUMS").write_text("\n".join(manifest) + "\n", encoding="ascii")
    print(f"GCN8_ASSET_BUNDLE=PASS count={len(ASSETS)}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
