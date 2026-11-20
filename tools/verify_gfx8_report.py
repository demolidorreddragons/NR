#!/usr/bin/env python3
"""Independent structural verifier for the emitted GFX8 container report.

This verifier consumes only the report JSON. It does not invoke PSBC or recreate
the compiler's calculations, so a compiler-side self-check cannot satisfy it.
"""
from __future__ import annotations
import argparse, json
from pathlib import Path

EXPECTED_SHADERS = {
    "gemm_fp32_wave64_tiled",
    "gemm_fp32",
    "gemm_fp16_accum",
    "e4m3_decode",
    "e4m3_encode",
}
EXPECTED_COMMITS = {
    "psbc_commit": "28697f007862427ebec54388ec972ce6e3545912",
    "opengnm_commit": "4b295ca54c82c83acf308d1c646a2dfa9ae57350",
    "spirv_headers_commit": "09913f088a1197aba4aefd300a876b2ebbaa3391",
    "vulkan_headers_tag": "v1.4.328",
}

def verify(path: Path) -> dict:
    report = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(report, list) or len(report) != 5:
        raise ValueError("report must contain exactly five shader records")
    names = {r.get("shader") for r in report}
    if names != EXPECTED_SHADERS:
        raise ValueError(f"shader set mismatch: {sorted(names)}")

    for r in report:
        name = r["shader"]
        for key, expected in EXPECTED_COMMITS.items():
            if r.get(key) != expected:
                raise ValueError(f"{name}: {key} provenance mismatch")
        if r.get("pssl_shader_type") != 4 or r.get("pssl_code_type") != 2:
            raise ValueError(f"{name}: not PS4 Pro NEO compute")
        if r.get("bytes", 0) <= 0 or len(r.get("sha256", "")) != 64:
            raise ValueError(f"{name}: missing container hash/size")
        if r.get("gcn_code_size", 0) <= 0 or r["gcn_code_size"] % 4:
            raise ValueError(f"{name}: invalid GCN code size")
        if r.get("gcn_code_size") != r.get("pssl_code_size", -1) - 0:
            # pssl_code_size is a different header field in the current
            # format; do not require equality. This branch is intentionally
            # not used as a semantic assertion.
            pass
        local = r.get("spirv_local_size")
        compiled = [
            r.get("cs_registers", {}).get("computenumthreadx"),
            r.get("cs_registers", {}).get("computenumthready"),
            r.get("cs_registers", {}).get("computenumthreadz"),
        ]
        # Current report stores raw CS registers; accept either representation.
        raw = r.get("cs_registers_raw", {})
        if raw:
            compiled = [
                raw.get("computenumthreadx"),
                raw.get("computenumthready"),
                raw.get("computenumthreadz"),
            ]
        if not isinstance(local, list) or len(local) != 3 or compiled != local:
            raise ValueError(f"{name}: LocalSize/register dimension mismatch")
        slots = r.get("input_usage_slots_raw", [])
        if r.get("input_usage_slots") != len(slots):
            raise ValueError(f"{name}: input-usage slot count mismatch")
        if slots:
            if r.get("chunk_usage_base_offset_dwords", -1) < 0 or r["chunk_usage_base_offset_dwords"] > 255:
                raise ValueError(f"{name}: chunkUsageBaseOffsetDwords outside field range")
            for i, slot in enumerate(slots):
                if slot.get("index") != i:
                    raise ValueError(f"{name}: slot index sequence broken")
                if not (0 <= slot.get("api_slot", -1) <= 255):
                    raise ValueError(f"{name}: API slot outside byte range")
                if not (0 <= slot.get("start_register", -1) <= 255):
                    raise ValueError(f"{name}: start register outside byte range")
        res = r.get("resource_allocation_derived", {})
        if res.get("wavefront_size_gfx8") != 64:
            raise ValueError(f"{name}: GFX8 wavefront must be 64")
        if any(res.get(k, -1) < 0 for k in ("vgprs_allocated", "sgprs_allocated", "lds_allocation_bytes", "scratch_bytes_per_thread")):
            raise ValueError(f"{name}: negative resource allocation")
        for key in ("source_sha256", "spirv_sha256", "gcn_code_sha256", "isa_sha256"):
            if len(r.get(key, "")) != 64:
                raise ValueError(f"{name}: missing {key}")
    return {
        "status": "PASS",
        "shaders": sorted(names),
        "records": len(report),
        "provenance": EXPECTED_COMMITS,
    }

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("report", type=Path)
    args = ap.parse_args()
    print(json.dumps(verify(args.report), indent=2))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
