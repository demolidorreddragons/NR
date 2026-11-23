#include "compute_shader_contract.h"

namespace pne::gcn8 {
namespace {

std::uint32_t read_u32_le(const std::uint8_t* data) noexcept {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

} // namespace

ComputeShaderError parse_compute_shader_binary(
    const std::uint8_t* data,
    std::size_t size,
    ComputeShaderMetadata* output) noexcept {
    if (!output)
        return ComputeShaderError::null_output;
    *output = {};
    if (!data)
        return ComputeShaderError::null_input;

    constexpr std::size_t file_header_end =
        kGnmShaderFileHeaderOffset + kGnmShaderFileHeaderSize;
    if (size < file_header_end)
        return ComputeShaderError::truncated_file_header;

    if (read_u32_le(data + kGnmShaderFileHeaderOffset) != kGnmShaderMagic)
        return ComputeShaderError::invalid_gnm_magic;
    if (data[kGnmShaderFileHeaderOffset + 8] != kGnmComputeStage)
        return ComputeShaderError::unsupported_shader_stage;
    if (data[kGnmShaderFileHeaderOffset + 11] != kGnmNeoTarget)
        return ComputeShaderError::unsupported_gpu_target;

    const std::uint32_t stage_header_size =
        static_cast<std::uint32_t>(data[kGnmShaderFileHeaderOffset + 9]) * 4u;
    if (stage_header_size < kGnmComputeStageHeaderSize)
        return ComputeShaderError::invalid_stage_header_size;

    if (size < kGnmComputeStageOffset + stage_header_size)
        return ComputeShaderError::truncated_stage_header;

    const std::uint32_t common_data = read_u32_le(data + kGnmComputeStageOffset);
    const std::uint8_t slot_count = static_cast<std::uint8_t>(common_data >> 24);
    const std::size_t required_stage_bytes =
        kGnmComputeStageHeaderSize + static_cast<std::size_t>(slot_count) * 4u;
    if (required_stage_bytes > stage_header_size)
        return ComputeShaderError::invalid_input_usage_table;

    const std::uint32_t code_offset =
        read_u32_le(data + kGnmComputeStageOffset + 8);
    if (code_offset != stage_header_size ||
        kGnmComputeStageOffset + code_offset >= size)
        return ComputeShaderError::invalid_code_offset;

    const std::uint32_t pgm_rsrc2 =
        read_u32_le(data + kGnmComputeStageOffset + 20);
    const std::uint8_t user_sgpr_count =
        static_cast<std::uint8_t>((pgm_rsrc2 >> 1) & 0x1Fu);

    const std::uint32_t thread_x =
        read_u32_le(data + kGnmComputeStageOffset + 24);
    const std::uint32_t thread_y =
        read_u32_le(data + kGnmComputeStageOffset + 28);
    const std::uint32_t thread_z =
        read_u32_le(data + kGnmComputeStageOffset + 32);
    if (thread_x == 0 || thread_y == 0 || thread_z == 0)
        return ComputeShaderError::invalid_compute_thread_dimensions;

    bool found_descriptor_table = false;
    std::uint8_t descriptor_api_slot = 0;
    std::uint8_t descriptor_start_register = 0;
    std::uint8_t descriptor_usage_type = 0;
    std::uint8_t descriptor_flags = 0;
    const std::size_t slots_offset =
        kGnmComputeStageOffset + kGnmComputeStageHeaderSize;

    for (std::uint8_t i = 0; i < slot_count; ++i) {
        const std::size_t offset = slots_offset + static_cast<std::size_t>(i) * 4u;
        const std::uint8_t usage_type = data[offset];
        if (usage_type != kGnmIndirectResourceTableUsage)
            continue;
        if (found_descriptor_table)
            return ComputeShaderError::duplicate_descriptor_table_slot;
        found_descriptor_table = true;
        descriptor_usage_type = usage_type;
        descriptor_api_slot = data[offset + 1];
        descriptor_start_register = data[offset + 2];
        descriptor_flags = data[offset + 3];
    }

    if (!found_descriptor_table)
        return ComputeShaderError::missing_descriptor_table_slot;
    if (descriptor_api_slot != 0)
        return ComputeShaderError::unsupported_descriptor_api_slot;
    if (static_cast<unsigned>(descriptor_start_register) +
            kGnmIndirectResourceTableSgprs > user_sgpr_count)
        return ComputeShaderError::descriptor_pointer_out_of_user_sgpr_range;

    output->stage_header_size_bytes = stage_header_size;
    output->code_offset_bytes = code_offset;
    output->compute_pgm_rsrc2 = pgm_rsrc2;
    output->compute_num_thread_x = thread_x;
    output->compute_num_thread_y = thread_y;
    output->compute_num_thread_z = thread_z;
    output->user_sgpr_count = user_sgpr_count;
    output->input_usage_slot_count = slot_count;
    output->descriptor_table_start_register = descriptor_start_register;
    output->descriptor_table_api_slot = descriptor_api_slot;
    output->descriptor_table_usage_type = descriptor_usage_type;
    output->descriptor_table_flags = descriptor_flags;
    return ComputeShaderError::none;
}

const char* compute_shader_error_string(ComputeShaderError error) noexcept {
    switch (error) {
    case ComputeShaderError::none: return "none";
    case ComputeShaderError::null_input: return "null input";
    case ComputeShaderError::null_output: return "null output";
    case ComputeShaderError::truncated_file_header: return "truncated GNM file header";
    case ComputeShaderError::invalid_gnm_magic: return "invalid GNM shader magic";
    case ComputeShaderError::unsupported_shader_stage: return "shader is not a compute stage";
    case ComputeShaderError::unsupported_gpu_target: return "shader target is not PS4 Pro NEO";
    case ComputeShaderError::invalid_stage_header_size: return "invalid compute-stage header size";
    case ComputeShaderError::truncated_stage_header: return "truncated compute-stage header";
    case ComputeShaderError::invalid_input_usage_table: return "input-usage table exceeds stage header";
    case ComputeShaderError::invalid_code_offset: return "compute code offset does not match stage header";
    case ComputeShaderError::missing_descriptor_table_slot: return "set-0 indirect resource-table slot is missing";
    case ComputeShaderError::duplicate_descriptor_table_slot: return "multiple indirect resource-table slots are unsupported";
    case ComputeShaderError::unsupported_descriptor_api_slot: return "only descriptor API slot 0 is supported";
    case ComputeShaderError::descriptor_pointer_out_of_user_sgpr_range: return "descriptor pointer exceeds USER_SGPR range";
    case ComputeShaderError::invalid_compute_thread_dimensions: return "compute thread dimensions must be non-zero";
    }
    return "unknown";
}

} // namespace pne::gcn8
