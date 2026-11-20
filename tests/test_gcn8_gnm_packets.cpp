#include <cassert>
#include <cstdint>

#include "gnm_resource_binding.h"
#include "pm4/sid.h"
#include "gnm_types.h"

int main() {
    alignas(256) uint32_t words[64]{};
    GnmCommandBuffer cmd{};
    cmd.beginptr = words;
    cmd.endptr = words + 64;
    cmd.cmdptr = words;
    cmd.sizedwords = 64;

    constexpr uint64_t table_address = 0x12340000ULL;
    assert(pne_gcn8_set_cs_indirect_resource_table_pointer(
               &cmd, 2, table_address, 80) == PNE_GCN8_BINDING_OK);
    assert(cmd.cmdptr - words == 3);
    assert(words[0] == (PKT3(PKT3_SET_SH_REG, 1, 0) | PKT3_SHADER_TYPE_S(1)));
    // The PSBC usage slot starts at SGPR2, not SGPR0.
    assert(words[1] ==
           ((R_00B900_COMPUTE_USER_DATA_0 + 2u * 4u - SI_SH_REG_OFFSET) >> 2));
    assert(words[2] == static_cast<uint32_t>(table_address));

    GnmCsStageRegisters cs{};
    cs.computepgmlo = 0x12345u;
    cs.computepgmhi = 0;
    cs.computepgmrsrc1 = 0x11u;
    cs.computepgmrsrc2 = 0x22u;
    cs.computenumthreadx = 64;
    cs.computenumthready = 1;
    cs.computenumthreadz = 1;
    uint32_t* shader_packet = cmd.cmdptr;
    assert(pne_gcn8_set_cs_shader(&cmd, &cs) == PNE_GCN8_BINDING_OK);
    assert(cmd.cmdptr - shader_packet == 25);
    assert(shader_packet[0] ==
           (PKT3(PKT3_SET_SH_REG, 2, 0) | PKT3_SHADER_TYPE_S(1)));
    assert(shader_packet[1] == ((0xB830u - SI_SH_REG_OFFSET) >> 2));
    assert(shader_packet[2] == cs.computepgmlo && shader_packet[3] == 0);
    assert(shader_packet[4] ==
           (PKT3(PKT3_SET_SH_REG, 2, 0) | PKT3_SHADER_TYPE_S(1)));
    assert(shader_packet[5] == ((0xB848u - SI_SH_REG_OFFSET) >> 2));
    assert(shader_packet[6] == cs.computepgmrsrc1);
    assert(shader_packet[7] == cs.computepgmrsrc2);
    assert(shader_packet[8] ==
           (PKT3(PKT3_SET_SH_REG, 3, 0) | PKT3_SHADER_TYPE_S(1)));
    assert(shader_packet[9] == ((0xB81Cu - SI_SH_REG_OFFSET) >> 2));
    assert(shader_packet[10] == 64 && shader_packet[11] == 1 &&
           shader_packet[12] == 1);
    assert(shader_packet[13] == PKT3(PKT3_NOP, 10, 0));
    for (uint32_t i = 14; i < 25; ++i) assert(shader_packet[i] == 0);

    uint32_t* dispatch = cmd.cmdptr;
    assert(pne_gcn8_dispatch_direct(&cmd, 2, 3, 4, 0) ==
           PNE_GCN8_BINDING_OK);
    assert(cmd.cmdptr - dispatch == 9);
    assert(dispatch[0] ==
           (PKT3(PKT3_DISPATCH_DIRECT, 4, 0) | PKT3_SHADER_TYPE_S(1)));
    assert(dispatch[1] == 2 && dispatch[2] == 3 && dispatch[3] == 4);
    assert(dispatch[4] == 1);
    assert(dispatch[5] == PKT3(PKT3_NOP, 2, 0));
    assert(dispatch[6] == 0 && dispatch[7] == 0 && dispatch[8] == 0);

    constexpr uint64_t fence_address = 0x12345000ULL;
    constexpr uint32_t magic = 0x4E52434Du;
    uint32_t* eop = cmd.cmdptr;
    assert(pne_gcn8_emit_eop_fence(&cmd, fence_address, magic) ==
           PNE_GCN8_BINDING_OK);
    assert(cmd.cmdptr - eop == 6);
    assert(eop[0] == PKT3(PKT3_EVENT_WRITE_EOP, 4, 0));
    assert(eop[1] ==
           (EVENT_TYPE(GNM_CACHE_FLUSH_AND_INV_TS_EVENT) | EVENT_INDEX(5)));
    assert(eop[2] == 0x12345000u);
    assert(eop[3] ==
           (EOP_DST_SEL(EOP_DST_SEL_MEM) |
            EOP_DATA_SEL(GNM_DATA_SEL_SEND_DATA32) |
            EOP_INT_SEL(EOP_INT_SEL_SEND_DATA_AFTER_WR_CONFIRM)));
    assert(eop[4] == magic && eop[5] == 0);

    cmd.cmdptr = words;
    cmd.endptr = words + 5;
    assert(pne_gcn8_set_cs_shader(&cmd, &cs) ==
           PNE_GCN8_BINDING_COMMAND_BUFFER_FULL);
    assert(cmd.cmdptr == words);
    assert(pne_gcn8_dispatch_direct(&cmd, 1, 1, 1, 0) ==
           PNE_GCN8_BINDING_COMMAND_BUFFER_FULL);
    assert(cmd.cmdptr == words);
    assert(pne_gcn8_emit_eop_fence(&cmd, fence_address, magic) ==
           PNE_GCN8_BINDING_COMMAND_BUFFER_FULL);
    assert(cmd.cmdptr == words);

    cmd.endptr = words + 64;
    cs.computepgmhi = 1;
    assert(pne_gcn8_set_cs_shader(&cmd, &cs) ==
           PNE_GCN8_BINDING_INVALID_ARGUMENT);
    assert(pne_gcn8_emit_eop_fence(&cmd, fence_address + 1, magic) ==
           PNE_GCN8_BINDING_INVALID_ARGUMENT);
    assert(pne_gcn8_emit_eop_fence(&cmd, 0x0001000000000000ULL, magic) ==
           PNE_GCN8_BINDING_INVALID_ARGUMENT);
    return 0;
}
