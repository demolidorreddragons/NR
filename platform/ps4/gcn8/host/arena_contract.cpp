#include "arena_contract.h"
namespace pne::gcn8 {
bool checked_add_u64(std::uint64_t a, std::uint64_t b, std::uint64_t* out) noexcept {
    if (!out) return false;
    *out = 0;
    if (b > UINT64_MAX - a) return false;
    *out = a + b;
    return true;
}
bool checked_mul_u64(std::uint64_t a, std::uint64_t b, std::uint64_t* out) noexcept {
    if (!out) return false;
    *out = 0;
    if (a != 0 && b > UINT64_MAX / a) return false;
    *out = a * b;
    return true;
}
ArenaError arena_init(std::uint64_t capacity, Arena* out) noexcept {
    if (!out) return ArenaError::null_output;
    *out = {};
    if (!capacity) return ArenaError::zero_capacity;
    out->capacity = capacity;
    return ArenaError::none;
}
ArenaError validate_arena_range(std::uint64_t capacity, ArenaRange r) noexcept {
    if (!r.size) return ArenaError::zero_size;
    std::uint64_t end = 0;
    if (!checked_add_u64(r.offset, r.size, &end)) return ArenaError::arithmetic_overflow;
    return end <= capacity ? ArenaError::none : ArenaError::out_of_bounds;
}
bool arena_ranges_overlap(ArenaRange a, ArenaRange b, bool* overlap) noexcept {
    if (!overlap) return false;
    *overlap = false;
    std::uint64_t ae = 0, be = 0;
    if (!a.size || !b.size || !checked_add_u64(a.offset, a.size, &ae) ||
        !checked_add_u64(b.offset, b.size, &be)) return false;
    *overlap = a.offset < be && b.offset < ae;
    return true;
}
ArenaError arena_allocate(Arena* arena, std::uint64_t size, std::uint64_t alignment, ArenaRange* out) noexcept {
    if (!out || !arena) return ArenaError::null_output;
    *out = {};
    if (!size) return ArenaError::zero_size;
    if (!alignment || (alignment & (alignment - 1)) != 0) return ArenaError::invalid_alignment;
    std::uint64_t biased = 0;
    if (!checked_add_u64(arena->cursor, alignment - 1, &biased)) return ArenaError::arithmetic_overflow;
    const std::uint64_t offset = biased & ~(alignment - 1);
    const ArenaRange candidate{offset, size};
    const auto valid = validate_arena_range(arena->capacity, candidate);
    if (valid != ArenaError::none) return valid;
    std::uint64_t end = 0;
    if (!checked_add_u64(offset, size, &end)) return ArenaError::arithmetic_overflow;
    out->offset = offset;
    out->size = size;
    arena->cursor = end;
    return ArenaError::none;
}
const char* arena_error_string(ArenaError e) noexcept {
    switch (e) {
    case ArenaError::none: return "none";
    case ArenaError::null_output: return "null output";
    case ArenaError::zero_capacity: return "zero capacity";
    case ArenaError::zero_size: return "zero size";
    case ArenaError::invalid_alignment: return "alignment must be a non-zero power of two";
    case ArenaError::arithmetic_overflow: return "unsigned arithmetic overflow";
    case ArenaError::out_of_bounds: return "range exceeds arena capacity";
    case ArenaError::overlap: return "ranges overlap";
    }
    return "unknown";
}
}
