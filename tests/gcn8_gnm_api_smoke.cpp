#include <cstdint>
#include <type_traits>

#include "gnmdriver.h"
#include <orbis/libkernel.h>
#include "gnm_buffer.h"
#include "gnm_commandbuffer.h"
#include "gnm_drawcommandbuffer.h"
#include "gnm_shaderbinary.h"
#include "../platform/ps4/gcn8/gnm_resource_binding.h"

using GnmDispatchDirectSignature =
    int32_t (PS4_SYSV_ABI *)(uint32_t*, uint32_t, uint32_t, uint32_t,
                            uint32_t, uint32_t);
using GnmSetCsShaderSignature =
    int32_t (PS4_SYSV_ABI *)(uint32_t*, uint32_t,
                            const uint32_t*);
using GnmSubmitCommandBuffersSignature =
    int32_t (PS4_SYSV_ABI *)(uint32_t, void* const[], uint32_t*,
                            void* const[], uint32_t*);
using GnmSubmitDoneSignature = int (PS4_SYSV_ABI *)(void);
using GnmDispatchInitSignature = uint32_t (PS4_SYSV_ABI *)(uint32_t*, uint32_t);
using GnmFlushGarlicSignature = void (PS4_SYSV_ABI *)(void);
using KernelGetDirectMemorySizeSignature = size_t (*)();
using KernelMunmapSignature = int32_t (*)(void*, size_t);
using KernelUsleepSignature = int32_t (*)(uint32_t);

using GnmCmdDispatchSignature =
    void (*)(GnmCommandBuffer*, uint32_t, uint32_t, uint32_t, uint32_t);
using GnmCmdSetCsShaderSignature =
    void (*)(GnmCommandBuffer*, const GnmCsStageRegisters*);
using GnmCmdSetVsharpSignature =
    void (*)(GnmCommandBuffer*, GnmShaderStage, uint32_t,
             const GnmBuffer*);

using KernelAllocateDirectMemorySignature =
    int32_t (*)(off_t, off_t, size_t, size_t, int32_t, off_t*);
using KernelMapDirectMemorySignature =
    int32_t (*)(void**, size_t, int32_t, int32_t, off_t, size_t);
using KernelReleaseDirectMemorySignature = int32_t (*)(off_t, size_t);

static_assert(std::is_same<decltype(&sceKernelAllocateDirectMemory),
                           KernelAllocateDirectMemorySignature>::value,
              "unexpected sceKernelAllocateDirectMemory signature");
static_assert(std::is_same<decltype(&sceKernelMapDirectMemory),
                           KernelMapDirectMemorySignature>::value,
              "unexpected sceKernelMapDirectMemory signature");
static_assert(std::is_same<decltype(&sceKernelReleaseDirectMemory),
                           KernelReleaseDirectMemorySignature>::value,
              "unexpected sceKernelReleaseDirectMemory signature");

static_assert(sizeof(GnmBuffer) == 16, "GnmBuffer ABI changed");
static_assert(sizeof(GnmInputUsageSlot) == 4,
              "input-usage slot ABI changed");
static_assert(GNM_SHINPUTUSAGE_PTR_INDIRECTRESOURCETABLE == 0x1c,
              "indirect resource table usage enum changed");
static_assert(sizeof(GnmCommandBuffer) == 64, "GnmCommandBuffer ABI changed");
static_assert(sizeof(GnmCsStageRegisters) == 28,
              "compute shader register ABI changed");
static_assert(sizeof(GnmCsShader) == 36, "compute shader container ABI changed");
static_assert(std::is_same<decltype(&sceGnmDispatchDirect),
                           GnmDispatchDirectSignature>::value,
              "unexpected sceGnmDispatchDirect signature");
static_assert(std::is_same<decltype(&sceGnmSetCsShader),
                           GnmSetCsShaderSignature>::value,
              "unexpected sceGnmSetCsShader signature");
static_assert(std::is_same<decltype(&sceGnmSubmitCommandBuffers),
                           GnmSubmitCommandBuffersSignature>::value,
              "unexpected sceGnmSubmitCommandBuffers signature");
static_assert(std::is_same<decltype(&sceGnmDispatchInitDefaultHardwareState),
                           GnmDispatchInitSignature>::value,
              "unexpected dispatch default-state signature");
static_assert(std::is_same<decltype(&sceGnmFlushGarlic),
                           GnmFlushGarlicSignature>::value,
              "unexpected Garlic flush signature");
static_assert(std::is_same<decltype(&sceKernelGetDirectMemorySize),
                           KernelGetDirectMemorySizeSignature>::value,
              "unexpected direct-memory-size signature");
