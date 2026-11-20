#include "shader_binary_contract.h"

#include <cstring>
#include <limits>

namespace pne::gcn8 {
namespace {

constexpr std::size_t kPsslHeaderSize = 0x24;
constexpr std::size_t kGnmFileHeaderSize = 0x10;
constexpr std::size_t kGnmCsShaderSize = 0x24;
constexpr std::size_t kGnmBinaryInfoSize = 0x1C;
constexpr std::uint32_t kGnmShaderFileMagic = 0x72646853u; // "Shdr"
constexpr std::uint8_t kComputeShaderType = 0x04;
constexpr std::uint8_t kNeoGpuMode = 0x02;
constexpr std::uint8_t kIndirectResourceTableUsage = 0x1C;

std::uint16_t read_u16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8);
}

std::uint32_t read_u32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

} // namespace

ShaderBinaryError parse_gfx8_cs_shader_binary(
    const std::uint8_t* data,
    std::size_t size,
    CsShaderBinaryView* output) noexcept {
    if (!output) return ShaderBinaryError::null_output;
    *output = {};
    if (!data) return ShaderBinaryError::null_input;

    constexpr std::size_t file_header_offset = kPsslHeaderSize;
    constexpr std::size_t cs_header_offset = kPsslHeaderSize + kGnmFileHeaderSize;
    constexpr std::size_t slot_table_offset = cs_header_offset + kGnmCsShaderSize;
    constexpr std::size_t minimum_size =
        slot_table_offset + kGnmBinaryInfoSize + 4;
    if (size < minimum_size) return ShaderBinaryError::truncated_header;

    const std::uint32_t file_magic = read_u32(data + file_header_offset);
    if (file_magic != kGnmShaderFileMagic)
        return ShaderBinaryError::invalid_file_magic;

    const std::uint16_t version_major = read_u16(data + file_header_offset + 4);
    const std::uint16_t version_minor = read_u16(data + file_header_offset + 6);
    const std::uint8_t shader_type = data[file_header_offset + 8];
    const std::uint8_t header_size_dwords = data[file_header_offset + 9];
    const std::uint8_t target_gpu_mode = data[file_header_offset + 11];
    if (shader_type != kComputeShaderType)
        return ShaderBinaryError::not_compute_shader;
    if (target_gpu_mode != kNeoGpuMode)
        return ShaderBinaryError::not_neo_target;

    const std::uint32_t common_word = read_u32(data + cs_header_offset);
    const std::uint32_t common_shader_size = common_word & 0x7FFFFFu;
    const std::uint32_t common_slot_count = (common_word >> 24) & 0xFFu;
    const std::uint32_t expected_header_bytes =
        static_cast<std::uint32_t>(kGnmCsShaderSize + common_slot_count * 4u);
    if (header_size_dwords * 4u != expected_header_bytes)
        return ShaderBinaryError::invalid_header_size;

    std::size_t orb_offset = 0;
    std::uint32_t orb_count = 0;
    for (std::size_t i = 0; i + 7 <= size; ++i) {
        if (std::memcmp(data + i, "OrbShdr", 7) == 0) {
            orb_offset = i;
            ++orb_count;
        }
    }
    if (orb_count == 0 || orb_count > 1)
        return ShaderBinaryError::missing_or_duplicate_orbshdr;
    if (orb_offset + kGnmBinaryInfoSize > size)
        return ShaderBinaryError::truncated_binary_info;

    const std::uint32_t length_word = read_u32(data + orb_offset + 8);
    const std::uint32_t code_size = (length_word >> 8) & 0x00FFFFFFu;
    const std::uint32_t chunk_offset = data[orb_offset + 12];
    const std::uint32_t slot_count = data[orb_offset + 13];
    if (slot_count != common_slot_count)
        return ShaderBinaryError::slot_count_mismatch;
    if (code_size == 0 || (code_size & 3u) != 0)
        return ShaderBinaryError::invalid_code_size;
    if (chunk_offset > 255u)
        return ShaderBinaryError::invalid_chunk_offset;

    const std::size_t actual_slot_offset =
        chunk_offset ? orb_offset - static_cast<std::size_t>(chunk_offset) * 4u : 0;
    if (slot_count != 1 || actual_slot_offset != slot_table_offset)
        return ShaderBinaryError::slot_table_mismatch;

    const std::size_t code_offset = orb_offset - code_size;
    const std::size_t expected_code_offset = slot_table_offset + slot_count * 4u;
    if (code_offset != expected_code_offset || code_offset > orb_offset ||
        code_size > size - code_offset)
        return ShaderBinaryError::code_range_mismatch;
    if (common_shader_size != code_size + kGnmBinaryInfoSize)
        return ShaderBinaryError::common_shader_size_mismatch;

    const std::uint8_t usage_type = data[actual_slot_offset];
    const std::uint8_t api_slot = data[actual_slot_offset + 1];
    const std::uint8_t start_register = data[actual_slot_offset + 2];
    if (usage_type != kIndirectResourceTableUsage || api_slot != 0 ||
        start_register != 2 || data[actual_slot_offset + 3] != 0)
        return ShaderBinaryError::invalid_resource_table_slot;

    CsStageRegisters registers{};
    registers.computepgmlo = read_u32(data + cs_header_offset + 8);
    registers.computepgmhi = read_u32(data + cs_header_offset + 12);
    registers.computepgmrsrc1 = read_u32(data + cs_header_offset + 16);
    registers.computepgmrsrc2 = read_u32(data + cs_header_offset + 20);
    registers.computenumthreadx = read_u32(data + cs_header_offset + 24);
    registers.computenumthready = read_u32(data + cs_header_offset + 28);
    registers.computenumthreadz = read_u32(data + cs_header_offset + 32);
    if (registers.computepgmlo != expected_header_bytes ||
        registers.computepgmhi != 0 ||
        registers.computenumthreadx == 0 ||
        registers.computenumthready == 0 ||
        registers.computenumthreadz == 0)
        return ShaderBinaryError::invalid_cs_registers;

    output->registers = registers;
    output->code_offset = static_cast<std::uint32_t>(code_offset);
    output->code_size = code_size;
    output->slot_table_offset = static_cast<std::uint32_t>(actual_slot_offset);
    output->slot_count = slot_count;
    output->chunk_usage_base_offset_dwords = chunk_offset;
    output->crc32 = read_u32(data + orb_offset + 24);
    output->version_major = version_major;
    output->version_minor = version_minor;
    output->target_gpu_mode = target_gpu_mode;
    return ShaderBinaryError::none;
}

