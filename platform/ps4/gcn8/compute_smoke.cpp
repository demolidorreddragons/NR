#include "compute_smoke.h"
#include "stage_log.h"
#include "gnm_runtime_resolver.h"
#include "gnm_provider_census.h"

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
#include "host/vsharp_codec.h"
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

struct DirectAllocation {
    off_t direct_offset = 0;
    void* mapped = nullptr;
    bool allocated = false;
    bool keep_until_process_exit = false;

    bool allocate(Logger& log) {
        log.message("STAGE=BEFORE_GET_DIRECT_MEMORY_SIZE");
        const size_t direct_size = sceKernelGetDirectMemorySize();
        log.line("DIRECT_MEMORY_AVAILABLE_BYTES", direct_size);
        log.message("STAGE=AFTER_GET_DIRECT_MEMORY_SIZE");
        if (direct_size < kDirectBytes) return false;
        log.message("STAGE=BEFORE_ALLOCATE_DIRECT_MEMORY");
        int32_t rc = sceKernelAllocateDirectMemory(
            0, static_cast<off_t>(direct_size), kDirectBytes, kAlignment,
            GNM_DIRECT_MEMORY_TYPE_WC_GARLIC, &direct_offset);
        log.line("ALLOCATE_DIRECT_MEMORY_RESULT", static_cast<uint32_t>(rc));
        if (rc != 0) return false;
        allocated = true;
        log.message("STAGE=BEFORE_MAP_DIRECT_MEMORY");
        rc = sceKernelMapDirectMemory(
            &mapped, kDirectBytes, GNM_PROT_CPU_GPU_RW, 0,
            direct_offset, kAlignment);
        log.line("MAP_DIRECT_MEMORY_RESULT", static_cast<uint32_t>(rc));
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

bool kernel_dlsym_lookup(void* context, const char* name, void** address) {
    if (!context || !name || !address) return false;
    *address = nullptr;
    const auto module = *static_cast<const int32_t*>(context);
    return sceKernelDlsym(module, name, address) == 0 && *address != nullptr;
}

bool passive_module_list(void*, uint32_t* handles, size_t capacity,
                         size_t* count) {
    // Keep the Orbis API's native element type at its boundary. The policy
    // layer uses uint32_t handles, so convert explicitly instead of aliasing
    // its storage as OrbisKernelModule[].
    constexpr size_t kMaxCensusModules = 256;
    if (!handles || !count || capacity == 0 ||
        capacity > kMaxCensusModules) {
        return false;
    }

    OrbisKernelModule modules[kMaxCensusModules]{};
    if (sceKernelGetModuleList(modules, capacity, count) != 0) return false;

    const size_t copied = *count < capacity ? *count : capacity;
    for (size_t i = 0; i < copied; ++i)
        handles[i] = static_cast<uint32_t>(modules[i]);
    return true;
}

bool passive_module_name(void*, uint32_t handle, char* name, size_t capacity) {
    if (!name || capacity == 0) return false;
    OrbisKernelModuleInfo info{};
    info.size = sizeof(info);
    if (sceKernelGetModuleInfo(static_cast<OrbisKernelModule>(handle), &info) != 0)
        return false;
    std::strncpy(name, info.name, capacity - 1);
    name[capacity - 1] = '\\0';
    return true;
}

// Passive-only policy: inspect both providers, never load or start either one.
bool find_loaded_gnm_runtime_module(int32_t* module_handle, Logger& log) {
    if (!module_handle) return false;
    *module_handle = -1;
    uint32_t handles[256]{};
    const auto census = pne::gcn8::runtime::census_loaded_gnm_providers(
        passive_module_list, passive_module_name, nullptr, handles, 256);
    log.line("GNM_CENSUS_STATUS", static_cast<uint32_t>(census.status));
    if (census.status !=
        pne::gcn8::runtime::ProviderCensusStatus::complete) {
        log.message("GNM_MODULE_CENSUS_INCOMPLETE=1");
        return false;
    }
    if (census.has_normal)
        log.line("GNM_NORMAL_PROVIDER_HANDLE", census.normal_handle);
    if (census.has_neo)
        log.line("GNM_NEO_PROVIDER_HANDLE", census.neo_handle);
    log.line("GNM_NORMAL_PROVIDER_PRESENT", census.has_normal ? 1 : 0);
    log.line("GNM_NEO_PROVIDER_PRESENT", census.has_neo ? 1 : 0);
    uint32_t selected_handle = 0;
    if (!pne::gcn8::runtime::select_gnm_provider_handle(
            census, &selected_handle)) {
        log.message("GNM_PROVIDER_NOT_FOUND_NO_MODULE_LOAD_ATTEMPTED");
        return false;
    }
    *module_handle = static_cast<int32_t>(selected_handle);
    log.line("GNM_SELECTED_PROVIDER_HANDLE", selected_handle);
    log.message(census.has_normal
        ? "GNM_SELECTED_PROVIDER=NORMAL"
        : "GNM_SELECTED_PROVIDER=NEO");
    return true;
}
} // namespace

extern "C" int pne_gcn8_compute_smoke_run(const char* shader_path,
                                           const char* log_path) {
    Logger log(log_path);
    stage_log("COMPUTE_SMOKE_ENTERED");
    log.message("PNE_GCN8_COMPUTE_SMOKE_BEGIN");
    log.message("STAGE=ENTER_COMPUTE_SMOKE");

#ifndef PNE_GCN8_ENABLE_UNVALIDATED_COMPUTE
    log.message("RESULT=BLOCKED; REASON=COMPUTE_HARDWARE_GATES_NOT_CLEARED");
    log.message("COMPUTE_DISPATCH_NOT_ATTEMPTED");
    return 27;
#endif

    int32_t gnm_module_handle = -1;
    if (!find_loaded_gnm_runtime_module(&gnm_module_handle, log)) {
        log.message("RESULT=BLOCKED; REASON=GNM_RUNTIME_MODULE_UNAVAILABLE");
        return 28;
    }
    pne::gcn8::runtime::GnmRuntimeFunctions gnm{};
    if (!pne::gcn8::runtime::resolve_gnm_runtime_functions(
            kernel_dlsym_lookup, &gnm_module_handle, &gnm)) {
        log.message("RESULT=BLOCKED; REASON=GNM_REQUIRED_EXPORT_MISSING");
        return 29;
    }
    log.message("GNM_RUNTIME_EXPORTS_RESOLVED=4");

    uint8_t* shader_file = nullptr;
    size_t shader_file_size = 0;
    log.message("STAGE=BEFORE_SHADER_OPEN");
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
    stage_log("SHADER_LOADED_AND_PARSED");
    log.line("SHADER_FILE_BYTES", shader_file_size);
    log.line("SHADER_CODE_BYTES", shader_view.code_size);
    log.line("SHADER_CODE_OFFSET", shader_view.code_offset);
    log.line("CHUNK_USAGE_BASE_OFFSET_DWORDS",
             shader_view.chunk_usage_base_offset_dwords);
    log.line("INPUT_USAGE_SLOT_COUNT", shader_view.slot_count);
    log.line("INPUT_USAGE_SLOT_TABLE_OFFSET", shader_view.slot_table_offset);
    log.message("SLOT_POINTER_SEMANTICS=PSBC_CONFIRMED_ORBSHDR_MINUS_CHUNKUSAGEBASEOFFSET_DWORDS");

    DirectAllocation memory;
    log.message("STAGE=BEFORE_DIRECT_MEMORY_ALLOCATE");
    if (!memory.allocate(log)) {
        log.message("RESULT=FAIL; REASON=DIRECT_MEMORY_ALLOC_OR_MAP");
        std::free(shader_file);
        return 13;
    }
    stage_log("MEM_ALLOC_OK");
    log.message("STAGE=DIRECT_MEMORY_ALLOCATED");
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

    // Use the same four-dword V# codec exercised by host tests. Avoid the
    // OpenGNM convenience setters here because their error path imports the
    // optional sceGnmWriteMsg diagnostic export.
    const auto make_float_buffer = [](GnmBuffer* descriptor, float* address,
                                      GnmMemoryType memory_type) {
        if (!descriptor || !address) return pne::gcn8::VSharpCodecError::null_output;
        const GnmDataFormat format = GNM_FMT_R32_FLOAT;
        const std::uint32_t raw_memory_type =
            static_cast<std::uint32_t>(memory_type);
        pne::gcn8::VSharpBufferFields fields{};
        fields.gpu_address = reinterpret_cast<std::uintptr_t>(address);
        fields.stride = sizeof(float);
        fields.num_records = kElementCount;
        pne::gcn8::set_vsharp_memory_type(fields, raw_memory_type, false, false);
        fields.dstselx = static_cast<std::uint8_t>(format.chanx);
        fields.dstsely = static_cast<std::uint8_t>(format.chany);
        fields.dstselz = static_cast<std::uint8_t>(format.chanz);
        fields.dstselw = static_cast<std::uint8_t>(format.chanw);
        fields.numformat = static_cast<std::uint8_t>(format.chantype);
        fields.dataformat = static_cast<std::uint8_t>(format.surfacefmt);
        std::uint32_t words[4]{};
        const auto error = pne::gcn8::pack_vsharp_buffer(fields, words);
        if (error == pne::gcn8::VSharpCodecError::none)
            std::memcpy(descriptor, words, sizeof(words));
        return error;
    };

    GnmBuffer input_descriptor{};
    const auto input_descriptor_error =
        make_float_buffer(&input_descriptor, input, GNM_MEMORY_READONLY);
    if (input_descriptor_error != pne::gcn8::VSharpCodecError::none) {
        log.line("INPUT_VSHARP_CODEC_ERROR",
                 static_cast<std::uint32_t>(input_descriptor_error));
        log.message("RESULT=FAIL; REASON=INPUT_VSHARP_DESCRIPTOR_ENCODING");
        return 15;
    }
    GnmBuffer output_descriptor{};
    const auto output_descriptor_error =
        make_float_buffer(&output_descriptor, output, GNM_MEMORY_UNCACHED);
    if (output_descriptor_error != pne::gcn8::VSharpCodecError::none) {
        log.line("OUTPUT_VSHARP_CODEC_ERROR",
                 static_cast<std::uint32_t>(output_descriptor_error));
        log.message("RESULT=FAIL; REASON=OUTPUT_VSHARP_DESCRIPTOR_ENCODING");
        return 15;
    }

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

    log.message("STAGE=BEFORE_GNM_DEFAULT_STATE");
    const uint32_t initial_dwords =
        gnm.dispatch_init_default_hardware_state(dcb, kDcbDwords);
    if (initial_dwords == 0 || initial_dwords >= kDcbDwords) {
        log.message("RESULT=FAIL; REASON=DEFAULT_COMPUTE_STATE");
        return 18;
    }
    cmd.cmdptr = dcb + initial_dwords;

    uint32_t* before = cmd.cmdptr;
    const auto shader_bind_result = pne_gcn8_set_cs_shader(&cmd, &cs_registers);
    log.line("CS_SHADER_BIND_RESULT",
             static_cast<uint32_t>(shader_bind_result));
    if (shader_bind_result != PNE_GCN8_BINDING_OK ||
        cmd.cmdptr - before != 25) {
        log.message("RESULT=FAIL; REASON=CS_SHADER_BIND_PACKET");
        return 19;
    }

    const auto table_pointer_result =
        pne_gcn8_set_cs_indirect_resource_table_pointer(
            &cmd, 2, table_address, 80);
    log.line("RESOURCE_TABLE_POINTER_RESULT",
             static_cast<uint32_t>(table_pointer_result));
    if (table_pointer_result != PNE_GCN8_BINDING_OK) {
        log.message("RESULT=FAIL; REASON=RESOURCE_TABLE_POINTER");
        return 20;
    }

    before = cmd.cmdptr;
    const auto dispatch_result = pne_gcn8_dispatch_direct(&cmd, 1, 1, 1, 0);
    log.line("DISPATCH_PACKET_RESULT",
             static_cast<uint32_t>(dispatch_result));
    if (dispatch_result != PNE_GCN8_BINDING_OK ||
        cmd.cmdptr - before != 9) {
        log.message("RESULT=FAIL; REASON=DISPATCH_PACKET");
        return 21;
    }

    before = cmd.cmdptr;
    const auto fence_result =
        pne_gcn8_emit_eop_fence(&cmd, fence_address, kFenceMagic);
    log.line("EOP_FENCE_PACKET_RESULT", static_cast<uint32_t>(fence_result));
    if (fence_result != PNE_GCN8_BINDING_OK ||
        cmd.cmdptr - before != 6) {
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

    log.message("STAGE=BEFORE_GNM_SUBMIT");
    gnm.flush_garlic();
    void* dcb_addresses[1] = {dcb};
    uint32_t dcb_sizes[1] = {
        static_cast<uint32_t>((cmd.cmdptr - dcb) * sizeof(uint32_t))};
    log.message("STAGE=GNM_SUBMIT_CALL");
    const int32_t submit_result = gnm.submit_command_buffers(
        1, dcb_addresses, dcb_sizes, nullptr, nullptr);
    if (submit_result != 0) {
        log.line("SUBMIT_ERROR", static_cast<uint32_t>(submit_result));
        log.message("RESULT=FAIL; REASON=SUBMIT_COMMAND_BUFFERS");
        return 23;
    }
    stage_log("DISPATCH_SUBMITTED");
    const int submit_done = gnm.submit_done();
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

    stage_log("DISPATCH_DONE_FENCE_CONFIRMED");
    gnm.flush_garlic();
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
