#!/usr/bin/env python3
"""Independent GFX8/NEO compute shader-container gate for opengnm-psbc output.

This checks the candidate compiler's own structural/CRC verifier and adds the
missing target-mode assertion. It is an offline artifact check, not a GPU test.
"""
from __future__ import annotations

import hashlib
import json
import pathlib
import sys

MAGIC = 0x72646853
GNM_HEADER_OFFSET = 0x24
GNM_SHADER_COMPUTE = 0x04
GNM_TARGET_NEO = 0x02


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: verify_gcn8_compute_sb.py <compiler-root> <shader.sb>", file=sys.stderr)
        return 2

    compiler_root = pathlib.Path(sys.argv[1]).resolve()
    shader_path = pathlib.Path(sys.argv[2]).resolve()
    sys.path.insert(0, str(compiler_root / "tests"))

    from verify_sb import verify_shader  # pinned upstream verifier

    data = shader_path.read_bytes()
    if len(data) < GNM_HEADER_OFFSET + 16:
        raise SystemExit(f"shader binary too short: {len(data)} bytes")

    magic = int.from_bytes(data[GNM_HEADER_OFFSET:GNM_HEADER_OFFSET + 4], "little")
    stage = data[GNM_HEADER_OFFSET + 8]
    target = data[GNM_HEADER_OFFSET + 11]

    if magic != MAGIC:
        raise SystemExit(f"GNM magic mismatch: 0x{magic:08x}")
    if stage != GNM_SHADER_COMPUTE:
        raise SystemExit(f"GNM stage mismatch: expected compute 0x04, got 0x{stage:02x}")
    if target != GNM_TARGET_NEO:
        raise SystemExit(
            f"GPU target mismatch: expected PS4 Pro NEO/GFX8 0x02, got 0x{target:02x}"
        )
    if not verify_shader(str(shader_path), GNM_SHADER_COMPUTE):
        raise SystemExit("upstream shader structure/CRC verification failed")

    report = {
        "status": "PASS_ARTIFACT_ONLY_NOT_HARDWARE",
        "compiler": "PS4-OpenGNM/opengnm-psbc",
        "shader_stage": "compute",
        "gpu_target": "PS4 Pro NEO/GFX8",
        "gnm_magic": f"0x{magic:08x}",
        "gnm_stage_byte": stage,
        "gnm_target_byte": target,
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "hardware_dispatch_tested": False,
        "firmware_14_02_tested": False,
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
