#include "compute_shader_contract.h"

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "CHECK failed: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

using namespace pne::gcn8;

namespace {

constexpr std::uint32_t kFixtureCodeSize = 80;
constexpr std::uint32_t kFixtureInfoSize = 28;
constexpr std::uint32_t kFixtureParamInfoSize = 28;

void write_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

std::vector<std::uint8_t> make_shader(std::uint8_t slot_count = 1) {
    const std::size_t stage_header_size =
        kGnmComputeStageHeaderSize + static_cast<std::size_t>(slot_count) * 4u;
    const std::size_t code_start = kGnmComputeStageOffset + stage_header_size;
    const std::size_t info_offset = code_start + kFixtureCodeSize;
    const std::size_t total_size = info_offset + kFixtureInfoSize + kFixtureParamInfoSize;
    std::vector<std::uint8_t> bytes(total_size, 0);

    write_u32(bytes, kGnmShaderFileHeaderOffset, kGnmShaderMagic);
    bytes[kGnmShaderFileHeaderOffset + 8] = kGnmComputeStage;
    bytes[kGnmShaderFileHeaderOffset + 9] =
        static_cast<std::uint8_t>(stage_header_size / 4u);
    bytes[kGnmShaderFileHeaderOffset + 11] = kGnmNeoTarget;

    const std::size_t stage = kGnmComputeStageOffset;
    const std::uint32_t shader_binary_size = kFixtureCodeSize + kFixtureInfoSize;
    write_u32(bytes, stage, (static_cast<std::uint32_t>(slot_count) << 24) |
                            shader_binary_size);
    write_u32(bytes, stage + 8, static_cast<std::uint32_t>(stage_header_size));
    write_u32(bytes, stage + 20, 0x88); // USER_SGPR = 4
    write_u32(bytes, stage + 24, 64);
    write_u32(bytes, stage + 28, 1);
    write_u32(bytes, stage + 32, 1);

    static constexpr char kMagic[] = "OrbShdr";
    std::memcpy(bytes.data() + info_offset, kMagic, sizeof(kMagic) - 1);
    bytes[info_offset + 7] = kGnmShaderBinaryInfoVersion;
    bytes[info_offset + 8] = 0x11;
    bytes[info_offset + 9] = static_cast<std::uint8_t>(kFixtureCodeSize);
    bytes[info_offset + 12] = static_cast<std::uint8_t>(
        (info_offset - (stage + kGnmComputeStageHeaderSize)) / 4u);
    bytes[info_offset + 13] = slot_count;
    write_u32(bytes, info_offset + 16, 0x94329A6B);
    write_u32(bytes, info_offset + 20, 0xE15419E8);
    write_u32(bytes, info_offset + 24, 0x7519EC2E);
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
    CHECK(metadata.shader_binary_size_bytes == 108);
    CHECK(metadata.shader_code_size_bytes == 80);
    CHECK(metadata.shader_binary_info_offset_bytes == 0xAC);
    CHECK(metadata.chunk_usage_base_offset_dwords == 21);
    CHECK(metadata.shader_hash_low == 0x94329A6B);
    CHECK(metadata.shader_hash_high == 0xE15419E8);
    CHECK(metadata.shader_crc32 == 0x7519EC2E);
    CHECK(metadata.shader_binary_info_version == 7);
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
    CHECK(parse_compute_shader_binary(valid.data(), 0, &metadata) == ComputeShaderError::truncated_file_header);
    CHECK(parse_compute_shader_binary(valid.data(), valid.size(), nullptr) == ComputeShaderError::null_output);

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
    write_u32(bad, kGnmComputeStageOffset, (1u << 24) | 27u);
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::invalid_shader_binary_size);

    bad = valid;
    bad.resize(0xAC + 27);
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::truncated_shader_binary_info);

    bad = valid;
    bad[0xAC] = 0;
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::invalid_shader_binary_info_magic);

    bad = valid;
    bad[0xAC + 7] = 6;
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::unsupported_shader_binary_info_version);

    bad = valid;
    bad[0xAC + 9] = 79;
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::shader_binary_info_size_mismatch);

    bad = valid;
    bad[0xAC + 13] = 2;
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::shader_binary_info_slot_count_mismatch);

    bad = valid;
    bad[0xAC + 12] = 0;
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::invalid_chunk_usage_offset);

    bad = valid;
    write_u32(bad, 0xAC + 16, 0);
    write_u32(bad, 0xAC + 20, 0);
    CHECK(parse_compute_shader_binary(bad.data(), bad.size(), &metadata) == ComputeShaderError::invalid_shader_hash);

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

    std::ifstream fixture("tests/fixtures/gcn8-compute-smoke.sb", std::ios::binary);
    CHECK(fixture.good());
    const std::vector<std::uint8_t> fixture_bytes(
        (std::istreambuf_iterator<char>(fixture)), std::istreambuf_iterator<char>());
    CHECK(fixture_bytes.size() == 228);
    CHECK(parse_compute_shader_binary(fixture_bytes.data(), fixture_bytes.size(), &metadata) ==
          ComputeShaderError::none);
    CHECK(metadata.shader_code_size_bytes == 80);
    CHECK(metadata.shader_binary_info_offset_bytes == 0xAC);
    CHECK(metadata.descriptor_table_start_register == 2);
    CHECK(metadata.compute_num_thread_x == 64);
    CHECK(metadata.compute_num_thread_y == 1);
    CHECK(metadata.compute_num_thread_z == 1);
    return 0;
}
