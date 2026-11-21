# GCN8 host arena contract — evidence report
Date: 2026-11-21
Branch: `feature/gcn8-numeric-kernels`
Integration commit: `ece55f8ff4cabea70e2c8690c1d9720e953a9e8f`

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
- Ranges are half-open: adjacent ranges do not overlap.
- The allocator is monotonic; it does not free, recycle, or manage concurrent access.

## Test coverage authored
- checked add/multiply normal and overflow cases;
- null output handling;
- alignments 1, 16, 32, 64 and aligned offsets;
- exact end-of-arena allocation and exhaustion;
- zero size, zero/non-power-of-two alignment, zero capacity;
- out-of-bounds and uint64 overflow;
- adjacent versus overlapping half-open ranges;
- invalid range arithmetic.

## Verification status
- Source and test files committed to the target branch.
- CMake/CTest registration committed.
- GitHub combined commit status query for the integration commit returned an empty status list.
- Commit-associated workflow query returned no runs. CI is therefore **NOT CONFIRMED / NOT RUN evidence unavailable**; do not label this change CI PASS.
- This execution environment did not run the repository's CMake build or CTest. The tests are authored but execution is **NOT_RUN**.

## Risks and boundaries
This is a host arithmetic/layout contract only. It does not establish PS4 memory allocation semantics, GPU VA derivation, GPU mapping, alignment requirements of a particular GNM resource, cache coherency, or hardware safety. Those remain separate blocked investigations and require primary API evidence and independently reviewed, reversible hardware procedures.