ShaderBinaryError set_cs_program_gpu_address(
    CsStageRegisters* registers,
    std::uint64_t gpu_address) noexcept {
    constexpr std::uint64_t kAddressLimit = std::uint64_t{1} << 48;
    if (!registers || gpu_address == 0 || (gpu_address & 0xFFu) != 0 ||
        gpu_address >= kAddressLimit)
        return ShaderBinaryError::invalid_gpu_address;
    registers->computepgmlo = static_cast<std::uint32_t>(gpu_address >> 8);
    registers->computepgmhi = static_cast<std::uint32_t>(gpu_address >> 40);
    return ShaderBinaryError::none;
}

const char* shader_binary_error_string(ShaderBinaryError error) noexcept {
    switch (error) {
    case ShaderBinaryError::none: return "none";
    case ShaderBinaryError::null_input: return "null shader binary";
    case ShaderBinaryError::null_output: return "null metadata output";
    case ShaderBinaryError::truncated_header: return "truncated GNM compute header";
    case ShaderBinaryError::invalid_file_magic: return "invalid GNM shader file magic";
    case ShaderBinaryError::not_compute_shader: return "shader is not compute stage";
    case ShaderBinaryError::not_neo_target: return "shader is not PS4 Pro NEO target";
    case ShaderBinaryError::invalid_header_size: return "invalid CS header/slot table size";
    case ShaderBinaryError::slot_count_mismatch: return "CS and OrbShdr slot counts disagree";
    case ShaderBinaryError::missing_or_duplicate_orbshdr: return "missing or duplicate OrbShdr marker";
    case ShaderBinaryError::truncated_binary_info: return "truncated OrbShdr binary info";
    case ShaderBinaryError::invalid_code_size: return "invalid GCN code size";
    case ShaderBinaryError::invalid_chunk_offset: return "input-usage pointer exceeds 8-bit field";
    case ShaderBinaryError::slot_table_mismatch: return "input-usage pointer does not resolve to expected table";
    case ShaderBinaryError::code_range_mismatch: return "GCN code range does not follow CS header and slot table";
    case ShaderBinaryError::common_shader_size_mismatch: return "CS common shader size mismatch";
    case ShaderBinaryError::invalid_resource_table_slot: return "unexpected indirect resource table slot";
    case ShaderBinaryError::invalid_cs_registers: return "invalid CS stage registers";
    case ShaderBinaryError::invalid_gpu_address: return "GPU shader address must be non-zero, 256-byte aligned and below 48 bits";
    }
    return "unknown";
}

} // namespace pne::gcn8
