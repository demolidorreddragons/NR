#include "resource_table_contract.h"

#include <cassert>
#include <cstdint>
#include <limits>

using namespace pne::gcn8;

int main() {
    const BindingRequirement gemm[] = {
        {0, 0, 1, DescriptorKind::buffer},
        {0, 1, 1, DescriptorKind::buffer},
        {0, 2, 1, DescriptorKind::buffer},
        {0, 3, 1, DescriptorKind::buffer},
    };
    ResourceTableLayout layout{};
    assert(build_resource_table_layout(gemm, 4, &layout) == ResourceTableError::none);
    assert(layout.binding_count == 4);
    assert(layout.byte_size == 208);
    assert(layout.bindings[0].byte_offset == 0);
    assert(layout.bindings[1].byte_offset == 64);
    assert(layout.bindings[2].byte_offset == 128);
    assert(layout.bindings[3].byte_offset == 192);
    for (const auto& binding : layout.bindings)
        assert(binding.descriptor_bytes == 16 && binding.array_count == 1);

    assert(descriptor_size_bytes(DescriptorKind::sampled_image) == 32);
    assert(descriptor_size_bytes(DescriptorKind::storage_image) == 32);
    assert(descriptor_size_bytes(DescriptorKind::sampler) == 16);
    assert(descriptor_size_bytes(DescriptorKind::combined_image_sampler) == 48);
    assert(descriptor_size_bytes(static_cast<DescriptorKind>(0xff)) == 0);

    const BindingRequirement last_array[] = {{0, 0, 4, DescriptorKind::buffer}};
    assert(build_resource_table_layout(last_array, 1, &layout) == ResourceTableError::none);
    assert(layout.byte_size == 64);

    const BindingRequirement overlap[] = {
        {0, 0, 5, DescriptorKind::buffer},
        {0, 1, 1, DescriptorKind::buffer},
    };
    assert(build_resource_table_layout(overlap, 2, &layout) == ResourceTableError::overlapping_binding);
    assert(layout.binding_count == 0 && layout.byte_size == 0);

    const BindingRequirement wrong_set[] = {{1, 0, 1, DescriptorKind::buffer}};
    assert(build_resource_table_layout(wrong_set, 1, &layout) == ResourceTableError::unsupported_descriptor_set);
    const BindingRequirement unsorted[] = {{0, 2, 1, DescriptorKind::buffer}, {0, 1, 1, DescriptorKind::buffer}};
    assert(build_resource_table_layout(unsorted, 2, &layout) == ResourceTableError::bindings_not_strictly_sorted);
    const BindingRequirement duplicate[] = {{0, 1, 1, DescriptorKind::buffer}, {0, 1, 1, DescriptorKind::buffer}};
    assert(build_resource_table_layout(duplicate, 2, &layout) == ResourceTableError::bindings_not_strictly_sorted);
    const BindingRequirement zero_array[] = {{0, 0, 0, DescriptorKind::buffer}};
    assert(build_resource_table_layout(zero_array, 1, &layout) == ResourceTableError::zero_array_count);
    const BindingRequirement invalid_kind[] = {{0, 0, 1, static_cast<DescriptorKind>(0xff)}};
    assert(build_resource_table_layout(invalid_kind, 1, &layout) == ResourceTableError::invalid_descriptor_kind);

    assert(build_resource_table_layout(nullptr, 1, &layout) == ResourceTableError::null_requirements);
    assert(build_resource_table_layout(nullptr, 0, &layout) == ResourceTableError::none);
    assert(layout.binding_count == 0 && layout.byte_size == 0);
    assert(build_resource_table_layout(gemm, 4, nullptr) == ResourceTableError::null_output);

    BindingRequirement too_many[kMaxResourceBindings + 1]{};
    assert(build_resource_table_layout(too_many, kMaxResourceBindings + 1, &layout) == ResourceTableError::too_many_bindings);

    const BindingRequirement offset_overflow[] = {{
        0, std::numeric_limits<std::uint32_t>::max() / kDescriptorBindingStride + 1,
        1, DescriptorKind::buffer
    }};
    assert(build_resource_table_layout(offset_overflow, 1, &layout) == ResourceTableError::offset_overflow);

    const BindingRequirement table_overflow[] = {{
        0, 1, std::numeric_limits<std::uint32_t>::max() / 16,
        DescriptorKind::buffer
    }};
    assert(build_resource_table_layout(table_overflow, 1, &layout) == ResourceTableError::table_size_overflow);

    assert(validate_indirect_resource_table_address(0x1000) == ResourceTableError::none);
    assert(validate_indirect_resource_table_address(0xFFFFFFFCULL) == ResourceTableError::none);
    assert(validate_indirect_resource_table_address(0x1002) == ResourceTableError::invalid_table_address);
    assert(validate_indirect_resource_table_address(0x100000000ULL) == ResourceTableError::invalid_table_address);
    assert(validate_indirect_resource_table_address(0) == ResourceTableError::invalid_table_address);
    return 0;
}
