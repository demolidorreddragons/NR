#ifndef PNE_GCN8_ARENA_CONTRACT_H
#define PNE_GCN8_ARENA_CONTRACT_H
#include <cstdint>
namespace pne::gcn8 {
enum class ArenaError { none, null_output, zero_capacity, zero_size, invalid_alignment, arithmetic_overflow, out_of_bounds, overlap };
struct Arena { std::uint64_t capacity = 0; std::uint64_t cursor = 0; };
struct ArenaRange { std::uint64_t offset = 0; std::uint64_t size = 0; };
bool checked_add_u64(std::uint64_t a, std::uint64_t b, std::uint64_t* out) noexcept;
bool checked_mul_u64(std::uint64_t a, std::uint64_t b, std::uint64_t* out) noexcept;
ArenaError arena_init(std::uint64_t capacity, Arena* out) noexcept;
ArenaError arena_allocate(Arena* arena, std::uint64_t size, std::uint64_t alignment, ArenaRange* out) noexcept;
ArenaError validate_arena_range(std::uint64_t capacity, ArenaRange range) noexcept;
bool arena_ranges_overlap(ArenaRange a, ArenaRange b, bool* overlap) noexcept;
const char* arena_error_string(ArenaError error) noexcept;
}
#endif
