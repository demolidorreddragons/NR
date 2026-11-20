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
}


extern "C" std::int32_t pne_gcn8_set_cs_shader(
    GnmCommandBuffer* command_buffer,
    const GnmCsStageRegisters* registers) {
    if (!command_buffer || !command_buffer->cmdptr ||
        !command_buffer->endptr || !registers ||
        command_buffer->cmdptr > command_buffer->endptr ||
        registers->computepgmhi != 0 ||
        registers->computenumthreadx == 0 ||
        registers->computenumthready == 0 ||
        registers->computenumthreadz == 0)
        return PNE_GCN8_BINDING_INVALID_ARGUMENT;

    // Match the pinned OpenGNM drawcommandbuffer.c sequence:
    // COMPUTE_PGM_LO/HI, COMPUTE_PGM_RSRC1/2, COMPUTE_NUM_THREAD_X/Y/Z,
    // then the required 11-dword trailing NOP payload.
    constexpr std::uint32_t kShRegisterOffset = 0x0000B000u;
    constexpr std::uint32_t kComputePgmLo = 0x0000B830u;
    constexpr std::uint32_t kComputePgmRsrc1 = 0x0000B848u;
    constexpr std::uint32_t kComputeNumThreadX = 0x0000B81Cu;
    constexpr std::int32_t kPacketDwords = 25;
    if (command_buffer->endptr - command_buffer->cmdptr < kPacketDwords)
        return PNE_GCN8_BINDING_COMMAND_BUFFER_FULL;

    std::uint32_t* p = command_buffer->cmdptr;
    std::size_t at = 0;
    const auto write_set_sh_reg = [&](std::uint32_t register_address,
                                      const std::uint32_t* values,
                                      std::uint32_t count) {
        p[at++] = PKT3(PKT3_SET_SH_REG, count, 0) | PKT3_SHADER_TYPE_S(1);
        p[at++] = (register_address - kShRegisterOffset) >> 2;
        for (std::uint32_t i = 0; i < count; ++i) p[at++] = values[i];
    };

    const std::uint32_t pgm[2] = {registers->computepgmlo, 0};
    const std::uint32_t resources[2] = {
        registers->computepgmrsrc1, registers->computepgmrsrc2};
    const std::uint32_t threads[3] = {
        registers->computenumthreadx, registers->computenumthready,
        registers->computenumthreadz};
    write_set_sh_reg(kComputePgmLo, pgm, 2);
    write_set_sh_reg(kComputePgmRsrc1, resources, 2);
    write_set_sh_reg(kComputeNumThreadX, threads, 3);
    p[at++] = PKT3(PKT3_NOP, 10, 0);
    for (std::uint32_t i = 0; i < 11; ++i) p[at++] = 0;

    if (at != static_cast<std::size_t>(kPacketDwords))
        return PNE_GCN8_BINDING_INVALID_ARGUMENT;
    command_buffer->cmdptr += at;
    return PNE_GCN8_BINDING_OK;
}

extern "C" std::int32_t pne_gcn8_dispatch_direct(
    GnmCommandBuffer* command_buffer,
    std::uint32_t groups_x,
    std::uint32_t groups_y,
    std::uint32_t groups_z,
    std::uint32_t flags) {
    if (!command_buffer || !command_buffer->cmdptr ||
        !command_buffer->endptr ||
        command_buffer->cmdptr > command_buffer->endptr ||
        static_cast<std::int32_t>(groups_x | groups_y | groups_z) < 0)
        return PNE_GCN8_BINDING_INVALID_ARGUMENT;
    if (command_buffer->endptr - command_buffer->cmdptr < 9)
        return PNE_GCN8_BINDING_COMMAND_BUFFER_FULL;

    std::uint32_t* p = command_buffer->cmdptr;
    p[0] = PKT3(PKT3_DISPATCH_DIRECT, 4, (flags & 1u)) |
           PKT3_SHADER_TYPE_S(1);
    p[1] = groups_x;
    p[2] = groups_y;
    p[3] = groups_z;
    p[4] = (flags & 0x18u) + 1u;
    p[5] = PKT3(PKT3_NOP, 2, 0);
    p[6] = 0;
    p[7] = 0;
    p[8] = 0;
    command_buffer->cmdptr += 9;
    return PNE_GCN8_BINDING_OK;
}

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
