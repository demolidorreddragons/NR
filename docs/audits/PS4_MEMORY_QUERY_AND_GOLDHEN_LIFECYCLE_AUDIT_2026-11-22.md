# PS4 memory-query and GoldHEN lifecycle contract audit — 2026-11-22

## Scope

Follow-up to:
- `GOLDHEN_DETOUR_SDK_AUDIT_2026-11-22.md`
- `GOLDHEN_DETOUR_RELOCATION_AUDIT_2026-11-22.md`
- `GOLDHEN_HDE64_BOUNDED_DECODER_AUDIT_2026-11-22.md`
- `NR_X64_DETOUR_INTEGRATION_TEARDOWN_AUDIT_2026-11-22.md`

This pass checks whether the project's pinned PS4 headers expose memory-map query primitives and whether public GoldHEN plugin-loader source establishes an unload/quiescence guarantee. It does not change executable code, PRX, PKG, plugin registration, or console state.

## Revisions and source identity

- NR branch: `feature/gcn8-numeric-kernels`
- NR HEAD inspected: `36abfb75289f16a7ded4abe54e2cf2477ea99e4c`
- GoldHEN Plugins SDK pin in NR workflow: `bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`
- OpenOrbis PS4 Toolchain header tag inspected: `v0.5.4`

## Finding MEMQUERY-001 — pinned OpenOrbis headers expose two memory-query APIs

The exact OpenOrbis v0.5.4 `include/orbis/libkernel.h` declares:

```c
int32_t sceKernelQueryMemoryProtection(void *, void **, void **, int32_t *);
int32_t sceKernelVirtualQuery(const void *, int32_t,
                              OrbisKernelVirtualQueryInfo *, size_t);
```

The pinned `include/orbis/_types/kernel.h` defines `OrbisKernelVirtualQueryInfo` with:
- `start_addr`, `end_addr`
- `offset`
- `prot`, `mtype`
- flags for flexible/direct/stack/pooled/committed memory
- a 32-byte `name`

This corrects the earlier audit's open question about whether the public project toolchain headers expose any region-query API: they do. The declarations alone do **not** establish firmware 14.02 behavior, exact query flags, return-value semantics, region-boundary inclusivity, or whether either API can safely be used to read an arbitrary address. The header comments are empty. No runtime call was made.

### Engineering consequence

A future bounded decoder can investigate a fail-closed preflight:
1. Query the mapping containing the candidate instruction pointer.
2. Require a successful query and a region whose returned bounds contain the complete requested read window.
3. Require read permission from the returned protection field.
4. Limit each decode operation to bytes remaining in that verified region and a hard architectural maximum.
5. Reject query failures, malformed/inconsistent bounds, decoder errors, zero/overlong instruction lengths, and any instruction crossing the validated window.

This is only a proposed preflight, not yet a safe-read guarantee. A query-then-read sequence has a time-of-check/time-of-use race if another thread can unmap or reprotect the region. It also does not pin the mapping. A production implementation must either establish a platform-supported stability/serialization contract or avoid decoding arbitrary live target memory. Do not treat a single successful query as proof that subsequent reads cannot fault.

## Finding MEMQUERY-002 — richer VirtualQuery metadata is available, but its contract is undocumented in the pinned headers

`OrbisKernelVirtualQueryInfo` provides region bounds and protection metadata that are more expressive than a boolean readable check. However, the v0.5.4 declaration has no accompanying contract comment. The structure's presence is evidence of an API surface, not proof that its semantics are stable or identical on firmware 14.02.

Before production use, resolve from authoritative implementation/reversing evidence:
- valid `flags` values passed as the second argument;
- required `size` argument and structure-version expectations;
- return code and failure semantics;
- whether `start_addr`/`end_addr` describe a half-open interval;
- protection bit definitions and whether they describe current effective access;
- behavior across adjacent mappings and concurrent mapping changes.

