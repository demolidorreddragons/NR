#!/usr/bin/env python3
"""Independent GFX8/NEO compute shader-container and descriptor-ABI gate.

This checks the candidate compiler's structural/CRC verifier, the embedded
GNM input-usage table, and the pinned compiler/GNM pointer-width declarations.
It is an offline artifact/source check, not a GPU test.
"""
from __future__ import annotations

import hashlib
import json
import pathlib
import re
import sys

MAGIC = 0x72646853
GNM_HEADER_OFFSET = 0x24
GNM_SHADER_COMPUTE = 0x04
GNM_TARGET_NEO = 0x02
GNM_COMPUTE_SHADER_HEADER_SIZE = 0x24
GNM_INPUT_USAGE_SLOT_SIZE = 0x04
GNM_PTR_INDIRECT_RESOURCE_TABLE = 0x1C


def parse_compute_shader_layout(data: bytes) -> dict:
    stage_offset = GNM_HEADER_OFFSET + 0x10
    if len(data) < stage_offset + GNM_COMPUTE_SHADER_HEADER_SIZE:
        raise ValueError("truncated GnmCsShader header")

    stage_header_words = data[GNM_HEADER_OFFSET + 9]
    stage_header_size = stage_header_words * 4
    if stage_header_size < GNM_COMPUTE_SHADER_HEADER_SIZE:
        raise ValueError("compute stage header is smaller than GnmCsShader")
    if stage_offset + stage_header_size > len(data):
        raise ValueError("compute stage header extends beyond shader binary")

    common_word = int.from_bytes(data[stage_offset:stage_offset + 4], "little")
    slot_count = (common_word >> 24) & 0xFF
    required_stage_size = (
        GNM_COMPUTE_SHADER_HEADER_SIZE + slot_count * GNM_INPUT_USAGE_SLOT_SIZE
    )
    if stage_header_size < required_stage_size:
        raise ValueError("input-usage table exceeds declared compute stage header")

    code_offset = int.from_bytes(data[stage_offset + 8:stage_offset + 12], "little")
    compute_pgm_rsrc2 = int.from_bytes(data[stage_offset + 20:stage_offset + 24], "little")
    compute_user_sgpr_count = (compute_pgm_rsrc2 >> 1) & 0x1F
    compute_num_thread = [
        int.from_bytes(data[stage_offset + offset:stage_offset + offset + 4], "little")
        for offset in (24, 28, 32)
    ]
    if code_offset != stage_header_size:
        raise ValueError(
            f"compute code offset {code_offset} does not match stage header size "
            f"{stage_header_size}"
        )

    slots = []
    slot_base = stage_offset + GNM_COMPUTE_SHADER_HEADER_SIZE
    for index in range(slot_count):
        offset = slot_base + index * GNM_INPUT_USAGE_SLOT_SIZE
        raw = data[offset:offset + GNM_INPUT_USAGE_SLOT_SIZE]
        if len(raw) != GNM_INPUT_USAGE_SLOT_SIZE:
            raise ValueError("truncated GnmInputUsageSlot")
        slots.append({
            "usage_type": raw[0],
            "api_slot": raw[1],
            "start_register": raw[2],
            "flags": raw[3],
        })

    table_slots = [
        slot for slot in slots
        if slot["usage_type"] == GNM_PTR_INDIRECT_RESOURCE_TABLE
    ]
    if len(table_slots) != 1:
        raise ValueError(
            "expected exactly one PTR_INDIRECTRESOURCETABLE input-usage slot, "
            f"found {len(table_slots)}"
        )
    if table_slots[0]["api_slot"] != 0:
        raise ValueError("smoke shader descriptor table must use API slot 0")

    return {
        "stage_header_size_bytes": stage_header_size,
        "compute_code_offset_bytes": code_offset,
        "compute_pgm_rsrc2": compute_pgm_rsrc2,
        "compute_user_sgpr_count": compute_user_sgpr_count,
        "compute_num_thread": compute_num_thread,
        "input_usage_slot_count": slot_count,
        "input_usage_slots": slots,
        "descriptor_table_slot": table_slots[0],
    }


