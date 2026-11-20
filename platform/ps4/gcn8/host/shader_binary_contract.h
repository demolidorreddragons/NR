#ifndef PNE_GCN8_SHADER_BINARY_CONTRACT_H
#define PNE_GCN8_SHADER_BINARY_CONTRACT_H

#include <cstddef>
#include <cstdint>

namespace pne::gcn8 {

struct CsStageRegisters {
    std::uint32_t computepgmlo;
    std::uint32_t computepgmhi;
    std::uint32_t computepgmrsrc1;
    std::uint32_t computepgmrsrc2;
    std::uint32_t computenumthreadx;
    std::uint32_t computenumthready;
    std::uint32_t computenumthreadz;
};

struct InputUsageSlot {
    std::uint8_t usage_type;
    std::uint8_t api_slot;
    std::uint8_t start_register;
    std::uint8_t control;
};

struct CsShaderBinaryView {
    CsStageRegisters registers{};
    std::uint32_t code_offset = 0;
    std::uint32_t code_size = 0;
    std::uint32_t slot_table_offset = 0;
    std::uint32_t slot_count = 0;
    std::uint32_t crc32 = 0;
    std::uint16_t version_major = 0;
    std::uint16_t version_minor = 0;
    std::uint8_t target_gpu_mode = 0;
};

enum class ShaderBinaryError {
    none,
    null_input,
    null_output,
    truncated_header,
    invalid_file_magic,
    not_compute_shader,
    not_neo_target,
    invalid_header_size,
    slot_count_mismatch,
    missing_or_duplicate_orbshdr,
    truncated_binary_info,
    invalid_code_size,
    invalid_chunk_offset,
    slot_table_mismatch,
    code_range_mismatch,
    common_shader_size_mismatch,
    invalid_resource_table_slot,
    invalid_cs_registers,
    invalid_gpu_address
};

// Parses the PS4 NEO/GFX8 compute binary emitted by the pinned PSBC toolchain.
// This validates structure and extracts registers; it does not recompute CRC.
ShaderBinaryError parse_gfx8_cs_shader_binary(
    const std::uint8_t* data,
    std::size_t size,
    CsShaderBinaryView* output) noexcept;

// Replaces the binary-relative program offset with a GPU virtual code address.
// GNM encodes the address in 256-byte units across the low/high CS registers.
ShaderBinaryError set_cs_program_gpu_address(
    CsStageRegisters* registers,
    std::uint64_t gpu_address) noexcept;

const char* shader_binary_error_string(ShaderBinaryError error) noexcept;

} // namespace pne::gcn8

#endif
