#include "compute_smoke.h"

#include <orbis/libkernel.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "gnmdriver.h"
#include "gnm_buffer.h"
#include "gnm_commandbuffer.h"
#include "gnm_drawcommandbuffer.h"
#include "gnm_helpers.h"
#include "gnm_shader.h"
#include "gnm_dataformat.h"

#include "gnm_resource_binding.h"
#include "host/resource_table_contract.h"
#include "host/shader_binary_contract.h"

namespace {
constexpr uint32_t kElementCount = 64;
constexpr uint32_t kDcbBytes = 0x4000;
constexpr uint32_t kDcbDwords = kDcbBytes / sizeof(uint32_t);
constexpr uint64_t kDirectBytes = 0x100000;
constexpr uint64_t kAlignment = 0x10000;
constexpr uint64_t kDcbOffset = 0x00000;
constexpr uint64_t kShaderOffset = 0x10000;
constexpr uint64_t kTableOffset = 0x20000;
constexpr uint64_t kInputOffset = 0x30000;
constexpr uint64_t kOutputOffset = 0x40000;
constexpr uint64_t kFenceOffset = 0x50000;
constexpr uint32_t kFenceMagic = 0x4E52434Du; // "NRCM"
constexpr uint32_t kFencePollCount = 10000;
constexpr uint32_t kFencePollUs = 1000;

static_assert(kDcbOffset + kDcbBytes <= kShaderOffset,
              "DCB overlaps shader storage");
static_assert(kShaderOffset < kTableOffset &&
              kTableOffset + 80 <= kInputOffset,
              "shader/resource table regions overlap");
static_assert(kInputOffset + kElementCount * sizeof(float) <= kOutputOffset,
              "input/output regions overlap");
static_assert(kOutputOffset + kElementCount * sizeof(float) <= kFenceOffset,
              "output/fence regions overlap");
static_assert(kFenceOffset + sizeof(uint32_t) <= kDirectBytes,
              "fence exceeds direct allocation");

struct DirectAllocation {
    off_t direct_offset = 0;
    void* mapped = nullptr;
    bool allocated = false;
    bool keep_until_process_exit = false;

    bool allocate() {
        const size_t direct_size = sceKernelGetDirectMemorySize();
        if (direct_size < kDirectBytes) return false;
        int32_t rc = sceKernelAllocateDirectMemory(
            0, static_cast<off_t>(direct_size), kDirectBytes, kAlignment,
            GNM_DIRECT_MEMORY_TYPE_WC_GARLIC, &direct_offset);
        if (rc != 0) return false;
        allocated = true;
        rc = sceKernelMapDirectMemory(
            &mapped, kDirectBytes, GNM_PROT_CPU_GPU_RW, 0,
            direct_offset, kAlignment);
        if (rc != 0 || !mapped) {
            mapped = nullptr;
            return false;
        }
        return true;
    }

    ~DirectAllocation() {
        if (keep_until_process_exit) return;
        if (mapped) sceKernelMunmap(mapped, kDirectBytes);
        if (allocated) sceKernelReleaseDirectMemory(direct_offset, kDirectBytes);
    }
};

struct Logger {
    FILE* file = nullptr;
    FILE* fallback = stdout;

    explicit Logger(const char* path) {
        if (path && path[0]) file = std::fopen(path, "ab");
    }
    ~Logger() { if (file) std::fclose(file); }

    void line(const char* key, uint64_t value) {
        FILE* out = file ? file : fallback;
        std::fprintf(out, "%s=0x%llx\n", key,
                     static_cast<unsigned long long>(value));
        std::fflush(out);
    }
    void message(const char* text) {
        FILE* out = file ? file : fallback;
        std::fprintf(out, "%s\n", text);
        std::fflush(out);
    }
};

bool read_file(const char* path, uint8_t** bytes, size_t* size) {
    if (!path || !bytes || !size) return false;
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    if (std::fseek(f, 0, SEEK_END) != 0) { std::fclose(f); return false; }
    const long length = std::ftell(f);
    if (length <= 0 || std::fseek(f, 0, SEEK_SET) != 0) {
        std::fclose(f); return false;
    }
    auto* data = static_cast<uint8_t*>(std::malloc(static_cast<size_t>(length)));
    if (!data) { std::fclose(f); return false; }
    const size_t got = std::fread(data, 1, static_cast<size_t>(length), f);
    std::fclose(f);
    if (got != static_cast<size_t>(length)) { std::free(data); return false; }
    *bytes = data;
    *size = got;
    return true;
}

bool address_range(uint64_t address, uint64_t size, uint64_t limit) {
    return address != 0 && address < limit && size <= limit - address;
}
} // namespace

