#include "resource_table_contract.h"

#include <cassert>
#include <cstdint>

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
    for (std::size_t i = 0; i < layout.binding_count; ++i)
        assert(layout.bindings[i].descriptor_bytes == 16 && layout.bindings[i].array_count == 1);

    assert(descriptor_size_bytes(DescriptorKind::sampled_image) == 32);
    assert(descriptor_size_bytes(DescriptorKind::storage_image) == 32);
    assert(descriptor_size_bytes(DescriptorKind::sampler) == 16);
    assert(descriptor_size_bytes(DescriptorKind::combined_image_sampler) == 48);

    const BindingRequirement last_array[] = {{0, 0, 5, DescriptorKind::buffer}};
    assert(build_resource_table_layout(last_array, 1, &layout) == ResourceTableError::none);
    assert(layout.byte_size == 80);

    const BindingRequirement overlap[] = {
        {0, 0, 5, DescriptorKind::buffer},
        {0, 1, 1, DescriptorKind::buffer},
    };
    assert(build_resource_table_layout(overlap, 2, &layout) == ResourceTableError::overlapping_binding);
    assert(layout.binding_count == 0 && layout.byte_size == 0);

    const BindingRequirement wrong_set[] = {{1, 0, 1, DescriptorKind::buffer}};
    assert(build_resource_table_layout(wrong_set, 1, &layout) == ResourceTableError::unsupported_descriptor_set);
    const BindingRequirement unsorted[] = {
        {0, 2, 1, DescriptorKind::buffer},
        {0, 1, 1, DescriptorKind::buffer}
    };
    assert(build_resource_table_layout(unsorted, 2, &layout) == ResourceTableError::bindings_not_strictly_sorted);
    const BindingRequirement zero_array[] = {{0, 0, 0, DescriptorKind::buffer}};
    assert(build_resource_table_layout(zero_array, 1, &layout) == ResourceTableError::zero_array_count);
    assert(build_resource_table_layout(nullptr, 1, &layout) == ResourceTableError::null_requirements);
    assert(build_resource_table_layout(gemm, 4, nullptr) == ResourceTableError::null_output);

    assert(validate_indirect_resource_table_range(0x1000, 208) == ResourceTableError::none);
    assert(validate_indirect_resource_table_range(0x1002, 208) == ResourceTableError::invalid_table_address);
    assert(validate_indirect_resource_table_range(0x100000000ULL, 208) == ResourceTableError::invalid_table_address);
    assert(validate_indirect_resource_table_range(0, 208) == ResourceTableError::invalid_table_address);
    assert(validate_indirect_resource_table_range(0x1000, 0) == ResourceTableError::invalid_table_address);
    assert(validate_indirect_resource_table_range(0xfffffff0ULL, 16) == ResourceTableError::none);
    assert(validate_indirect_resource_table_range(0xfffffff0ULL, 32) == ResourceTableError::invalid_table_address);

    std::uint32_t packet[3]{};
    assert(build_cs_indirect_table_pointer_packet(2, 0x12340000, 208, packet) == ResourceTableError::none);
    assert(packet[0] == 0xC0017602u); // SET_SH_REG, one dword, compute engine.
    assert(packet[1] == 0x242u);      // COMPUTE_USER_DATA_2 relative to SH register base.
    assert(packet[2] == 0x12340000u); // 32-bit table pointer; do not overwrite SGPR3.
    assert(build_cs_indirect_table_pointer_packet(15, 0x12340000, 208, packet) == ResourceTableError::none);
    assert(packet[1] == 0x24Fu);
    assert(build_cs_indirect_table_pointer_packet(16, 0x12340000, 208, packet) == ResourceTableError::invalid_user_data_slot);
    assert(build_cs_indirect_table_pointer_packet(2, 0x100000000ULL, 208, packet) == ResourceTableError::invalid_table_address);
    assert(build_cs_indirect_table_pointer_packet(2, 0x12340000, 208, nullptr) == ResourceTableError::null_output);
    return 0;
}
