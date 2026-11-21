# V# host codec verification — 2026-11-21

## Scope
Static, host-only verification of the four-dword GNM V# buffer descriptor codec. No PS4 runtime, GNM submission, cache operation, PRX activation, package installation, or hardware dispatch was performed.

## Primary layout reference
OpenGNM commit pinned by this project: `4b295ca54c82c83acf308d1c646a2dfa9ae57350`, file `include/gnm_buffer.h`.
The upstream `GnmBuffer` structure declares:
- dword 0: `baseaddress` (32 bits)
- dword 1: `baseaddresshi:12`, `mtype_l1s:2`, `mtype_l2:2`, `stride:14`, `cacheswizzle:1`, `swizzleen:1`
- dword 2: `numrecords` (32 bits)
- dword 3: `dstselx/y/z/w:3` each, `numformat:3`, `dataformat:4`, `elementsize:2`, `indexstride:2`, `addtiden/atc/hashen/heap:1` each, `mtype:3`, `type:2`
- upstream static assertion: `sizeof(GnmBuffer) == 0x10`.

## Implementation
Added `platform/ps4/gcn8/host/vsharp_codec.h/.cpp`. The codec uses explicit masks/shifts and does not depend on C++ bitfield layout or GNM runtime calls. It validates nonzero 4-byte-aligned 44-bit addresses and the documented widths of stride, channel selectors, numeric/data formats, and remaining packed fields. The record count is intentionally a full 32-bit dword.

The codec only serializes caller-supplied values. It does not infer or endorse mtype/cache policy, prove the GPU address is valid on the target firmware, or validate resource ownership/coherency.

## Tests
Added `tests/test_gcn8_vsharp_codec.cpp` and registered CTest `gcn8_vsharp_codec` in CMake.
Coverage includes:
- A mixed-field vector with exact expected dwords.
- Maximum representable address, stride, record count and packed fields.
- Zero and out-of-range addresses, misalignment, stride overflow.
- Channel, numformat, dataformat, mtype, elementsize, indexstride and type overflow.
- Null output and zeroed output on validation failures.

## Local execution evidence
The codec and expanded test were compiled and executed in the available Linux host container with:
```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic vsharp_codec.cpp test.cpp -o test
./test
```
Result: PASS. This local invocation compiled the codec source and exercised the same test vectors committed in the repository.

## CI status
GitHub Actions run/status for the final commit `e6c17de58eed4cd3491ff03685bad24ddefa53a8` was not exposed by the available repository status query at report time (no status entries returned). Therefore repository CI is recorded as NOT_CONFIRMED, not PASS.

## Disposition
Host serialization and boundary tests: PASS (local).
Repository CI: NOT_CONFIRMED.
GPU VA validity, descriptor semantics on PS4, cache policy/coherency, submit/fence/readback, and hardware execution: BLOCKED / NOT_RUN.
