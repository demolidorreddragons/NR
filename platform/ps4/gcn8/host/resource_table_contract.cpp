#include "resource_table_contract.h"

#include <limits>

namespace pne::gcn8 {

std::uint32_t descriptor_size_bytes(DescriptorKind kind) noexcept {
    switch (kind) {
    case DescriptorKind::buffer:
    case DescriptorKind::texel_buffer:
    case DescriptorKind::sampler:
        return 16;
    case DescriptorKind::sampled_image:
    case DescriptorKind::storage_image:
        return 32;
    case DescriptorKind::combined_image_sampler:
        return 48;
    }
    return 0;
}

ResourceTableError build_resource_table_layout(
    const BindingRequirement* requirements,
    std::size_t requirement_count,
    ResourceTableLayout* output) noexcept {
    if (!output) return ResourceTableError::null_output;
    *output = {};
    if (requirement_count != 0 && !requirements)
        return ResourceTableError::null_requirements;
    if (requirement_count > kMaxResourceBindings)
        return ResourceTableError::too_many_bindings;

    std::uint32_t previous_end = 0;
    std::uint32_t previous_binding = 0;
    bool have_previous = false;
    for (std::size_t i = 0; i < requirement_count; ++i) {
        const auto& requirement = requirements[i];
        if (requirement.descriptor_set != 0)
            return ResourceTableError::unsupported_descriptor_set;
        if (requirement.array_count == 0)
            return ResourceTableError::zero_array_count;
        if (have_previous && requirement.binding <= previous_binding)
            return ResourceTableError::bindings_not_strictly_sorted;

        const std::uint32_t descriptor_bytes = descriptor_size_bytes(requirement.kind);
        if (descriptor_bytes == 0)
            return ResourceTableError::invalid_descriptor_kind;
        if (requirement.binding > std::numeric_limits<std::uint32_t>::max() / kDescriptorBindingStride)
            return ResourceTableError::offset_overflow;
        if (requirement.array_count > std::numeric_limits<std::uint32_t>::max() / descriptor_bytes)
            return ResourceTableError::table_size_overflow;

        const std::uint32_t offset = requirement.binding * kDescriptorBindingStride;
        const std::uint32_t payload_bytes = descriptor_bytes * requirement.array_count;
        if (offset > std::numeric_limits<std::uint32_t>::max() - payload_bytes)
            return ResourceTableError::table_size_overflow;
        const std::uint32_t end = offset + payload_bytes;
        if (have_previous && previous_end > offset)
            return ResourceTableError::overlapping_binding;

        auto& binding = output->bindings[i];
        binding.binding = requirement.binding;
        binding.byte_offset = offset;
        binding.descriptor_bytes = descriptor_bytes;
        binding.array_count = requirement.array_count;
        previous_binding = requirement.binding;
        previous_end = end;
        have_previous = true;
    }
    output->binding_count = requirement_count;
    output->byte_size = requirement_count ? previous_end : 0;
    return ResourceTableError::none;
}

ResourceTableError validate_indirect_resource_table_range(
    std::uint64_t gpu_address, std::uint32_t byte_size) noexcept {
    constexpr std::uint64_t address_space_end = std::uint64_t{1} << 32;
    if (gpu_address == 0 || (gpu_address & 3u) != 0 ||
        gpu_address > std::numeric_limits<std::uint32_t>::max() ||
        byte_size == 0 || byte_size > address_space_end - gpu_address)
        return ResourceTableError::invalid_table_address;
    return ResourceTableError::none;
}

ResourceTableError build_cs_indirect_table_pointer_packet(
    std::uint32_t start_register,
    std::uint64_t gpu_address,
    std::uint32_t byte_size,
    std::uint32_t packet_dwords[4]) noexcept {
    if (!packet_dwords) return ResourceTableError::null_output;
    packet_dwords[0] = packet_dwords[1] = packet_dwords[2] = packet_dwords[3] = 0;
    if (start_register > 14u)
        return ResourceTableError::invalid_user_data_slot;
    const auto range_error = validate_indirect_resource_table_range(gpu_address, byte_size);
    if (range_error != ResourceTableError::none)
        return range_error;

    constexpr std::uint32_t packet3_set_sh_reg_compute_2_dwords = 0xC0027602u;
    constexpr std::uint32_t sh_register_offset = 0x0000B000u;
    constexpr std::uint32_t compute_user_data_0 = 0x0000B900u;
    constexpr std::uint32_t compute_user_data_15 = 0x0000B93Cu;
    const std::uint32_t register_address =
        compute_user_data_0 + start_register * sizeof(std::uint32_t);
    if (register_address + sizeof(std::uint32_t) > compute_user_data_15 ||
        register_address < sh_register_offset)
        return ResourceTableError::invalid_user_data_slot;

    packet_dwords[0] = packet3_set_sh_reg_compute_2_dwords;
    packet_dwords[1] = (register_address - sh_register_offset) >> 2;
    packet_dwords[2] = static_cast<std::uint32_t>(gpu_address);
    packet_dwords[3] = 0; // PSBC address32_hi is zero.
    return ResourceTableError::none;
}

const char* resource_table_error_string(ResourceTableError error) noexcept {
    switch (error) {
    case ResourceTableError::none: return "none";
    case ResourceTableError::null_requirements: return "null requirements";
    case ResourceTableError::null_output: return "null output";
    case ResourceTableError::too_many_bindings: return "too many bindings";
    case ResourceTableError::unsupported_descriptor_set: return "only descriptor set 0 is supported";
    case ResourceTableError::zero_array_count: return "descriptor array count must be non-zero";
    case ResourceTableError::invalid_descriptor_kind: return "unsupported descriptor kind";
    case ResourceTableError::bindings_not_strictly_sorted: return "bindings must be strictly sorted";
    case ResourceTableError::offset_overflow: return "descriptor binding offset overflow";
    case ResourceTableError::overlapping_binding: return "descriptor binding overlaps previous binding";
    case ResourceTableError::table_size_overflow: return "descriptor table size overflow";
    case ResourceTableError::invalid_table_address: return "indirect table range must be non-zero, 4-byte aligned and entirely below 4 GiB";
    case ResourceTableError::invalid_user_data_slot: return "two-dword pointer exceeds CS user-data register range";
    }
    return "unknown";
}

} // namespace pne::gcn8
