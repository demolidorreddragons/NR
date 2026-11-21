#include "arena_contract.h"
#include <cassert>
#include <cstdint>
#include <limits>
using namespace pne::gcn8;
int main() {
 std::uint64_t v=99;
 assert(checked_add_u64(4,5,&v) && v==9);
 assert(!checked_add_u64(UINT64_MAX,1,&v) && v==0);
 assert(checked_mul_u64(7,9,&v) && v==63);
 assert(!checked_mul_u64(UINT64_MAX,2,&v) && v==0);
 assert(!checked_add_u64(1,1,nullptr));
 Arena a{}; assert(arena_init(256,&a)==ArenaError::none);
 ArenaRange r{};
 assert(arena_allocate(&a,13,1,&r)==ArenaError::none && r.offset==0 && r.size==13);
 assert(arena_allocate(&a,16,16,&r)==ArenaError::none && r.offset==16);
 assert(arena_allocate(&a,32,32,&r)==ArenaError::none && r.offset==32);
 assert(arena_allocate(&a,192,64,&r)==ArenaError::none && r.offset==64);
 assert(a.cursor==256);
 assert(arena_allocate(&a,1,1,&r)==ArenaError::out_of_bounds);
 assert(arena_allocate(&a,0,1,&r)==ArenaError::zero_size);
 assert(arena_allocate(&a,1,3,&r)==ArenaError::invalid_alignment);
 assert(arena_allocate(&a,1,0,&r)==ArenaError::invalid_alignment);
 assert(arena_allocate(nullptr,1,1,&r)==ArenaError::null_output);
 assert(arena_init(0,&a)==ArenaError::zero_capacity);
 assert(arena_init(8,nullptr)==ArenaError::null_output);
 assert(validate_arena_range(16,{8,8})==ArenaError::none);
 assert(validate_arena_range(16,{8,9})==ArenaError::out_of_bounds);
 assert(validate_arena_range(UINT64_MAX,{UINT64_MAX,1})==ArenaError::arithmetic_overflow);
 assert(validate_arena_range(16,{0,0})==ArenaError::zero_size);
 bool overlap=false;
 assert(arena_ranges_overlap({0,8},{8,4},&overlap) && !overlap);
 assert(arena_ranges_overlap({0,9},{8,4},&overlap) && overlap);
 assert(!arena_ranges_overlap({UINT64_MAX,2},{0,1},&overlap));
 assert(!arena_ranges_overlap({0,1},{0,1},nullptr));
 Arena huge{}; assert(arena_init(UINT64_MAX,&huge)==ArenaError::none);
 huge.cursor=UINT64_MAX-3;
 assert(arena_allocate(&huge,1,8,&r)==ArenaError::arithmetic_overflow);
 return 0;
}
