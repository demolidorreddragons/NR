#ifndef PNE_GCN8_COMPUTE_SHADER_CONTRACT_H
#define PNE_GCN8_COMPUTE_SHADER_CONTRACT_H

#include <cstddef>
#include <cstdint>

namespace pne::gcn8 {

inline constexpr std::size_t kGnmShaderFileHeaderOffset = 0x24;
inline constexpr std::size_t kGnmShaderFileHeaderSize = 0x10;
inline constexpr std::size_t kGnmComputeStageOffset = 0x34;
inline constexpr std::size_t kGnmComputeStageHeaderSize = 0x24;
inline constexpr std::size_t kGnmShaderBinaryInfoSize = 28;
inline constexpr std::uint32_t kGnmShaderMagic = 0x72646853;
inline constexpr std::uint8_t kGnmComputeStage = 0x04;
inline constexpr std::uint8_t kGnmNeoTarget = 0x02;
inline constexpr std::uint8_t kGnmIndirectResourceTableUsage = 0x1C;
inline constexpr std::uint8_t kGnmIndirectResourceTableSgprs = 2;
inline constexpr std::uint8_t kGnmShaderBinaryInfoVersion = 7;

enum class ComputeShaderError : std::uint8_t {
    none,
    null_input,
    null_output,
    truncated_file_header,
    invalid_gnm_magic,
    unsupported_shader_stage,
    unsupported_gpu_target,
    invalid_stage_header_size,
    truncated_stage_header,
    invalid_input_usage_table,
    invalid_code_offset,
    invalid_shader_binary_size,
    truncated_shader_binary_info,
    invalid_shader_binary_info_magic,
    unsupported_shader_binary_info_version,
    shader_binary_info_size_mismatch,
    shader_binary_info_slot_count_mismatch,
    invalid_chunk_usage_offset,
    invalid_shader_hash,
    missing_descriptor_table_slot,
    duplicate_descriptor_table_slot,
    unsupported_descriptor_api_slot,
    descriptor_pointer_out_of_user_sgpr_range,
    invalid_compute_thread_dimensions
};

struct ComputeShaderMetadata {
    std::uint32_t stage_header_size_bytes = 0;
    std::uint32_t code_offset_bytes = 0;
    std::uint32_t shader_binary_size_bytes = 0;
    std::uint32_t shader_code_size_bytes = 0;
    std::uint32_t shader_binary_info_offset_bytes = 0;
    std::uint32_t chunk_usage_base_offset_dwords = 0;
    std::uint32_t shader_hash_low = 0;
    std::uint32_t shader_hash_high = 0;
    std::uint32_t shader_crc32 = 0;
    std::uint32_t compute_pgm_rsrc2 = 0;
    std::uint32_t compute_num_thread_x = 0;
    std::uint32_t compute_num_thread_y = 0;
    std::uint32_t compute_num_thread_z = 0;
    std::uint8_t shader_binary_info_version = 0;
    std::uint8_t shader_binary_info_flags = 0;
    std::uint8_t user_sgpr_count = 0;
    std::uint8_t input_usage_slot_count = 0;
    std::uint8_t descriptor_table_start_register = 0;
    std::uint8_t descriptor_table_api_slot = 0;
    std::uint8_t descriptor_table_usage_type = 0;
    std::uint8_t descriptor_table_flags = 0;
};

// Parses the PS4 GNM compute-stage header and OrbShdr trailer, including the
// set-0 indirect resource-table pointer range. Metadata validation only: this
// does not allocate GPU memory, write user data, submit commands, or prove readback.
ComputeShaderError parse_compute_shader_binary(
    const std::uint8_t* data,
    std::size_t size,
    ComputeShaderMetadata* output) noexcept;

const char* compute_shader_error_string(ComputeShaderError error) noexcept;

} // namespace pne::gcn8

#endif
