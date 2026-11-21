# GCN8 host arena contract — evidence report
Date: 2026-11-21
Branch: `feature/gcn8-numeric-kernels`
Integration commit: `ece55f8ff4cabea70e2c8690c1d9720e953a9e8f`
Latest test-hardening commit: `23ca2dd4675947ea76fc98caeb12215bd3f0330a`

## Scope
Introduces a host-only, GPU-independent arena contract:
- checked unsigned 64-bit addition and multiplication;
- fixed-capacity arena initialization;
- monotonic suballocation with power-of-two alignment;
- overflow-safe range validation;
- half-open interval overlap detection.

Files:
- `platform/ps4/gcn8/host/arena_contract.h`
- `platform/ps4/gcn8/host/arena_contract.cpp`
- `tests/test_gcn8_arena_contract.cpp`
- `CMakeLists.txt`

## Contract decisions
- Arena offsets are integer offsets relative to a host-side capacity model.
- They are NOT CPU pointers, GPU virtual addresses, physical addresses, or GNM direct-memory offsets.
- No allocation, mapping, cache operation, descriptor construction, command submission, or PS4 API is performed.
- Zero-capacity arenas and zero-sized allocations/ranges are rejected.
- Alignment must be a non-zero power of two.
- Allocation failure leaves the cursor unchanged and clears the output range.
- Alignment applies to the arena-relative offset; a future backing allocation must itself satisfy any required base alignment.
- Ranges are half-open: adjacent ranges do not overlap.
- The allocator is monotonic; it does not free, recycle, or manage concurrent access.

## Test coverage authored
- checked add/multiply normal and overflow cases;
- null output handling;
- alignments 1, 16, 32, 64 and aligned offsets;
- exact end-of-arena allocation and exhaustion;
- failure atomicity (cursor unchanged and output cleared), including corrupted cursor state;
- zero size, zero/non-power-of-two alignment, zero capacity;
- out-of-bounds and uint64 overflow;
- adjacent versus overlapping half-open ranges;
- invalid range arithmetic.

## Verification status
- Source and test files committed to the target branch.
- CMake/CTest registration committed.
- Host test workflow run `44865198168` for integration commit `ece55f8ff4cabea70e2c8690c1d9720e953a9e8f`: **SUCCESS**.
- Follow-up workflow run `44865213327` for report commit `e1039a8c3a994f11c8eb4ccdd9918c1d7540c2f7`: **SUCCESS**.
- The follow-up run built `test_gcn8_vsharp_codec` and `test_gcn8_arena_contract`; CTest reported **7/7 passed**, including both tests.
- Strict local host compile/run of arena arithmetic/allocation tests passed with `g++ -std=c++17 -Wall -Wextra -Werror -pedantic`.
- Additional failure-atomicity assertions were committed afterward in `23ca2dd4675947ea76fc98caeb12215bd3f0330a`; a new CI run for this latest test-only commit is pending/not yet confirmed.

## Risks and boundaries
This is a host arithmetic/layout contract only. It does not establish PS4 memory allocation semantics, GPU VA derivation, GPU mapping, alignment requirements of a particular GNM resource, cache coherency, or hardware safety. Those remain separate blocked investigations and require primary API evidence and independently reviewed, reversible hardware procedures.
