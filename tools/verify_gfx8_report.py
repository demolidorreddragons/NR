#!/usr/bin/env python3
"""Independent byte-level verifier for the emitted GFX8 shader artifact directory.

This reads the actual .sb, .spv, .gcn.bin and .isa files. It does not invoke
PSBC or trust compiler-generated report fields without comparing them to bytes.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
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
USAGE_NAMES = {0x1C: "PTR_INDIRECTRESOURCETABLE"}
PSSL_HEADER_SIZE = 0x24
GNM_FILE_HEADER_SIZE = 0x10
GNM_CS_SHADER_SIZE = 0x24
GNM_BINARY_INFO_SIZE = 0x1C
CS_REGISTER_NAMES = (
    "computepgmlo",
    "computepgmhi",
    "computepgmrsrc1",
    "computepgmrsrc2",
    "computenumthreadx",
    "computenumthready",
    "computenumthreadz",
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def verify(artifact_dir: Path) -> dict:
    report_path = artifact_dir / "gnm-container-report.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    require(isinstance(report, list) and len(report) == 5,
            "report must contain exactly five shader records")
    names = {record.get("shader") for record in report}
    require(names == EXPECTED_SHADERS,
            f"shader set mismatch: {sorted(names)}")

    results = []
    for record in report:
        name = record["shader"]
        for key, expected in EXPECTED_COMMITS.items():
            require(record.get(key) == expected,
                    f"{name}: {key} provenance mismatch")

        source_path = Path("platform/ps4/gcn8/shaders") / f"{name}.comp"
        spv_path = artifact_dir / f"{name}.spv"
        sb_path = artifact_dir / f"{name}.sb"
        code_path = artifact_dir / f"{name}.gcn.bin"
        isa_path = artifact_dir / f"{name}.isa"
        for path in (source_path, spv_path, sb_path, code_path, isa_path):
            require(path.is_file(), f"{name}: missing artifact {path}")

        source = source_path.read_bytes()
        spv = spv_path.read_bytes()
        sb = sb_path.read_bytes()
        code_file = code_path.read_bytes()
        isa = isa_path.read_bytes()

        require(sha256(source) == record.get("source_sha256"),
                f"{name}: source SHA-256 mismatch")
        require(sha256(spv) == record.get("spirv_sha256"),
                f"{name}: SPIR-V SHA-256 mismatch")
        require(len(spv) == record.get("spirv_bytes"),
                f"{name}: SPIR-V byte count mismatch")
        require(sha256(sb) == record.get("sha256"),
                f"{name}: GNM container SHA-256 mismatch")
        require(len(sb) == record.get("bytes"),
                f"{name}: GNM container byte count mismatch")
        require(sha256(isa) == record.get("isa_sha256"),
                f"{name}: ISA SHA-256 mismatch")
        require(len(isa.decode("utf-8").splitlines()) == record.get("isa_lines"),
                f"{name}: ISA line count mismatch")

        orb_offsets = []
        start = 0
        while True:
            pos = sb.find(b"OrbShdr", start)
            if pos < 0:
                break
            orb_offsets.append(pos)
            start = pos + 1
        require(len(orb_offsets) == 1,
                f"{name}: expected exactly one OrbShdr marker")
        orb = orb_offsets[0]
        require(orb + GNM_BINARY_INFO_SIZE <= len(sb),
                f"{name}: truncated OrbShdr binary info")

        length_word = struct.unpack_from("<I", sb, orb + 8)[0]
        code_size = (length_word >> 8) & 0xFFFFFF
        chunk_offset = sb[orb + 12]
        slot_count = sb[orb + 13]
        code_start = orb - code_size

        require(orb == record.get("orbshdr_offset"),
                f"{name}: OrbShdr offset differs from report")
        require(code_size == record.get("gcn_code_size"),
                f"{name}: GCN code size differs from report")
        require(code_size > 0 and code_size % 4 == 0,
                f"{name}: invalid GCN code size")
        require(chunk_offset == record.get("chunk_usage_base_offset_dwords"),
                f"{name}: chunk usage offset differs from report")
        require(chunk_offset <= 255,
                f"{name}: chunk usage offset exceeds its 8-bit field")
        require(slot_count == record.get("input_usage_slots"),
                f"{name}: input usage count differs from report")

        file_header = PSSL_HEADER_SIZE
        cs_header = file_header + GNM_FILE_HEADER_SIZE
        slot_start = cs_header + GNM_CS_SHADER_SIZE
        common_word = struct.unpack_from("<I", sb, cs_header)[0]
        common_shader_size = common_word & 0x7FFFFF
        common_num_slots = (common_word >> 24) & 0xFF
        require(sb[file_header + 8] == 4 and sb[file_header + 11] == 2,
                f"{name}: not a PS4 Pro NEO compute binary")
        require(common_num_slots == slot_count,
                f"{name}: CS and OrbShdr slot counts disagree")
        require(sb[file_header + 9] * 4 == GNM_CS_SHADER_SIZE + slot_count * 4,
                f"{name}: GNM header size does not match CS header + slots")
        require(code_start == slot_start + slot_count * 4,
                f"{name}: code does not immediately follow CS header/slot table")
        require(common_shader_size == code_size + GNM_BINARY_INFO_SIZE,
                f"{name}: common shader size does not match code + OrbShdr info")

        if slot_count:
            actual_slot_start = orb - chunk_offset * 4
            require(actual_slot_start == slot_start,
                    f"{name}: chunk pointer does not resolve to the actual slot table")
            require(actual_slot_start + slot_count * 4 <= code_start,
                    f"{name}: input usage table overlaps code")
            actual_slots = []
            for index in range(slot_count):
                offset = actual_slot_start + index * 4
                usage_type, api_slot, start_register, control = sb[offset:offset + 4]
                actual_slots.append({
                    "index": index,
                    "usage_type": usage_type,
                    "usage_name": USAGE_NAMES.get(usage_type, "UNKNOWN"),
                    "api_slot": api_slot,
                    "start_register": start_register,
                    "control_raw": control,
                    "register_count_bit": control & 1,
                    "resource_type_bit": (control >> 1) & 1,
                    "chunk_mask": control >> 4,
                })
            require(actual_slots == record.get("input_usage_slots_raw"),
                    f"{name}: raw input usage table differs from report")
            require(len(actual_slots) == 1 and
                    all(slot["usage_type"] == 0x1C and slot["api_slot"] == 0
                        and slot["start_register"] == 2
                        and slot["register_count_bit"] == 0
                        and slot["control_raw"] == 0 for slot in actual_slots),
                    f"{name}: expected one-dword indirect resource table pointer at CS user SGPR 2")
        else:
            require(chunk_offset == 0,
                    f"{name}: non-zero chunk offset with no input usage slots")

        code = sb[code_start:orb]
        require(len(code) == code_size, f"{name}: truncated code range")
        require(sha256(code) == record.get("gcn_code_sha256"),
                f"{name}: code SHA-256 differs from report")
        require(code == code_file,
                f"{name}: extracted .gcn.bin differs from container code")

        register_words = struct.unpack_from("<7I", sb, cs_header + 8)
        actual_registers = dict(zip(CS_REGISTER_NAMES, register_words))
        require(actual_registers == record.get("cs_registers_raw"),
                f"{name}: raw CS register words differ from report")

        register_db_path = Path(".deps/opengnm-psbc/src/amd/registers/gfx8.json")
        require(register_db_path.is_file(), "pinned Mesa GFX8 register database is missing")
        register_db = json.loads(register_db_path.read_text(encoding="utf-8"))["register_types"]

        def decode_fields(value: int, register_name: str) -> dict:
            decoded = {}
            for field in register_db[register_name]["fields"]:
                low, high = field["bits"]
                decoded[field["name"]] = (value >> low) & ((1 << (high - low + 1)) - 1)
            return decoded

        decoded_rsrc1 = decode_fields(actual_registers["computepgmrsrc1"], "COMPUTE_PGM_RSRC1")
        decoded_rsrc2 = decode_fields(actual_registers["computepgmrsrc2"], "COMPUTE_PGM_RSRC2")
        reported_decoded = record.get("cs_registers_decoded_gfx8", {})
        require(decoded_rsrc1 == reported_decoded.get("COMPUTE_PGM_RSRC1"),
                f"{name}: COMPUTE_PGM_RSRC1 decoded fields differ from raw bits")
        require(decoded_rsrc2 == reported_decoded.get("COMPUTE_PGM_RSRC2"),
                f"{name}: COMPUTE_PGM_RSRC2 decoded fields differ from raw bits")

        common = record.get("cs_common_data", {})
        expected_allocation = {
            "vgprs_allocated": (decoded_rsrc1["VGPRS"] + 1) * 4,
            "sgprs_allocated": (decoded_rsrc1["SGPRS"] + 1) * 8,
            "lds_allocation_bytes_gfx8": decoded_rsrc2["LDS_SIZE"] * 512,
            "scratch_bytes_per_thread": common.get("scratch_size_per_thread_dwords", -1) * 4,
            "scratch_enabled": bool(decoded_rsrc2["SCRATCH_EN"]),
            "user_sgpr_count": decoded_rsrc2["USER_SGPR"],
            "trap_present": bool(decoded_rsrc2["TRAP_PRESENT"]),
            "wavefront_size_gfx8": 64,
        }
        require(expected_allocation == record.get("resource_allocation_derived"),
                f"{name}: derived GFX8 allocation differs from raw register bits")
        local = record.get("spirv_local_size")
        compiled = [
            actual_registers["computenumthreadx"],
            actual_registers["computenumthready"],
            actual_registers["computenumthreadz"],
        ]
        require(isinstance(local, list) and len(local) == 3 and compiled == local,
                f"{name}: SPIR-V LocalSize and GNM CS thread dimensions differ")

        allocation = record.get("resource_allocation_derived", {})
        require(allocation.get("wavefront_size_gfx8") == 64,
                f"{name}: GFX8 wavefront size is not 64")
        for key in ("vgprs_allocated", "sgprs_allocated",
                    "lds_allocation_bytes_gfx8", "scratch_bytes_per_thread"):
            require(isinstance(allocation.get(key), int) and allocation[key] >= 0,
                    f"{name}: missing/invalid resource allocation field {key}")

        crc_value = struct.unpack_from("<I", sb, orb + 24)[0]
        require(f"{crc_value:08x}" == record.get("crc32"),
                f"{name}: report CRC field differs from container")
        results.append({
            "shader": name,
            "container_bytes": len(sb),
            "code_bytes": code_size,
            "slot_offset_dwords": chunk_offset,
            "slot_start": slot_start,
            "crc32": f"{crc_value:08x}",
            "code_sha256": sha256(code),
        })

    return {
        "status": "PASS",
        "verification": "byte-level independent artifact/report cross-check",
        "shaders": results,
        "records": len(results),
        "provenance": EXPECTED_COMMITS,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifact_dir", type=Path)
    args = parser.parse_args()
    print(json.dumps(verify(args.artifact_dir), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
