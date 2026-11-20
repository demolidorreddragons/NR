#include "gnm_resource_binding.h"

#include <cstring>

#include "host/resource_table_contract.h"
#include "pm4/pm4_ps4.h"
#include "pm4/sid.h"

static_assert((PKT3(PKT3_SET_SH_REG, 1, 0) | PKT3_SHADER_TYPE_S(1)) == 0xC0017602u,
              "PM4 compute SET_SH_REG packet encoding changed");
static_assert(R_00B900_COMPUTE_USER_DATA_0 == 0x0000B900u,
              "GFX8 compute user-data base register changed");
static_assert(R_00B93C_COMPUTE_USER_DATA_15 == 0x0000B93Cu,
              "GFX8 compute user-data end register changed");
static_assert(SI_SH_REG_OFFSET == 0x0000B000u,
              "SH register offset changed");

extern "C" std::int32_t pne_gcn8_set_cs_indirect_resource_table_pointer(
    GnmCommandBuffer* command_buffer,
    std::uint32_t start_register,
    std::uint64_t table_gpu_address,
    std::uint32_t table_bytes) {
    if (!command_buffer || !command_buffer->cmdptr || !command_buffer->endptr)
        return PNE_GCN8_BINDING_INVALID_ARGUMENT;

    // PSBC emits registercount=0/address32_hi=0: write only SGPR2; SGPR3 is live scalar data.
    std::uint32_t packet[3]{};
    const auto packet_error = pne::gcn8::build_cs_indirect_table_pointer_packet(
        start_register, table_gpu_address, table_bytes, packet);
    if (packet_error == pne::gcn8::ResourceTableError::invalid_user_data_slot)
        return PNE_GCN8_BINDING_INVALID_SLOT;
    if (packet_error == pne::gcn8::ResourceTableError::invalid_table_address)
        return PNE_GCN8_BINDING_INVALID_TABLE_RANGE;
    if (packet_error != pne::gcn8::ResourceTableError::none)
        return PNE_GCN8_BINDING_INVALID_ARGUMENT;

    if (command_buffer->endptr < command_buffer->cmdptr ||
        static_cast<std::uint32_t>(command_buffer->endptr -
                                   command_buffer->cmdptr) < 3u)
        return PNE_GCN8_BINDING_COMMAND_BUFFER_FULL;

    std::memcpy(command_buffer->cmdptr, packet, sizeof(packet));
    command_buffer->cmdptr += 3;
    return PNE_GCN8_BINDING_OK;
 
extern "C" std::int32_t pne_gcn8_emit_eop_fence(
    GnmCommandBuffer* command_buffer,
    std::uint64_t fence_gpu_address,
    std::uint32_t completion_value) {
    if (!command_buffer || !command_buffer->cmdptr ||
        !command_buffer->endptr || fence_gpu_address == 0 ||
        (fence_gpu_address & 3u) != 0 ||
        fence_gpu_address >= (std::uint64_t{1} << 48) ||
        command_buffer->cmdptr > command_buffer->endptr) {
        return PNE_GCN8_BINDING_INVALID_ARGUMENT;
    }
    if (command_buffer->endptr - command_buffer->cmdptr < 6)
        return PNE_GCN8_BINDING_COMMAND_BUFFER_FULL;

    std::uint32_t* p = command_buffer->cmdptr;
    p[0] = PKT3(PKT3_EVENT_WRITE_EOP, 4, 0);
    p[1] = EVENT_TYPE(GNM_CACHE_FLUSH_AND_INV_TS_EVENT) | EVENT_INDEX(5);
    p[2] = static_cast<std::uint32_t>(fence_gpu_address);
    p[3] = static_cast<std::uint32_t>((fence_gpu_address >> 32) & 0xFFFFu) |
           EOP_DST_SEL(EOP_DST_SEL_MEM) |
           EOP_DATA_SEL(GNM_DATA_SEL_SEND_DATA32) |
           EOP_INT_SEL(EOP_INT_SEL_SEND_DATA_AFTER_WR_CONFIRM);
    p[4] = completion_value;
    p[5] = 0;
    command_buffer->cmdptr += 6;
    return PNE_GCN8_BINDING_OK;
}
}
