# GoldHEN HDE64 decoder and bounded-read audit — 2026-11-22

## Scope and pinned inputs

- Consumer: `platform/ps4/goldhen/plugin/main.c`, x86-64 hook of `sceGnmSubmitAndFlipCommandBuffers`.
- GoldHEN SDK: `GoldHEN/GoldHEN_Plugins_SDK`, revision `bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`.
- Inspected SDK files: `source/Detour.c`, `include/Detour.h`, `source/HDE64.c`, `include/HDE64.h`, `Makefile`.
- OpenOrbis header inspected: `include/orbis/libkernel.h` from release tag `v0.5.4`.
- NR build compiles the SDK with `make -C GoldHEN_Plugins_SDK`, then links `-lGoldHEN_Hook` into the PRX. The SDK Makefile's `CFILES := $(wildcard $(PROJDIR)/*.c)` includes `HDE64.c` in the static hook library.

This is source/header analysis. No PS4 memory map, target prologue bytes, firmware runtime, cache behavior, or hook execution was observed.

## Additional findings

### HDE64-BOUND-001 — decoder has no input-length parameter

`hde64_disasm(const void *code, hde64s *hs)` accepts only a pointer and has no readable-byte bound. The implementation reads prefixes/opcodes/ModRM/SIB/displacements/immediates directly from the pointer. Its prefix scan can consume bytes before it knows whether a complete instruction is available; later decoding paths also dereference operands before the final instruction-length check.

The final `len > 15` check sets `F_ERROR_LENGTH` and clamps `len`, but it occurs after the reads. It is therefore not a memory-safety boundary. A caller cannot use the current API to prove that decoding stayed inside a readable window.

### HDE64-BOUND-002 — caller has no bounded prologue window

`Detour_GetInstructionSize` repeatedly invokes the unbounded decoder until at least the 14-byte x64 jump length is covered. It has no maximum aggregate byte count, no explicit readable-window length, and no page-boundary handling. `F_ERROR` is checked, but that does not prevent an out-of-range read that happens before the decoder returns.

A replacement must not pass an arbitrary target pointer directly to this decoder and call the result bounded. A local scratch copy alone is insufficient unless the copy itself is proven readable and the decoder is given a bounded implementation.

### HDE64-BOUND-003 — relative metadata is insufficient for relocation

The decoder reports `F_RELATIVE` for relative immediate control-flow instructions. It exposes ModRM fields and displacement flags, but it does not expose a normalized displacement offset or classify every RIP-relative memory operand as relocatable metadata. For the common 64-bit RIP-relative form, the caller must recognize ModRM `mod=00,r/m=101`, account for address-size override (`0x67`), and locate the disp32 field from the actual decoded encoding.

The current SDK detour ignores these metadata and copies the bytes verbatim. A replacement must either implement a fully tested relocator for each supported encoding or reject every unsupported relative/RIP-relative form before changing target memory.

### HDE64-BOUND-004 — decoder error flags must be rejected before any target mutation

The decoder sets error flags for opcode, operand, lock-prefix, and instruction-length errors. The SDK checks `F_ERROR` during length accumulation, but does not validate a separate bounded window because none exists. The replacement contract must reject zero-length decode, any error flag, overlong instruction, incomplete instruction, and aggregate stolen length exceeding a fixed documented maximum.

## API evidence and unresolved platform contract

OpenOrbis v0.5.4 declares:

- `int32_t sceKernelMmap(void *, size_t, int32_t, int32_t, int32_t, off_t, void **)`
- `int32_t sceKernelMprotect(const void *, size_t, int)`
- `int32_t sceKernelMunmap(void *, size_t)`

This confirms that `sceKernelMprotect` returns a status that can and must be checked. It does not establish exact firmware 14.02 behavior, page granularity, prior protection query, or cache-maintenance requirements.

The pinned SDK's `Detour_WriteJump64`, `Detour_DetourFunction64`, and `Detour_RestoreFunction` ignore `sceKernelMprotect` results before writing. The NR wrapper currently calls the SDK detour directly, so it cannot make those internal operations transactional.

No authoritative instruction-cache synchronization API or firmware 14.02 contract was identified in the inspected headers. x86 cache-coherence assumptions must not be substituted for a verified PS4/FreeBSD executable-code publication contract. This remains an explicit implementation blocker, not evidence that a cache flush is or is not required.

## Required NR-owned detour contract

Before implementation can be accepted:

1. Do not call the SDK's unbounded HDE64 decoder on the live target pointer. Use a decoder with an explicit input length, or a proven safe readable-window acquisition mechanism.
2. Bound total decode bytes and instruction count. Reject unreadable/incomplete windows, decoder errors, zero-length instructions, and unsupported encodings before any target protection or bytes are changed.
3. Initially prefer fail-closed rejection of all relative branches/calls and RIP-relative memory operands. Add relocation only after independent test vectors cover forward/backward targets, internal branches, branch-to-middle-of-instruction rejection, signed displacement overflow, and all supported encodings.
4. Allocate and completely populate trampoline and hook stub before target mutation. Validate every allocation, mapping, size, and generated branch target.
5. Check every `sceKernelMprotect` result. Snapshot original bytes and protection state where the platform provides a trustworthy query. If protection cannot be restored or target bytes cannot be verified, keep all reachable trampoline/stub resources alive and report a poisoned/uncertain state; never free memory that a still-installed hook may reference.
6. Define the executable-code publication/cache synchronization contract from authoritative PS4/FreeBSD documentation or validated platform primitives before enabling the hook.
7. Define concurrency/unload semantics. Do not restore/unmap while a callback may be executing; use a documented quiescence strategy or keep the hook resident when safe quiescence cannot be established.
8. Add host tests with mocked memory/protection/allocation operations, including failure injection at every operation boundary, byte-for-byte rollback checks, decoder boundary tests, and generated-code inspection.
9. Keep the feature disabled on unsupported prologues and all uncertain error paths. No silent fallback to the upstream SDK detour.
10. Run host tests, PRX build, static/import QA, and independent review. Hardware validation remains a separate gate.

## Current NR impact

`main.c` calls `Detour_DetourFunction` from the pinned SDK in `install_gnm_hook`. The current test `tests/test_goldhen_hook_contract.py` verifies wrapper/lifecycle source patterns but does not exercise decoder boundaries, instruction relocation, protection failures, concurrent execution, or executable-code publication.

This audit does not establish that the actual firmware 14.02 prologue contains a relative or RIP-relative instruction, that the detour caused the historical CE-34878-0, or that any hook was activated on hardware.

## Disposition

- Confirmed source-level risk: unbounded decoder reads and unbounded prologue accumulation are not guarded by the HDE64 API.
- Confirmed API fact: `sceKernelMprotect` returns `int32_t`; upstream detour code ignores that result.
- Not established: firmware 14.02 target bytes, memory mapping/protection query strategy, cache publication primitive, and safe live-patch quiescence.
- No code, PRX, PKG, PS4 configuration, or console data was changed by this audit.
