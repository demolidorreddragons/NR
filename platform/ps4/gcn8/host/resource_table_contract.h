#ifndef PNE_GCN8_RESOURCE_TABLE_CONTRACT_H
#define PNE_GCN8_RESOURCE_TABLE_CONTRACT_H
#include <cstddef>
#include <cstdint>
namespace pne::gcn8 {
enum class DescriptorKind : std::uint8_t { buffer, texel_buffer, sampled_image, storage_image, sampler, combined_image_sampler };
struct BindingRequirement { std::uint32_t descriptor_set, binding, array_count; DescriptorKind kind; };
struct BindingLayout { std::uint32_t binding, byte_offset, descriptor_bytes, array_count; };
inline constexpr std::uint32_t kDescriptorBindingStride = 64;
inline constexpr std::size_t kMaxResourceBindings = 32;
struct ResourceTableLayout { BindingLayout bindings[kMaxResourceBindings]{}; std::size_t binding_count = 0; std::uint32_t byte_size = 0; };
enum class ResourceTableError { none, null_requirements, null_output, too_many_bindings, unsupported_descriptor_set, zero_array_count, invalid_descriptor_kind, bindings_not_strictly_sorted, offset_overflow, overlapping_binding, table_size_overflow, invalid_table_address };
ResourceTableError build_resource_table_layout(const BindingRequirement* requirements, std::size_t requirement_count, ResourceTableLayout* output) noexcept;
ResourceTableError validate_indirect_resource_table_range(std::uint64_t gpu_address, std::uint32_t byte_size) noexcept;
std::uint32_t descriptor_size_bytes(DescriptorKind kind) noexcept;
const char* resource_table_error_string(ResourceTableError error) noexcept;
}
#endif
