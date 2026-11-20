#!/usr/bin/env python3
"""Export the pinned 71-block DLSS-NR reference graph as machine-readable layout metadata."""

import argparse
import json
from pathlib import Path

try:
    from .verify_dlssnr_tensor_layout import expected_lengths
except ImportError:  # direct script execution
    from verify_dlssnr_tensor_layout import expected_lengths

REFERENCE_COMMIT = "9d08f4184bbcb9d858e2fb7a7834ec0837a9d2f1"


def describe_block(block: int) -> dict:
    if block == 0:
        return {"role": "pre", "family": "pre_fused", "channels_in": 16, "channels_out": 32}
    if 1 <= block <= 3:
        return {"role": "encoder", "family": "fused_32", "channels_in": 32, "channels_out": 32}
    if block == 4:
        return {"role": "encoder_transition", "family": "fused_32", "channels_in": 32, "channels_out": 64}
    if 5 <= block <= 7:
        return {"role": "encoder", "family": "fused_64", "channels_in": 64, "channels_out": 64}
    if block == 8:
        return {"role": "encoder_transition", "family": "fused_64", "channels_in": 64, "channels_out": 128}
    if 9 <= block <= 13:
        return {"role": "encoder", "family": "fused_128", "channels_in": 128, "channels_out": 128}
    if block == 14:
        return {"role": "encoder_transition", "family": "fused_128", "channels_in": 128, "channels_out": 256}
    if 15 <= block <= 21:
        return {"role": "encoder", "family": "fused_256", "channels_in": 256, "channels_out": 256}
    if block == 22:
        return {"role": "encoder_transition", "family": "fused_256", "channels_in": 256, "channels_out": 512}
    if 23 <= block <= 29:
        return {"role": "encoder", "family": "split_512", "channels_in": 512, "channels_out": 512}
    if block == 30:
        return {"role": "encoder_to_vit_transition", "family": "split_512_plus_projection", "channels_in": 512, "channels_out": 1024}
    if 31 <= block <= 38:
        return {"role": "global_bottleneck", "family": "vit_1024", "channels_in": 1024, "channels_out": 1024}
    if block == 39:
        return {"role": "vit_to_decoder_transition", "family": "projection_1024_to_512", "channels_in": 1024, "channels_out": 512}
    if 40 <= block <= 47:
        return {"role": "decoder", "family": "split_512", "channels_in": 512, "channels_out": 512}
    if block == 48:
        return {"role": "decoder_upsample", "family": "upsample_512_to_256", "channels_in": 512, "channels_out": 256}
    if 49 <= block <= 55:
        return {"role": "decoder", "family": "fused_256", "channels_in": 256, "channels_out": 256}
    if block == 56:
        return {"role": "decoder_upsample", "family": "upsample_256_to_128", "channels_in": 256, "channels_out": 128}
    if 57 <= block <= 61:
        return {"role": "decoder", "family": "fused_128", "channels_in": 128, "channels_out": 128}
    if block == 62:
        return {"role": "decoder_upsample", "family": "upsample_128_to_64", "channels_in": 128, "channels_out": 64}
    if 63 <= block <= 65:
        return {"role": "decoder", "family": "fused_64", "channels_in": 64, "channels_out": 64}
    if block == 66:
        return {"role": "decoder_upsample", "family": "upsample_64_to_32", "channels_in": 64, "channels_out": 32}
    if 67 <= block <= 69:
        return {"role": "decoder", "family": "fused_32", "channels_in": 32, "channels_out": 32}
    if block == 70:
        return {"role": "post", "family": "post_32_to_4", "channels_in": 32, "channels_out": 4}
    raise ValueError(f"invalid block id {block}")


def build_graph_layout() -> dict:
    lengths = expected_lengths()
    blocks = []
    for block in range(71):
        descriptor = describe_block(block)
        records = []
        for name, size in sorted(lengths.items()):
            if name.startswith(f"block{block}."):
                layer_and_param = name.split(".", 2)
                layer = int(layer_and_param[1][5:])
                parameter = layer_and_param[2]
                consumed = "demo" if parameter == "blend_scale" else (
                    "unused_by_current_graph" if 31 <= block <= 38 and layer == 3 else "graph"
                )
                records.append({
                    "layer": layer,
                    "parameter": parameter,
                    "bytes": size,
                    "consumer": consumed,
                })
        blocks.append({"id": block, **descriptor, "tensor_records": records})

    transitions = [
        {"after_block": 4, "from_channels": 32, "to_channels": 64, "kind": "downsample"},
        {"after_block": 8, "from_channels": 64, "to_channels": 128, "kind": "downsample"},
        {"after_block": 14, "from_channels": 128, "to_channels": 256, "kind": "downsample"},
        {"after_block": 22, "from_channels": 256, "to_channels": 512, "kind": "downsample"},
        {"after_block": 30, "from_channels": 512, "to_channels": 1024, "kind": "encoder_projection"},
        {"after_block": 38, "from_channels": 1024, "to_channels": 512, "kind": "vit_projection"},
        {"after_block": 47, "from_channels": 512, "to_channels": 256, "kind": "upsample"},
        {"after_block": 55, "from_channels": 256, "to_channels": 128, "kind": "upsample"},
        {"after_block": 61, "from_channels": 128, "to_channels": 64, "kind": "upsample"},
        {"after_block": 65, "from_channels": 64, "to_channels": 32, "kind": "upsample"},
        {"after_block": 69, "from_channels": 32, "to_channels": 4, "kind": "post_head"},
    ]
    return {
        "schema": "nr-dlssnr-reference-graph-layout-v1",
        "reference": {
            "repository": "maanHimself/OpenDLSS-NR",
            "commit": REFERENCE_COMMIT,
            "version": "310.8.0 reference graph",
            "files": ["src/nr_graph.cpp", "src/nr_graph.h", "docs/network.md", "docs/weights.md"],
        },
        "model": {
            "weight_resource": "10/WEIGHTS_HT/1033",
            "weight_resource_sha256": "836f445d06ecd2e59bb9f17b84b91c143396fd76ccda1c9dc7fe81d5edd548f4",
            "packed_stage_sha256": "37a293b5b781b9351a8b4b5fbeb2ff68fbd8a84891378950812ff3b99d161116",
            "input_lanes": 16,
            "output_lanes": 4,
            "blocks": 71,
            "tensor_records": len(lengths),
        },
        "transitions": transitions,
        "blocks": blocks,
        "notes": [
            "This is a reference graph/layout description, not a PS4-executable graph.",
            "Eight 2-byte ViT layer3 records are retained in the packed model but unread by the current reference graph.",
            "The 2-byte block70.layer0.blend_scale record is consumed by demo composition.",
            "Tensor payload values are not decoded by this manifest; expected packed byte lengths are reconciled against the pinned reference layout formulas.",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    payload = json.dumps(build_graph_layout(), indent=2) + "\n"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(payload, encoding="utf-8")
    print(f"Wrote {args.output} ({len(payload.encode())} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
