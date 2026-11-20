#include "gnm_resource_binding.h"

#include "pm4/pm4_ps4.h"
#include "pm4/sid.h"

extern "C" std::int32_t pne_gcn8_set_cs_indirect_resource_table_pointer(
    GnmCommandBuffer* command_buffer,
    std::uint32_t start_register,
    std::uint64_t table_gpu_address,
    std::uint32_t table_bytes) {
    if (!command_buffer || !command_buffer->cmdptr || !command_buffer->endptr)
        return PNE_GCN8_BINDING_INVALID_ARGUMENT;

    // A pointer occupies two consecutive CS user SGPRs (low/high dwords).
    constexpr std::uint32_t cs_user_data_first =
        R_00B900_COMPUTE_USER_DATA_0;
    constexpr std::uint32_t cs_user_data_last =
        R_00B93C_COMPUTE_USER_DATA_15;
    constexpr std::uint32_t cs_user_data_count =
        ((cs_user_data_last - cs_user_data_first) / 4u) + 1u;
    if (start_register >= cs_user_data_count - 1u)
        return PNE_GCN8_BINDING_INVALID_SLOT;

    constexpr std::uint64_t address_space_end = std::uint64_t{1} << 32;
    if (table_gpu_address == 0 || (table_gpu_address & 3u) != 0 ||
        table_gpu_address > UINT32_MAX || table_bytes == 0 ||
        table_bytes > address_space_end - table_gpu_address)
        return PNE_GCN8_BINDING_INVALID_TABLE_RANGE;

    if (command_buffer->endptr < command_buffer->cmdptr ||
        static_cast<std::uint32_t>(command_buffer->endptr -
                                   command_buffer->cmdptr) < 4u)
        return PNE_GCN8_BINDING_COMMAND_BUFFER_FULL;

    const std::uint32_t register_address =
        cs_user_data_first + (start_register * sizeof(std::uint32_t));
    if (register_address < SI_SH_REG_OFFSET ||
        register_address + sizeof(std::uint32_t) > cs_user_data_last)
        return PNE_GCN8_BINDING_INVALID_REGISTER_RANGE;

    // SET_SH_REG with SHADER_TYPE=1 selects the compute engine (MEC).
    command_buffer->cmdptr[0] =
        PKT3(PKT3_SET_SH_REG, 2, 0) | PKT3_SHADER_TYPE_S(1);
    command_buffer->cmdptr[1] =
        (register_address - SI_SH_REG_OFFSET) >> 2;
    command_buffer->cmdptr[2] = static_cast<std::uint32_t>(table_gpu_address);
    command_buffer->cmdptr[3] = 0; // PSBC emits address32_hi = 0.
    command_buffer->cmdptr += 4;
    return PNE_GCN8_BINDING_OK;
}
