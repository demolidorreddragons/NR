#!/usr/bin/env python3
"""Validate the 310.8.x packed model manifest against OpenDLSS-NR's graph layouts."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

RESOURCE_SHA256 = "836f445d06ecd2e59bb9f17b84b91c143396fd76ccda1c9dc7fe81d5edd548f4"
STAGE_SHA256 = "37a293b5b781b9351a8b4b5fbeb2ff68fbd8a84891378950812ff3b99d161116"
MANIFEST_SHA256 = "1610780bbc4d40b4f98f25e146063dd2f3ef05d0e6e60af548ae4d4566b43d02"
RESOURCE_BYTES = 147_695_410
STAGE_BYTES = 147_683_778


def align_up(v: int, a: int) -> int:
    return (v + a - 1) // a * a


def fused_end(c: int) -> int:
    if c not in (32, 64, 128, 256):
        raise ValueError(f"unsupported fused width {c}")
    heads = c // 32
    expert = c >= 64
    experts = c // 32 if expert else 0
    expand = experts * c * 128 if expert else c * 128
    ffn_weights = expand + experts * 128 * 32 + experts * 32 * c if expert else expand + 128 * c
    ffn_cos_skip = ffn_weights + 16
    qkv = ffn_cos_skip + c * 2 + 16
    relative = qkv + c * c * 3
    scale = relative + heads * 8192
    projection = scale + align_up(heads * 4, 16)
    attn_cos_skip = projection + c * c
    return attn_cos_skip + c * 2


def upsample_length(input_c: int, c: int) -> int:
    if input_c != c * 2:
        raise ValueError("upsample layout requires 2x input channels")
    heads = c // 32
    expert = c >= 64
    experts = c // 32 if expert else 0
    narrow_padding = 16 if c == 32 else 0
    expand = experts * c * 128 if expert else c * 128
    ffn_weights = expand + experts * 128 * 32 + experts * 32 * c if expert else expand + 128 * c
    upsample_weight = ffn_weights
    ffn_cos_skip = upsample_weight + input_c * c + narrow_padding
    transition_scale = ffn_cos_skip + c * 2 + narrow_padding
    qkv = transition_scale + c * 2
    relative = qkv + c * c * 3
    scale = relative + heads * 8192
    projection = scale + align_up(heads * 4, 16)
    attn_cos_skip = projection + c * c
    return attn_cos_skip + c * 2 + 16


def expected_lengths() -> dict[str, int]:
    expected: dict[str, int] = {}

    def add(block: int, layer: int, size: int, parameter: str = "layer") -> None:
        name = f"block{block}.layer{layer}.{parameter}"
        if name in expected:
            raise AssertionError(f"duplicate expected tensor {name}")
        expected[name] = size

    add(0, 0, 21696)
    for block in range(1, 4):
        add(block, 0, fused_end(32) + 16)
    add(4, 0, fused_end(32) + 32 * 64 + 16)
    for block in range(5, 8):
        add(block, 0, fused_end(64) + 16)
    add(8, 0, fused_end(64) + 64 * 128)
    for block in range(9, 14):
        add(block, 0, fused_end(128) + 16)
    add(14, 0, fused_end(128) + 128 * 256)
    for block in range(15, 22):
        add(block, 0, fused_end(256) + 16)
    add(22, 0, fused_end(256) + 256 * 512)

    split = {0: 524288, 1: 263168, 2: 917568, 3: 263168}
    for block in range(23, 31):
        for layer, size in split.items():
            add(block, layer, size)
    add(30, 4, 512 * 1024 + 16)

    vit = {0: 4194320, 1: 4196352, 2: 3145856, 3: 2, 4: 1050624}
    for block in range(31, 39):
        for layer, size in vit.items():
            add(block, layer, size)

    add(39, 0, 1024 * 512 + 512 * 2)
    for block in range(40, 48):
        for layer, size in split.items():
            add(block, layer, size)

    add(48, 0, upsample_length(512, 256))
    for block in range(49, 56):
        add(block, 0, fused_end(256) + 16)
    add(56, 0, upsample_length(256, 128))
    for block in range(57, 62):
        add(block, 0, fused_end(128) + 16)
    add(62, 0, upsample_length(128, 64))
    for block in range(63, 66):
        add(block, 0, fused_end(64) + 16)
    add(66, 0, upsample_length(64, 32))
    for block in range(67, 70):
        add(block, 0, fused_end(32) + 16)

    add(70, 0, 21808)
    add(70, 0, 2, "blend_scale")
    if len(expected) != 153:
        raise AssertionError(f"reference graph formula set has {len(expected)} tensors, expected 153")
    return expected


def verify(model_dir: Path) -> dict:
    manifest_path = model_dir / "manifest.json"
    manifest_bytes = manifest_path.read_bytes()
    manifest_sha = hashlib.sha256(manifest_bytes).hexdigest()
    if manifest_sha != MANIFEST_SHA256:
        raise ValueError("manifest SHA-256 differs from verified export")
    manifest = json.loads(manifest_bytes)
    if manifest.get("format") != "PNE-exported-DLSSNR-packed-v1":
        raise ValueError("unexpected model-directory format")
    if manifest.get("totals", {}).get("blockCount") != 71:
        raise ValueError("manifest blockCount is not 71")
    if len(manifest.get("tensors", [])) != 153:
        raise ValueError("manifest tensor record count is not 153")
    if len(manifest.get("stages", [])) != 1:
        raise ValueError("expected the reproducible single-stage s00 export")

    stage_meta = manifest["stages"][0]
    if stage_meta.get("id") != "s00" or stage_meta.get("file") != "stages/s00.bin":
        raise ValueError("unexpected stage identity/path")
    if stage_meta.get("packedByteLength") != STAGE_BYTES or stage_meta.get("sha256") != STAGE_SHA256:
        raise ValueError("manifest stage size/hash differs from verified payload")

    source = manifest.get("source", {})
    if source.get("resource") != "10/WEIGHTS_HT/1033":
        raise ValueError("unexpected PE resource identity")
    if source.get("resourceBytes") != RESOURCE_BYTES or source.get("resourceSha256") != RESOURCE_SHA256:
        raise ValueError("source resource provenance mismatch")
    if source.get("recordCount") != 153:
        raise ValueError("source record count is not 153")

    stage = (model_dir / "model" / stage_meta["file"]).read_bytes()
    stage_sha = hashlib.sha256(stage).hexdigest()
    if len(stage) != STAGE_BYTES or stage_sha != STAGE_SHA256:
        raise ValueError("stage payload size/hash mismatch")

    actual = {}
    for tensor in manifest["tensors"]:
        name = tensor["name"]
        expected_name = f"block{tensor['block']}.layer{tensor['layer']}.{tensor['parameter']}"
        if name != expected_name:
            raise ValueError(f"tensor identity fields disagree with name: {name}")
        if name in actual:
            raise ValueError(f"duplicate tensor name: {name}")
        if tensor.get("stage") != "s00":
            raise ValueError(f"tensor is not in s00: {name}")
        offset, size = tensor.get("stageOffset"), tensor.get("byteLength")
        if not isinstance(offset, int) or not isinstance(size, int) or offset < 0 or size < 0 or offset + size > len(stage):
            raise ValueError(f"tensor slice out of bounds: {name}")
        actual[name] = (size, offset)

    expected = expected_lengths()
    missing = sorted(set(expected) - set(actual))
    extra = sorted(set(actual) - set(expected))
    if missing or extra:
        raise ValueError(f"tensor name set mismatch: missing={missing[:8]} extra={extra[:8]}")
    mismatches = [(name, expected[name], actual[name][0]) for name in expected if expected[name] != actual[name][0]]
    if mismatches:
        raise ValueError(f"tensor byteLength mismatch: {mismatches[:12]}")

    ordered = sorted((offset, size, name) for name, (size, offset) in actual.items())
    cursor = 0
    for offset, size, name in ordered:
        if offset != cursor:
            raise ValueError(f"stage slice gap/overlap at {name}: offset={offset}, expected={cursor}")
        cursor += size
    if cursor != len(stage):
        raise ValueError(f"tensor slices cover {cursor} bytes, stage has {len(stage)}")

    unread = sorted(
        name for name, (size, _) in actual.items()
        if ".layer3.layer" in name and 31 <= int(name.split(".")[0][5:]) <= 38
    )
    if len(unread) != 8 or any(actual[name][0] != 2 for name in unread):
        raise ValueError("expected eight 2-byte ViT layer3 scalars")

    return {
        "resource_sha256": source["resourceSha256"],
        "stage_sha256": stage_sha,
        "manifest_sha256": manifest_sha,
        "record_count": len(actual),
        "block_count": 71,
        "stage_bytes": len(stage),
        "tensor_layouts_verified": len(actual),
        "unread_vit_layer3_records": unread,
        "graph_reference": "OpenDLSS-NR 9d08f4184bbcb9d858e2fb7a7834ec0837a9d2f1 (310.8.0 graph)",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model_dir", type=Path)
    args = parser.parse_args()
    print(json.dumps(verify(args.model_dir), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
