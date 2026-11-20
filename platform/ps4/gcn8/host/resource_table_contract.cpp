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

        output->bindings[i] = {requirement.binding, offset, descriptor_bytes, requirement.array_count};
        previous_binding = requirement.binding;
        previous_end = end;
        have_previous = true;
    }
    output->binding_count = requirement_count;
    output->byte_size = requirement_count ? previous_end : 0;
    return ResourceTableError::none;
}

ResourceTableError validate_indirect_resource_table_address(std::uint64_t gpu_address) noexcept {
    if (gpu_address == 0 || (gpu_address & 3u) != 0 ||
        gpu_address > std::numeric_limits<std::uint32_t>::max())
        return ResourceTableError::invalid_table_address;
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
    case ResourceTableError::invalid_table_address: return "indirect table GPU address must be non-zero, 4-byte aligned and below 4 GiB";
    }
    return "unknown";
}

} // namespace pne::gcn8