extern "C" int pne_gcn8_compute_smoke_run(const char* shader_path,
                                           const char* log_path) {
    Logger log(log_path);
    log.message("PNE_GCN8_COMPUTE_SMOKE_BEGIN");

    uint8_t* shader_file = nullptr;
    size_t shader_file_size = 0;
    if (!read_file(shader_path, &shader_file, &shader_file_size)) {
        log.message("RESULT=FAIL; REASON=SHADER_FILE_READ");
        return 10;
    }

    pne::gcn8::CsShaderBinaryView shader_view{};
    auto parse_error = pne::gcn8::parse_gfx8_cs_shader_binary(
        shader_file, shader_file_size, &shader_view);
    if (parse_error != pne::gcn8::ShaderBinaryError::none) {
        log.message("RESULT=FAIL; REASON=SHADER_BINARY_PARSE");
        std::free(shader_file);
        return 11;
    }
    if (shader_view.registers.computenumthreadx != kElementCount ||
        shader_view.registers.computenumthready != 1 ||
        shader_view.registers.computenumthreadz != 1) {
        log.message("RESULT=FAIL; REASON=UNEXPECTED_LOCAL_SIZE");
        std::free(shader_file);
        return 12;
    }
    if (shader_view.code_size > kTableOffset - kShaderOffset) {
        log.message("RESULT=FAIL; REASON=SHADER_EXCEEDS_RESERVED_GPU_REGION");
        std::free(shader_file);
        return 12;
    }
    log.line("SHADER_FILE_BYTES", shader_file_size);
    log.line("SHADER_CODE_BYTES", shader_view.code_size);
    log.line("SHADER_CODE_OFFSET", shader_view.code_offset);
    log.line("CHUNK_USAGE_BASE_OFFSET_DWORDS",
             shader_view.chunk_usage_base_offset_dwords);
    log.line("INPUT_USAGE_SLOT_COUNT", shader_view.slot_count);
    log.line("INPUT_USAGE_SLOT_TABLE_OFFSET", shader_view.slot_table_offset);
    log.message("SLOT_POINTER_SEMANTICS=OPEN_GNM_START_POINTER_ASSUMPTION; FPPS4_ALTERNATIVE_UNRESOLVED");

    DirectAllocation memory;
    if (!memory.allocate()) {
        log.message("RESULT=FAIL; REASON=DIRECT_MEMORY_ALLOC_OR_MAP");
        std::free(shader_file);
        return 13;
    }
    const uint64_t base = static_cast<uint64_t>(
        reinterpret_cast<uintptr_t>(memory.mapped));
    log.line("DIRECT_MEMORY_BASE", base);
    log.line("DIRECT_MEMORY_PHYSICAL_OFFSET",
             static_cast<uint64_t>(memory.direct_offset));

    const uint64_t table_address = base + kTableOffset;
    const uint64_t shader_address = base + kShaderOffset;
    const uint64_t input_address = base + kInputOffset;
    const uint64_t output_address = base + kOutputOffset;
    const uint64_t fence_address = base + kFenceOffset;

    // The PSBC usage slot declares address32_hi=0. Fail before dispatch if the
    // mapped resource-table VA cannot be represented by that 32-bit SGPR.
    if (!address_range(table_address, 80, 0x100000000ULL)) {
        log.message("RESULT=BLOCKED; REASON=RESOURCE_TABLE_GPU_VA_NOT_BELOW_4G");
        std::free(shader_file);
        return 14;
    }
    if (!address_range(shader_address, shader_view.code_size, (uint64_t{1} << 40)) ||
        (shader_address & 0xFFu) != 0 ||
        !address_range(input_address, kElementCount * sizeof(float), (uint64_t{1} << 44)) ||
        !address_range(output_address, kElementCount * sizeof(float), (uint64_t{1} << 44)) ||
        !address_range(fence_address, sizeof(uint32_t), (uint64_t{1} << 48)) ||
        (fence_address & 3u) != 0) {
        log.message("RESULT=FAIL; REASON=GPU_ADDRESS_RANGE_OR_ALIGNMENT");
        std::free(shader_file);
        return 15;
    }

    auto* base_bytes = static_cast<uint8_t*>(memory.mapped);
    auto* dcb = reinterpret_cast<uint32_t*>(base_bytes + kDcbOffset);
    auto* shader_code = base_bytes + kShaderOffset;
    auto* table_bytes = base_bytes + kTableOffset;
    auto* input = reinterpret_cast<float*>(base_bytes + kInputOffset);
    auto* output = reinterpret_cast<float*>(base_bytes + kOutputOffset);
    auto* fence = reinterpret_cast<volatile uint32_t*>(base_bytes + kFenceOffset);

    std::memcpy(shader_code, shader_file + shader_view.code_offset,
                shader_view.code_size);
    std::free(shader_file);
    shader_file = nullptr;

    for (uint32_t i = 0; i < kElementCount; ++i) {
        input[i] = static_cast<float>(i) * 0.25f;
        output[i] = -12345.0f;
    }
    *fence = 0;

    // Build the four-dword GNM descriptors directly. OpenGNM's convenience
    // setters call sceGnmWriteMsg on invalid input; that optional diagnostic
    // import is not exported by the pinned OpenOrbis import library.
    const auto make_float_buffer = [](GnmBuffer* descriptor, float* address,
                                      GnmMemoryType memory_type) {
        const uintptr_t gpu_address = reinterpret_cast<uintptr_t>(address);
        *descriptor = {};
        descriptor->baseaddress = static_cast<uint32_t>(gpu_address);
        descriptor->baseaddresshi = (gpu_address >> 32) & 0xFFFu;
        descriptor->stride = sizeof(float);
        descriptor->numrecords = kElementCount;
        const GnmDataFormat format = GNM_FMT_R32_FLOAT;
        descriptor->numformat = static_cast<GnmBufNumFormat>(format.chantype);
        descriptor->dataformat = static_cast<GnmBufferFormat>(format.surfacefmt);
        descriptor->dstselx = format.chanx;
        descriptor->dstsely = format.chany;
        descriptor->dstselz = format.chanz;
        descriptor->dstselw = format.chanw;
        sceGnmBufSetMemoryType(descriptor, memory_type, false, false);
    };

    GnmBuffer input_descriptor{};
    make_float_buffer(&input_descriptor, input, GNM_MEMORY_READONLY);
    GnmBuffer output_descriptor{};
    make_float_buffer(&output_descriptor, output, GNM_MEMORY_UNCACHED);

    std::memset(table_bytes, 0, 80);
    std::memcpy(table_bytes, &input_descriptor, sizeof(input_descriptor));
    std::memcpy(table_bytes + 64, &output_descriptor, sizeof(output_descriptor));

    const pne::gcn8::BindingRequirement requirements[] = {
        {0, 0, 1, pne::gcn8::DescriptorKind::buffer},
        {0, 1, 1, pne::gcn8::DescriptorKind::buffer},
    };
    pne::gcn8::ResourceTableLayout layout{};
    if (pne::gcn8::build_resource_table_layout(requirements, 2, &layout) !=
            pne::gcn8::ResourceTableError::none ||
        layout.byte_size != 80) {
        log.message("RESULT=FAIL; REASON=RESOURCE_TABLE_LAYOUT");
        return 16;
    }

    auto registers = shader_view.registers;
    if (pne::gcn8::set_cs_program_gpu_address(&registers, shader_address) !=
        pne::gcn8::ShaderBinaryError::none) {
        log.message("RESULT=FAIL; REASON=SHADER_GPU_ADDRESS");
        return 17;
    }
    static_assert(sizeof(GnmCsStageRegisters) == sizeof(registers),
                  "OpenGNM CS register ABI mismatch");
    GnmCsStageRegisters cs_registers{};
    std::memcpy(&cs_registers, &registers, sizeof(cs_registers));

    GnmCommandBuffer cmd{};
    cmd.beginptr = dcb;
    cmd.endptr = dcb + kDcbDwords;
    cmd.cmdptr = dcb;
    cmd.sizedwords = kDcbDwords;

    const uint32_t initial_dwords =
        sceGnmDispatchInitDefaultHardwareState(dcb, kDcbDwords);
    if (initial_dwords == 0 || initial_dwords >= kDcbDwords) {
        log.message("RESULT=FAIL; REASON=DEFAULT_COMPUTE_STATE");
        return 18;
    }
    cmd.cmdptr = dcb + initial_dwords;

    uint32_t* before = cmd.cmdptr;
    pne_gcn8_set_cs_shader(&cmd, &cs_registers);
    if (cmd.cmdptr - before != 25) {
        log.message("RESULT=FAIL; REASON=CS_SHADER_BIND_PACKET");
        return 19;
    }

    if (pne_gcn8_set_cs_indirect_resource_table_pointer(
            &cmd, 2, table_address, 80) != PNE_GCN8_BINDING_OK) {
        log.message("RESULT=FAIL; REASON=RESOURCE_TABLE_POINTER");
        return 20;
    }

    before = cmd.cmdptr;
    pne_gcn8_dispatch_direct(&cmd, 1, 1, 1, 0);
    if (cmd.cmdptr - before != 9) {
        log.message("RESULT=FAIL; REASON=DISPATCH_PACKET");
        return 21;
    }

    before = cmd.cmdptr;
    pne_gcn8_emit_eop_fence(&cmd, fence_address, kFenceMagic);
    if (cmd.cmdptr - before != 6) {
        log.message("RESULT=FAIL; REASON=EOP_FENCE_PACKET");
        return 22;
    }

    log.line("RESOURCE_TABLE_GPU_VA", table_address);
    log.line("SHADER_GPU_VA", shader_address);
    log.line("INPUT_GPU_VA", input_address);
    log.line("OUTPUT_GPU_VA", output_address);
    log.line("FENCE_GPU_VA", fence_address);
    log.line("DEFAULT_STATE_DWORDS", initial_dwords);
    log.line("DCB_USED_DWORDS", static_cast<uint64_t>(cmd.cmdptr - dcb));

    sceGnmFlushGarlic();
    void* dcb_addresses[1] = {dcb};
    uint32_t dcb_sizes[1] = {
        static_cast<uint32_t>((cmd.cmdptr - dcb) * sizeof(uint32_t))};
    const int32_t submit_result = sceGnmSubmitCommandBuffers(
        1, dcb_addresses, dcb_sizes, nullptr, nullptr);
    if (submit_result != 0) {
        log.line("SUBMIT_ERROR", static_cast<uint32_t>(submit_result));
        log.message("RESULT=FAIL; REASON=SUBMIT_COMMAND_BUFFERS");
        return 23;
    }
    const int submit_done = sceGnmSubmitDone();
    if (submit_done != 0) {
        log.line("SUBMIT_DONE_ERROR", static_cast<uint32_t>(submit_done));
        memory.keep_until_process_exit = true;
        log.message("RESULT=BLOCKED; REASON=SUBMIT_DONE_FAILED_MEMORY_RETAINED");
        return 24;
    }

    bool fence_seen = false;
    for (uint32_t i = 0; i < kFencePollCount; ++i) {
        if (*fence == kFenceMagic) { fence_seen = true; break; }
        sceKernelUsleep(kFencePollUs);
    }
    if (!fence_seen) {
        memory.keep_until_process_exit = true;
        log.line("FENCE_LAST_VALUE", *fence);
        log.message("RESULT=BLOCKED; REASON=GPU_FENCE_TIMEOUT_MEMORY_RETAINED");
        return 25;
    }

    sceGnmFlushGarlic();
    for (uint32_t i = 0; i < kElementCount; ++i) {
        const float expected = input[i] + 1.0f;
        if (output[i] != expected) {
            log.line("MISMATCH_INDEX", i);
            uint32_t expected_bits = 0, actual_bits = 0;
            std::memcpy(&expected_bits, &expected, sizeof(expected_bits));
            std::memcpy(&actual_bits, &output[i], sizeof(actual_bits));
            log.line("EXPECTED_BITS", expected_bits);
            log.line("ACTUAL_BITS", actual_bits);
            log.message("RESULT=FAIL; REASON=READBACK_MISMATCH");
            return 26;
        }
    }

    log.message("RESULT=PASS; KERNEL=BUFFER_ADD_ONE; ELEMENTS=64");
    log.message("PNE_GCN8_COMPUTE_SMOKE_END");
    return 0;
}
