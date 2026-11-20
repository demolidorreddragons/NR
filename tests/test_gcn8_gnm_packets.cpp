#include <cassert>
#include <cstdint>

#include "gnm_resource_binding.h"
#include "pm4/sid.h"
#include "gnm_types.h"

int main() {
    alignas(256) uint32_t words[32]{};
    GnmCommandBuffer cmd{};
    cmd.beginptr = words;
    cmd.endptr = words + 32;
    cmd.cmdptr = words;
    cmd.sizedwords = 32;

    constexpr uint64_t table_address = 0x12340000ULL;
    assert(pne_gcn8_set_cs_indirect_resource_table_pointer(
               &cmd, 2, table_address, 80) == PNE_GCN8_BINDING_OK);
    assert(cmd.cmdptr - words == 3);
    assert(words[0] == (PKT3(PKT3_SET_SH_REG, 1, 0) | PKT3_SHADER_TYPE_S(1)));
    assert(words[1] == ((R_00B900_COMPUTE_USER_DATA_0 - SI_SH_REG_OFFSET) >> 2));
    assert(words[2] == static_cast<uint32_t>(table_address));

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
    assert(pne_gcn8_emit_eop_fence(&cmd, fence_address, magic) ==
           PNE_GCN8_BINDING_COMMAND_BUFFER_FULL);
    assert(cmd.cmdptr == words);

    cmd.endptr = words + 32;
    assert(pne_gcn8_emit_eop_fence(&cmd, fence_address + 1, magic) ==
           PNE_GCN8_BINDING_INVALID_ARGUMENT);
    assert(pne_gcn8_emit_eop_fence(&cmd, 0x0001000000000000ULL, magic) ==
           PNE_GCN8_BINDING_INVALID_ARGUMENT);
    return 0;
}
