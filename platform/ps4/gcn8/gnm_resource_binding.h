#ifndef PNE_GCN8_GNM_RESOURCE_BINDING_H
#define PNE_GCN8_GNM_RESOURCE_BINDING_H

#include <cstdint>

#include "gnm_commandbuffer.h"
#include "gnm_shader.h"

enum : std::int32_t {
    PNE_GCN8_BINDING_OK = 0,
    PNE_GCN8_BINDING_INVALID_ARGUMENT = -1,
    PNE_GCN8_BINDING_INVALID_SLOT = -2,
    PNE_GCN8_BINDING_INVALID_TABLE_RANGE = -3,
    PNE_GCN8_BINDING_COMMAND_BUFFER_FULL = -4,
    PNE_GCN8_BINDING_INVALID_REGISTER_RANGE = -5
};

// Emits a compute-engine SET_SH_REG packet for the 32-bit indirect resource
// table pointer used by the pinned PSBC GFX8 shader ABI.
extern "C" std::int32_t pne_gcn8_set_cs_indirect_resource_table_pointer(
    GnmCommandBuffer* command_buffer,
    std::uint32_t start_register,
    std::uint64_t table_gpu_address,
    std::uint32_t table_bytes);

// Emits the compute shader register setup sequence (GFX8/MEC) without relying
// on OpenGNM's optional high-level wrapper library.
extern "C" std::int32_t pne_gcn8_set_cs_shader(
    GnmCommandBuffer* command_buffer,
    const GnmCsStageRegisters* registers);

// Emits a compute-engine DISPATCH_DIRECT packet and trailing NOP padding.
extern "C" std::int32_t pne_gcn8_dispatch_direct(
    GnmCommandBuffer* command_buffer,
    std::uint32_t groups_x,
    std::uint32_t groups_y,
    std::uint32_t groups_z,
    std::uint32_t flags);

// Emits a cache-flush-and-invalidate EOP fence with a 32-bit completion value.
extern "C" std::int32_t pne_gcn8_emit_eop_fence(
    GnmCommandBuffer* command_buffer,
    std::uint64_t fence_gpu_address,
    std::uint32_t completion_value);

#endif
