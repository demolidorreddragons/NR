#include "compute_shader_contract.h"

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <vector>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "CHECK failed: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

using namespace pne::gcn8;

namespace {

void write_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

std::vector<std::uint8_t> make_shader(std::uint8_t slot_count = 1) {
    const std::size_t stage_header_size = 0x24 + static_cast<std::size_t>(slot_count) * 4;
    std::vector<std::uint8_t> bytes(kGnmComputeStageOffset + stage_header_size + 16, 0);
    write_u32(bytes, kGnmShaderFileHeaderOffset, kGnmShaderMagic);
    bytes[kGnmShaderFileHeaderOffset + 8] = kGnmComputeStage;
    bytes[kGnmShaderFileHeaderOffset + 9] =
        static_cast<std::uint8_t>(stage_header_size / 4);
    bytes[kGnmShaderFileHeaderOffset + 11] = kGnmNeoTarget;

    const std::size_t stage = kGnmComputeStageOffset;
    write_u32(bytes, stage, static_cast<std::uint32_t>(slot_count) << 24);
    write_u32(bytes, stage + 8, static_cast<std::uint32_t>(stage_header_size));
    write_u32(bytes, stage + 20, 0x88); // USER_SGPR = 4
    write_u32(bytes, stage + 24, 64);
    write_u32(bytes, stage + 28, 1);
    write_u32(bytes, stage + 32, 1);
    return bytes;
}

void set_slot(std::vector<std::uint8_t>& bytes, std::size_t index,
              std::uint8_t usage, std::uint8_t api_slot,
              std::uint8_t start_register, std::uint8_t flags = 0) {
    const std::size_t offset =
        kGnmComputeStageOffset + kGnmComputeStageHeaderSize + index * 4;
    bytes[offset] = usage;
    bytes[offset + 1] = api_slot;
    bytes[offset + 2] = start_register;
    bytes[offset + 3] = flags;
}

} // namespace

int main() {
    ComputeShaderMetadata metadata{};
    auto valid = make_shader();
    set_slot(valid, 0, kGnmIndirectResourceTableUsage, 0, 2);
    CHECK(parse_compute_shader_binary(valid.data(), valid.size(), &metadata) == ComputeShaderError::none);
    CHECK(metadata.stage_header_size_bytes == 0x28);
    CHECK(metadata.code_offset_bytes == 0x28);
    CHECK(metadata.compute_pgm_rsrc2 == 0x88);
    CHECK(metadata.user_sgpr_count == 4);
    CHECK(metadata.compute_num_thread_x == 64);
    CHECK(metadata.compute_num_thread_y == 1);
    CHECK(metadata.compute_num_thread_z == 1);
    CHECK(metadata.input_usage_slot_count == 1);
    CHECK(metadata.descriptor_table_usage_type == 0x1C);
    CHECK(metadata.descriptor_table_api_slot == 0);
    CHECK(metadata.descriptor_table_start_register == 2);

    CHECK(parse_compute_shader_binary(nullptr, 0, &metadata) == ComputeShaderError::null_input);
    CHECK(parse_compute_shader_binary(valid.data(), valid.size(), nullptr) == ComputeShaderError::null_output);
    CHECK(parse_compute_shader_binary(valid.data(), 0x20, &metadata) == ComputeShaderError::truncated_file_header);

    auto bad = valid;
    write_u32(bad, kGnmShaderFileHeaderOffset, 0);
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::invalid_gnm_magic);

    bad = valid;
    bad[kGnmShaderFileHeaderOffset + 8] = 0x03;
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::unsupported_shader_stage);

    bad = valid;
    bad[kGnmShaderFileHeaderOffset + 11] = 0x01;
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::unsupported_gpu_target);

    bad = valid;
    bad[kGnmShaderFileHeaderOffset + 9] = 8;
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::invalid_stage_header_size);

    bad = valid;
    bad.resize(kGnmComputeStageOffset + 0x27);
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::truncated_stage_header);

    bad = valid;
    write_u32(bad, kGnmComputeStageOffset + 8, 0x24);
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::invalid_code_offset);

    bad = valid;
    write_u32(bad, kGnmComputeStageOffset, 2u << 24);
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::invalid_input_usage_table);

    bad = valid;
    set_slot(bad, 0, 0x1D, 0, 2);
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::missing_descriptor_table_slot);

    bad = make_shader(2);
    set_slot(bad, 0, kGnmIndirectResourceTableUsage, 0, 2);
    set_slot(bad, 1, kGnmIndirectResourceTableUsage, 0, 2);
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::duplicate_descriptor_table_slot);

    bad = valid;
    set_slot(bad, 0, kGnmIndirectResourceTableUsage, 1, 2);
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::unsupported_descriptor_api_slot);

    bad = valid;
    set_slot(bad, 0, kGnmIndirectResourceTableUsage, 0, 3);
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::descriptor_pointer_out_of_user_sgpr_range);

    bad = valid;
    write_u32(bad, kGnmComputeStageOffset + 24, 0);
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::invalid_compute_thread_dimensions);
    return 0;
}
