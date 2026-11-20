#include "shader_binary_contract.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace pne::gcn8;

namespace {
constexpr std::size_t kFileHeader = 0x24;
constexpr std::size_t kCsHeader = 0x34;
constexpr std::size_t kSlotTable = 0x58;
constexpr std::size_t kCode = 0x5c;
constexpr std::size_t kCodeBytes = 16;
constexpr std::size_t kOrb = kCode + kCodeBytes;

void put_u16(std::vector<std::uint8_t>& b, std::size_t o, std::uint16_t v) {
    b[o] = static_cast<std::uint8_t>(v);
    b[o + 1] = static_cast<std::uint8_t>(v >> 8);
}

void put_u32(std::vector<std::uint8_t>& b, std::size_t o, std::uint32_t v) {
    b[o] = static_cast<std::uint8_t>(v);
    b[o + 1] = static_cast<std::uint8_t>(v >> 8);
    b[o + 2] = static_cast<std::uint8_t>(v >> 16);
    b[o + 3] = static_cast<std::uint8_t>(v >> 24);
}

std::vector<std::uint8_t> make_fixture() {
    std::vector<std::uint8_t> b(kOrb + 0x1c, 0);
    put_u32(b, kFileHeader, 0x72646853u); // Shdr
    put_u16(b, kFileHeader + 4, 1);
    put_u16(b, kFileHeader + 6, 0);
    b[kFileHeader + 8] = 4;  // compute
    b[kFileHeader + 9] = 10; // 0x24-byte CS header + one 4-byte slot
    b[kFileHeader + 11] = 2; // PS4 Pro NEO

    put_u32(b, kCsHeader, (1u << 24) | (kCodeBytes + 0x1cu));
    put_u16(b, kCsHeader + 4, 0);
    put_u16(b, kCsHeader + 6, 0);
    put_u32(b, kCsHeader + 8, 40); // binary-relative code offset
    put_u32(b, kCsHeader + 12, 0);
    put_u32(b, kCsHeader + 16, 0x12345678);
    put_u32(b, kCsHeader + 20, 0x00020990);
    put_u32(b, kCsHeader + 24, 8);
    put_u32(b, kCsHeader + 28, 8);
    put_u32(b, kCsHeader + 32, 1);

    b[kSlotTable] = 0x1c; // PTR_INDIRECTRESOURCETABLE
    b[kSlotTable + 1] = 0;
    b[kSlotTable + 2] = 2;
    b[kSlotTable + 3] = 0;
    for (std::size_t i = 0; i < kCodeBytes; ++i)
        b[kCode + i] = static_cast<std::uint8_t>(0xa0u + i);

    std::memcpy(b.data() + kOrb, "OrbShdr", 7);
    put_u32(b, kOrb + 8, static_cast<std::uint32_t>(kCodeBytes << 8));
    b[kOrb + 12] = static_cast<std::uint8_t>((kOrb - kSlotTable) / 4);
    b[kOrb + 13] = 1;
    put_u32(b, kOrb + 24, 0x12345678);
    return b;
}

bool parses(const std::vector<std::uint8_t>& bytes, ShaderBinaryError expected) {
    CsShaderBinaryView view{};
    return parse_gfx8_cs_shader_binary(bytes.data(), bytes.size(), &view) == expected;
}

int verify_file(const char* filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "cannot open " << filename << "\n";
        return 1;
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
    CsShaderBinaryView view{};
    const auto error = parse_gfx8_cs_shader_binary(bytes.data(), bytes.size(), &view);
    if (error != ShaderBinaryError::none) {
        std::cerr << filename << ": " << shader_binary_error_string(error) << "\n";
        return 1;
    }
    std::cout << filename << ": PASS code=" << view.code_size
              << " offset=" << view.slot_table_offset
              << " threads=" << view.registers.computenumthreadx << "x"
              << view.registers.computenumthready << "x"
              << view.registers.computenumthreadz << "\n";
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    auto valid = make_fixture();
    CsShaderBinaryView view{};
    assert(parse_gfx8_cs_shader_binary(valid.data(), valid.size(), &view) == ShaderBinaryError::none);
    assert(view.code_offset == kCode);
    assert(view.code_size == kCodeBytes);
    assert(view.slot_table_offset == kSlotTable);
    assert(view.slot_count == 1);
    assert(view.chunk_usage_base_offset_dwords == 5);
    assert(view.crc32 == 0x12345678);
    assert(view.version_major == 1 && view.version_minor == 0);
    assert(view.target_gpu_mode == 2);
    assert(view.registers.computepgmlo == 40);
    assert(view.registers.computenumthreadx == 8);
    assert(view.registers.computenumthready == 8);
    assert(view.registers.computenumthreadz == 1);

    auto broken = valid;
    broken[kFileHeader] = 0;
    assert(parses(broken, ShaderBinaryError::invalid_file_magic));

    broken = valid;
    broken[kFileHeader + 8] = 1;
    assert(parses(broken, ShaderBinaryError::not_compute_shader));

    broken = valid;
    broken[kFileHeader + 11] = 1;
    assert(parses(broken, ShaderBinaryError::not_neo_target));

    broken = valid;
    broken[kFileHeader + 9] = 9;
    assert(parses(broken, ShaderBinaryError::invalid_header_size));

    broken = valid;
    put_u32(broken, kCsHeader, kCodeBytes + 0x1cu);
    broken[kFileHeader + 9] = 9;
    assert(parses(broken, ShaderBinaryError::slot_count_mismatch));

    broken = valid;
    broken[kOrb + 12] = 6;
    assert(parses(broken, ShaderBinaryError::slot_table_mismatch));

    broken = valid;
    put_u32(broken, kOrb + 8, static_cast<std::uint32_t>(12u << 8));
    assert(parses(broken, ShaderBinaryError::code_range_mismatch));

    broken = valid;
    put_u32(broken, kCsHeader, (1u << 24) | (kCodeBytes + 0x1du));
    assert(parses(broken, ShaderBinaryError::common_shader_size_mismatch));

    broken = valid;
    broken[kSlotTable] = 0x1d;
    assert(parses(broken, ShaderBinaryError::invalid_resource_table_slot));

    broken = valid;
    put_u32(broken, kCsHeader + 8, 36);
    assert(parses(broken, ShaderBinaryError::invalid_cs_registers));

    broken = valid;
    broken[kOrb] = 0;
    assert(parses(broken, ShaderBinaryError::missing_or_duplicate_orbshdr));

    broken = valid;
    std::memcpy(broken.data() + kCode, "OrbShdr", 7);
    assert(parses(broken, ShaderBinaryError::missing_or_duplicate_orbshdr));

    assert(parses(valid, ShaderBinaryError::none));
    assert(parse_gfx8_cs_shader_binary(nullptr, valid.size(), &view) == ShaderBinaryError::null_input);
    assert(parse_gfx8_cs_shader_binary(valid.data(), valid.size(), nullptr) == ShaderBinaryError::null_output);
    assert(parse_gfx8_cs_shader_binary(valid.data(), 8, &view) == ShaderBinaryError::truncated_header);

    CsStageRegisters regs = view.registers;
    assert(set_cs_program_gpu_address(&regs, 0x1234567800ULL) == ShaderBinaryError::none);
    assert(regs.computepgmlo == static_cast<std::uint32_t>(0x1234567800ULL >> 8));
    assert(regs.computepgmhi == static_cast<std::uint32_t>(0x1234567800ULL >> 40));
    assert(set_cs_program_gpu_address(&regs, 0x1234567801ULL) == ShaderBinaryError::invalid_gpu_address);
    assert(set_cs_program_gpu_address(&regs, 0x1000000000000ULL) == ShaderBinaryError::invalid_gpu_address);
    assert(set_cs_program_gpu_address(nullptr, 0x1000) == ShaderBinaryError::invalid_gpu_address);

    for (int i = 1; i < argc; ++i) {
        if (verify_file(argv[i]) != 0) return 1;
    }
    return 0;
}
