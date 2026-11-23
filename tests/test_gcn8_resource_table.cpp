#include "resource_table_contract.h"

#include <cstdio>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "CHECK failed: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)
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
    CHECK(build_resource_table_layout(gemm, 4, &layout) == ResourceTableError::none);
    CHECK(layout.binding_count == 4);
    CHECK(layout.byte_size == 208);
    CHECK(layout.bindings[0].byte_offset == 0);
    CHECK(layout.bindings[1].byte_offset == 64);
    CHECK(layout.bindings[2].byte_offset == 128);
    CHECK(layout.bindings[3].byte_offset == 192);
    for (std::size_t i = 0; i < layout.binding_count; ++i) {
        const auto& binding = layout.bindings[i];
        CHECK(binding.descriptor_bytes == 16 && binding.array_count == 1);
    }

    CHECK(descriptor_size_bytes(DescriptorKind::sampled_image) == 32);
    CHECK(descriptor_size_bytes(DescriptorKind::storage_image) == 32);
    CHECK(descriptor_size_bytes(DescriptorKind::sampler) == 16);
    CHECK(descriptor_size_bytes(DescriptorKind::combined_image_sampler) == 48);
    CHECK(descriptor_size_bytes(static_cast<DescriptorKind>(0xff)) == 0);

    const BindingRequirement last_array[] = {{0, 0, 4, DescriptorKind::buffer}};
    CHECK(build_resource_table_layout(last_array, 1, &layout) == ResourceTableError::none);
    CHECK(layout.byte_size == 64);

    const BindingRequirement overlap[] = {
        {0, 0, 5, DescriptorKind::buffer},
        {0, 1, 1, DescriptorKind::buffer},
    };
    CHECK(build_resource_table_layout(overlap, 2, &layout) == ResourceTableError::overlapping_binding);
    CHECK(layout.binding_count == 0 && layout.byte_size == 0);

    const BindingRequirement wrong_set[] = {{1, 0, 1, DescriptorKind::buffer}};
    CHECK(build_resource_table_layout(wrong_set, 1, &layout) == ResourceTableError::unsupported_descriptor_set);
    const BindingRequirement unsorted[] = {{0, 2, 1, DescriptorKind::buffer}, {0, 1, 1, DescriptorKind::buffer}};
    CHECK(build_resource_table_layout(unsorted, 2, &layout) == ResourceTableError::bindings_not_strictly_sorted);
    const BindingRequirement duplicate[] = {{0, 1, 1, DescriptorKind::buffer}, {0, 1, 1, DescriptorKind::buffer}};
    CHECK(build_resource_table_layout(duplicate, 2, &layout) == ResourceTableError::bindings_not_strictly_sorted);
    const BindingRequirement zero_array[] = {{0, 0, 0, DescriptorKind::buffer}};
    CHECK(build_resource_table_layout(zero_array, 1, &layout) == ResourceTableError::zero_array_count);
    const BindingRequirement invalid_kind[] = {{0, 0, 1, static_cast<DescriptorKind>(0xff)}};
    CHECK(build_resource_table_layout(invalid_kind, 1, &layout) == ResourceTableError::invalid_descriptor_kind);

    CHECK(build_resource_table_layout(nullptr, 1, &layout) == ResourceTableError::null_requirements);
    CHECK(build_resource_table_layout(nullptr, 0, &layout) == ResourceTableError::none);
    CHECK(layout.binding_count == 0 && layout.byte_size == 0);
    CHECK(build_resource_table_layout(gemm, 4, nullptr) == ResourceTableError::null_output);

    BindingRequirement too_many[kMaxResourceBindings + 1]{};
    CHECK(build_resource_table_layout(too_many, kMaxResourceBindings + 1, &layout) == ResourceTableError::too_many_bindings);

    const BindingRequirement offset_overflow[] = {{
        0, std::numeric_limits<std::uint32_t>::max() / kDescriptorBindingStride + 1,
        1, DescriptorKind::buffer
    }};
    CHECK(build_resource_table_layout(offset_overflow, 1, &layout) == ResourceTableError::offset_overflow);

    const BindingRequirement table_overflow[] = {{
        0, 1, std::numeric_limits<std::uint32_t>::max() / 16,
        DescriptorKind::buffer
    }};
    CHECK(build_resource_table_layout(table_overflow, 1, &layout) == ResourceTableError::table_size_overflow);

    CHECK(validate_indirect_resource_table_address(0x1000) == ResourceTableError::none);
    CHECK(validate_indirect_resource_table_address(0xFFFFFFFCULL) == ResourceTableError::none);
    CHECK(validate_indirect_resource_table_address(0x1002) == ResourceTableError::invalid_table_address);
    CHECK(validate_indirect_resource_table_address(0x100000000ULL) == ResourceTableError::invalid_table_address);
    CHECK(validate_indirect_resource_table_address(0) == ResourceTableError::invalid_table_address);
    return 0;
}