static_assert(std::is_same<decltype(&sceKernelMunmap),
                           KernelMunmapSignature>::value,
              "unexpected direct-memory unmap signature");
static_assert(std::is_same<decltype(&sceKernelUsleep),
                           KernelUsleepSignature>::value,
              "unexpected kernel sleep signature");
static_assert(std::is_same<decltype(&sceGnmSubmitDone),
                           GnmSubmitDoneSignature>::value,
              "unexpected sceGnmSubmitDone signature");
static_assert(std::is_same<decltype(&sceGnmDrawCmdDispatchDirect),
                           GnmCmdDispatchSignature>::value,
              "unexpected command-buffer dispatch signature");
static_assert(std::is_same<decltype(&sceGnmDrawCmdSetCsShader),
                           GnmCmdSetCsShaderSignature>::value,
              "unexpected command-buffer CS binding signature");
static_assert(std::is_same<decltype(&sceGnmDrawCmdSetVsharpUserData),
                           GnmCmdSetVsharpSignature>::value,
              "unexpected buffer descriptor binding signature");
using PneGcn8SetResourceTablePointerSignature =
    int32_t (*)(GnmCommandBuffer*, uint32_t, uint64_t, uint32_t);
using PneGcn8EopFenceSignature =
    int32_t (*)(GnmCommandBuffer*, uint64_t, uint32_t);
static_assert(std::is_same<
                  decltype(&pne_gcn8_set_cs_indirect_resource_table_pointer),
                  PneGcn8SetResourceTablePointerSignature>::value,
              "unexpected NR compute resource-table pointer helper signature");
static_assert(std::is_same<decltype(&pne_gcn8_emit_eop_fence),
                           PneGcn8EopFenceSignature>::value,
              "unexpected NR compute EOP fence helper signature");

// Link references force the PS4 import library to resolve the firmware NIDs.
// These pointers are never invoked by the smoke executable.
extern "C" {
__attribute__((used)) GnmDispatchDirectSignature pne_gcn8_dispatch_direct_import =
    &sceGnmDispatchDirect;
__attribute__((used)) GnmSetCsShaderSignature pne_gcn8_set_cs_shader_import =
    &sceGnmSetCsShader;
__attribute__((used)) GnmSubmitCommandBuffersSignature
    pne_gcn8_submit_command_buffers_import = &sceGnmSubmitCommandBuffers;
__attribute__((used)) GnmSubmitDoneSignature
    pne_gcn8_submit_done_import = &sceGnmSubmitDone;
__attribute__((used)) GnmDispatchInitSignature
    pne_gcn8_dispatch_init_import = &sceGnmDispatchInitDefaultHardwareState;
__attribute__((used)) GnmFlushGarlicSignature
    pne_gcn8_flush_garlic_import = &sceGnmFlushGarlic;
__attribute__((used)) KernelGetDirectMemorySizeSignature
    pne_gcn8_direct_memory_size_import = &sceKernelGetDirectMemorySize;
__attribute__((used)) KernelMunmapSignature
    pne_gcn8_munmap_import = &sceKernelMunmap;
__attribute__((used)) KernelUsleepSignature
    pne_gcn8_usleep_import = &sceKernelUsleep;
__attribute__((used)) PneGcn8SetResourceTablePointerSignature
    pne_gcn8_set_indirect_resource_table_packet =
        &pne_gcn8_set_cs_indirect_resource_table_pointer;
__attribute__((used)) PneGcn8EopFenceSignature
    pne_gcn8_emit_eop_fence_packet = &pne_gcn8_emit_eop_fence;
__attribute__((used)) KernelAllocateDirectMemorySignature
    pne_gcn8_allocate_direct_memory_import = &sceKernelAllocateDirectMemory;
__attribute__((used)) KernelMapDirectMemorySignature
    pne_gcn8_map_direct_memory_import = &sceKernelMapDirectMemory;
__attribute__((used)) KernelReleaseDirectMemorySignature
    pne_gcn8_release_direct_memory_import = &sceKernelReleaseDirectMemory;
}

// Compile/link-only GNM and direct-memory import smoke test. No firmware call is executed.
int pne_gcn8_gnm_api_smoke() { return 0; }

// The pinned GoldHEN CRT's _init/_fini entry points require these module
// callbacks. Keep this link-only probe self-contained; neither callback is
// executed, and they deliberately perform no firmware or GPU operations.
extern "C" int module_start(std::size_t, const void*) { return 0; }
extern "C" int module_stop(std::size_t, const void*) { return 0; }
