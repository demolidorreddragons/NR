#include <cstdint>
#include <type_traits>

#include "gnmdriver.h"
#include "gnm_buffer.h"
#include "gnm_commandbuffer.h"
#include "gnm_drawcommandbuffer.h"
#include "gnm_shaderbinary.h"

using GnmDispatchDirectSignature =
    int32_t (PS4_SYSV_ABI *)(uint32_t*, uint32_t, uint32_t, uint32_t,
                            uint32_t, uint32_t);
using GnmSetCsShaderSignature =
    int32_t (PS4_SYSV_ABI *)(uint32_t*, uint32_t,
                            const uint32_t*);
using GnmCmdDispatchSignature =
    void (*)(GnmCommandBuffer*, uint32_t, uint32_t, uint32_t, uint32_t);
using GnmCmdSetCsShaderSignature =
    void (*)(GnmCommandBuffer*, const GnmCsStageRegisters*);
using GnmCmdSetVsharpSignature =
    void (*)(GnmCommandBuffer*, GnmShaderStage, uint32_t,
             const GnmBuffer*);

static_assert(sizeof(GnmBuffer) == 16, "GnmBuffer ABI changed");
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
static_assert(std::is_same<decltype(&sceGnmDrawCmdDispatchDirect),
                           GnmCmdDispatchSignature>::value,
              "unexpected command-buffer dispatch signature");
static_assert(std::is_same<decltype(&sceGnmDrawCmdSetCsShader),
                           GnmCmdSetCsShaderSignature>::value,
              "unexpected command-buffer CS binding signature");
static_assert(std::is_same<decltype(&sceGnmDrawCmdSetVsharpUserData),
                           GnmCmdSetVsharpSignature>::value,
              "unexpected buffer descriptor binding signature");

// Link references force the PS4 import library to resolve the firmware NIDs.
// These pointers are never invoked by the smoke executable.
extern "C" {
__attribute__((used)) GnmDispatchDirectSignature pne_gcn8_dispatch_direct_import =
    &sceGnmDispatchDirect;
__attribute__((used)) GnmSetCsShaderSignature pne_gcn8_set_cs_shader_import =
    &sceGnmSetCsShader;
}

// Compile/link-only API/ABI smoke test. No GNM call is executed.
int pne_gcn8_gnm_api_smoke() { return 0; }