def compiler_descriptor_pointer_layout(compiler_root: pathlib.Path) -> dict:
    source = (compiler_root / "src/amd/vulkan/radv_shader_args.c").read_text()
    match = re.search(
        r"static void\\s+add_descriptor_set\\s*\\([^)]*\\)\\s*\\{(.*?)\\n\\}",
        source,
        re.S,
    )
    if not match:
        raise ValueError("could not locate pinned compiler add_descriptor_set")
    body = match.group(1)
    arg = re.search(
        r"RADV_ADD_ARRAY_ARG\\(state,\\s*AC_ARG_SGPR,\\s*(\\d+),\\s*"
        r"AC_ARG_CONST_ADDR,\\s*descriptors,\\s*set\\)",
        body,
    )
    metadata = re.search(r"ud_info->num_sgprs\\s*=\\s*(\\d+)\\s*;", body)
    user_increment = re.search(
        r"state->args->num_user_sgprs\\s*\\+=\\s*(\\d+)\\s*;", body
    )
    reserve_high = "state->args->ac.num_sgprs_used++;" in body
    if not arg or not metadata or not user_increment:
        raise ValueError("could not read compiler descriptor SGPR allocation")
    payload_sgprs = int(arg.group(1))
    usage_sgprs = int(metadata.group(1))
    declared_user_sgprs = int(user_increment.group(1))
    reserved_high_sgprs = usage_sgprs - payload_sgprs
    if (
        payload_sgprs != 1
        or reserved_high_sgprs != 1
        or not reserve_high
        or declared_user_sgprs != usage_sgprs
    ):
        raise ValueError(
            "compiler must keep a scalar low-address ACO argument while reserving "
            "the second GNM pointer SGPR in both ACO and user-data layouts"
        )
    return {
        "payload_sgprs": payload_sgprs,
        "reserved_high_sgprs": reserved_high_sgprs,
        "usage_sgprs": usage_sgprs,
    }

def gnm_pointer_usage_width(opengnm_root: pathlib.Path) -> int:
    source = (opengnm_root / "include/gnm_shaderbinary.h").read_text()
    marker = "static inline int32_t sceGnmShaderInputUsageTypeSize"
    start = source.find(marker)
    if start < 0:
        raise ValueError("could not locate pinned GNM input-usage width helper")
    end = source.find("default:", start)
    if end < 0:
        raise ValueError("GNM input-usage width helper has no default branch")
    block = source[start:end]
    pointer_case = "case GNM_SHINPUTUSAGE_PTR_INDIRECTRESOURCETABLE:"
    case_start = block.find(pointer_case)
    if case_start < 0:
        raise ValueError("GNM helper does not define indirect resource table width")
    result = re.search(r"return\s+(\d+)\s*;", block[case_start:])
    if not result:
        raise ValueError("could not parse GNM indirect resource table width")
    return int(result.group(1))


def main() -> int:
    if len(sys.argv) != 4:
        print(
            "usage: verify_gcn8_compute_sb.py <compiler-root> <opengnm-root> <shader.sb>",
            file=sys.stderr,
        )
        return 2

    compiler_root = pathlib.Path(sys.argv[1]).resolve()
    opengnm_root = pathlib.Path(sys.argv[2]).resolve()
    shader_path = pathlib.Path(sys.argv[3]).resolve()
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

    try:
        layout = parse_compute_shader_layout(data)
        compiler_pointer = compiler_descriptor_pointer_layout(compiler_root)
        compiler_width = compiler_pointer["usage_sgprs"]
        gnm_width = gnm_pointer_usage_width(opengnm_root)
    except (OSError, ValueError) as exc:
        raise SystemExit(f"compute descriptor ABI inspection failed: {exc}") from exc

    if compiler_width != gnm_width:
        raise SystemExit(
            "descriptor pointer ABI width mismatch: compiler allocates "
            f"{compiler_width} SGPR(s), GNM input-usage contract requires {gnm_width}"
        )
    expected_user_sgprs = (
        layout["descriptor_table_slot"]["start_register"] + compiler_width + 1
    )  # preceding system SGPRs + descriptor pointer + workgroup_id.x
    if layout["compute_user_sgpr_count"] < expected_user_sgprs:
        raise SystemExit(
            "compute PGM_RSRC2 USER_SGPR count is too small: expected at least "
            f"{expected_user_sgprs}, got {layout['compute_user_sgpr_count']}"
        )
    if layout["compute_num_thread"] != [64, 1, 1]:
        raise SystemExit(
            "compute thread-group size mismatch: expected [64, 1, 1], got "
            f"{layout['compute_num_thread']}"
        )
    width_status = "CONSISTENT_SOURCE_WIDTH_AND_REGISTER_COUNT"
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
        "descriptor_table_slot": layout["descriptor_table_slot"],
        "input_usage_slot_count": layout["input_usage_slot_count"],
        "compute_stage_header_size_bytes": layout["stage_header_size_bytes"],
        "compute_pgm_rsrc2": f"0x{layout['compute_pgm_rsrc2']:08x}",
        "compute_user_sgpr_count": layout["compute_user_sgpr_count"],
        "compute_num_thread": layout["compute_num_thread"],
        "compiler_descriptor_arg_sgprs": compiler_width,
        "compiler_descriptor_payload_sgprs": compiler_pointer["payload_sgprs"],
        "compiler_reserved_high_sgprs": compiler_pointer["reserved_high_sgprs"],
        "gnm_indirect_resource_pointer_sgprs": gnm_width,
        "descriptor_pointer_abi_status": width_status,
        "hardware_dispatch_tested": False,
        "firmware_14_02_tested": False,
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