The simpler `sceKernelQueryMemoryProtection` declaration likewise has no comments in the pinned header. Do not infer its out-parameter meanings solely from names.

## Finding GH-LIFECYCLE-001 — public loader source does not establish safe runtime unload

The public GoldHEN Plugins Repository plugin-loader source resolves `plugin_load` and `plugin_unload`, invokes `plugin_load`, and invokes `plugin_unload` in the shown path when plugin initialization returns non-zero. The inspected source does not establish a general runtime unload protocol, callback quiescence, thread rendezvous, or a guarantee that no thread is executing a hook/trampoline when an unload callback runs.

This is deliberately a narrow conclusion about the inspected public loader source. It does not prove that GoldHEN has no other private/internal unload path. The SDK's example plugins demonstrate hook removal in `plugin_unload`, but that convention is not a synchronization guarantee.

### Engineering consequence

NR must not free the trampoline or stub merely because its input worker thread has joined. A safe teardown still needs a proven callback-entry gate plus active-callback drain/quiescence, and a failure mode that retains executable resources if target restoration cannot be confirmed. If no platform-supported rendezvous exists, unload must fail closed or defer reclamation rather than free code that may still be executing.

## Existing decoder/detour constraints remain

The pinned SDK HDE64 interface is `hde64_disasm(const void *code, hde64s *hs)`; it has no input length. Its implementation reads prefixes, opcodes, ModRM/SIB, displacement, and immediates before the final instruction-length check. The SDK detour copies stolen bytes to a different address without relocating relative branches or RIP-relative memory operands. These defects are not repaired by discovering memory-query APIs.

The SDK detour also ignores `sceKernelMprotect` return values and exposes void restore/destroy functions, so the NR caller cannot know whether restoration succeeded before unmapping trampoline memory.

## Decision

**Do not implement or activate a replacement hook in this pass.** The memory-query declarations make a bounded decoder investigation more concrete, but their 14.02 runtime contract and mapping-stability guarantees remain unresolved. GoldHEN's public loader source also does not supply the missing callback-quiescence contract.

## Next safe engineering step

Build a host-only, isolated decoder test harness before any PRX integration:
- compile the pinned HDE64 source in a subprocess-based test executable;
- place test bytes at the end of a readable page followed by a guard page;
- test truncated prefixes, opcode/ModRM/SIB/displacement/immediate truncation, malformed encodings, maximum-length encodings, and decoder error flags;
- assert that the current decoder can fault on truncated input only as a negative characterization test, not as a passing safety property;
- use the harness to validate a separately implemented bounded decoder or bounded byte-window adapter;
- add relocation classification tests for relative control-flow and RIP-relative memory operands;
- inject failures at every allocation/protection/write/restore stage and assert no target mutation before all preparation gates pass.

Host tests can prove host behavior of the test harness and decoder implementation only. They cannot prove PS4 VM API semantics, firmware 14.02 behavior, concurrent console execution safety, or hardware runtime.

## Evidence links

- OpenOrbis v0.5.4 libkernel declarations: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.4/include/orbis/libkernel.h
- OpenOrbis v0.5.4 query-info structure: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.4/include/orbis/_types/kernel.h
- GoldHEN public plugin-loader source: https://github.com/GoldHEN/GoldHEN_Plugins_Repository/blob/main/plugin_src/plugin_loader/source/main.c
- Pinned GoldHEN SDK Detour implementation: https://github.com/GoldHEN/GoldHEN_Plugins_SDK/blob/bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7/source/Detour.c
- Pinned GoldHEN SDK HDE64 implementation: https://github.com/GoldHEN/GoldHEN_Plugins_SDK/blob/bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7/source/HDE64.c

## Change and validation boundary

Documentation-only audit. No executable source, workflow, PRX, PKG, staging, installation, plugin activation, `plugins.ini`, or PS4 state was changed. No host test or hardware test was run in this pass.
