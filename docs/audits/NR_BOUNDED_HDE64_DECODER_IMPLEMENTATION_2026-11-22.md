# NR bounded HDE64 decoder and stolen-span preflight — implementation results

**Date:** 2026-11-22  
**Repository:** `demolidorreddragons/NR`  
**Branch:** `feature/gcn8-numeric-kernels`  
**Host-tested commit:** `4de424468977f6fcee8f26ef36815db9da8df7fc`  
**Host CI:** [host-tests #1510](https://github.com/demolidorreddragons/NR/actions/runs/45056225389) — telemetry job SUCCESS  
**PRX build and independent PKG QA:** [PS4 PRX and PKG #644](https://github.com/demolidorreddragons/NR/actions/runs/45056164974) — build job SUCCESS; independent QA job SUCCESS; `QA_STATUS=PASS_PC_ONLY_NOT_HARDWARE_TESTED`  
**Pinned GoldHEN SDK:** `bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`

## Implemented modules

Added:

- `platform/ps4/goldhen/plugin/hde64_bounded.h`
- `platform/ps4/goldhen/plugin/hde64_bounded.c`
- `platform/ps4/goldhen/plugin/hde64_bounded_plan.h`
- `platform/ps4/goldhen/plugin/hde64_bounded_plan.c`

The PRX Makefile compiles both modules as distinct objects. Neither is called by the live GNM hook.

### Bounded decoder adapter

The API accepts a pointer plus an explicit byte count, copies at most 15 caller-declared bytes into a zero-initialized 32-byte scratch window, invokes the pinned upstream HDE64 decoder on the scratch buffer, and rejects zero-length, incomplete, overlong, or decoder-error results.

The normalized result reports:

- instruction length and upstream HDE64 metadata;
- relative immediate offset and width;
- displacement offset and width;
- CALL, JMP, conditional branch, LOOP, unsupported-relative, or no-relative classification;
- ModRM, SIB and address-size-override indicators;
- conservative RIP-relative classification for ModRM `mod=00,r/m=101`, disp32, without address-size override.

The pinned upstream HDE64 implementation does not correctly account for the disp32 field in the address-size-override ModRM `mod=00,r/m=101` form. The adapter therefore rejects this encoding when upstream metadata omits `F_DISP32`, rather than accepting a truncated decode. Operand-size-override rel16 branches are decoded but marked unsupported.

### Fail-closed stolen-span preflight planner

The planner accepts an explicit readable span and a required minimum patch width, then builds an instruction-aligned plan capped at 64 bytes and 16 instructions. It rejects:

- all relative control-flow instructions;
- relative encodings marked unsupported;
- all recognized RIP-relative memory operands;
- truncated/error decodes;
- spans that exceed its byte or instruction limits.

It publishes the plan only after the full requested minimum is decoded. It does not change target memory, allocate executable memory, generate a trampoline, or perform relocation. This is a conservative preflight policy, not proof that every other possible x86 instruction class is position-independent.

## Host test coverage and evidence

Run `45056225389` completed the telemetry job successfully. The bounded decoder/planner step, installer checks, GCN8 contract checks, CMake configure/build and CTest all passed.

The bounded-decoder harness verifies:

1. Five raw upstream guard-page faults still reproduce for truncated input.
2. All five truncated windows are rejected by the NR adapter without a fault.
3. Null pointer, null output and zero-length spans are rejected.
4. A one-byte NOP at an exact one-byte span is accepted.
5. A valid 15-byte maximum-length instruction is accepted.
6. CALL rel32, JMP rel8, Jcc rel8/rel32 and LOOP rel8 metadata offsets/widths.
7. Operand-size-override JMP rel16 is classified unsupported.
8. RIP-relative MOV load and store-with-trailing-immediate displacement offsets.
9. SIB absolute disp32 is not misclassified as RIP-relative.
10. The unsupported address-size-override disp32 form is rejected.
11. disp8 memory operand metadata.
12. A 14-byte NOP prologue produces a complete instruction-aligned preflight plan.
13. A non-RIP SIB absolute-memory instruction is accepted by the current preflight policy.
14. Relative branch, RIP-relative operand and unsupported rel16 all fail closed.
15. Insufficient spans, zero minimum, null output and excessive instruction counts are rejected.
16. A 64-byte span of 16 four-byte instructions is accepted at the configured maximum.

The harness compiles the exact pinned SDK source revision above with both NR modules using `-std=c11 -Wall -Wextra -Werror`.

## Critical contract and limitations

This is a bounded **adapter around the legacy decoder**, not a wholly independent x86 decoder.

The caller must guarantee that the complete supplied byte span is readable and remains stable during the copy. The adapter/planner cannot validate arbitrary pointer provenance, mapping permissions, lifetime, concurrent unmapping, or query/read TOCTOU. The zero-padded scratch window contains the upstream decoder's speculative reads; the decoded-length check ensures the accepted instruction itself fits within the caller-supplied span.

The preflight planner is deliberately conservative, but it is not a relocator. It does not:

- relocate any relative branch/call or RIP-relative operand;
- remap branches targeting instructions inside a stolen span;
- generate a trampoline or jump-back sequence;
- handle displacement overflow or branch-to-middle cases;
- prove that all non-relative decoded instructions are safe to copy for every CPU mode/semantic edge case;
- acquire or pin a PS4 target mapping.

Neither module is connected to `install_gnm_hook`. The existing upstream detour path remains unchanged and must not be enabled on the strength of these tests.

## Build and hardware status

Host CI passed for the host-tested commit. The PRX workflow completed successfully. Its build logs show both `hde64_bounded.o` and `hde64_bounded_plan.o` compiled for `x86_64-pc-freebsd12-elf` and linked into the PRX. The independent PKG QA job also completed successfully with `QA_STATUS=PASS_PC_ONLY_NOT_HARDWARE_TESTED`. This is package-structure/host QA only, not console validation.

No PKG was staged or installed, and no PRX was activated on the console. No firmware-14.02 hook execution, GNM dispatch, GPU execution, or hardware stability claim follows from these results.

## Remaining gates before any hook integration

1. Establish a trustworthy readable-span acquisition contract on PS4, including mapping lifetime and concurrency.
2. Expand instruction semantics coverage and prove the accepted no-relocation policy is complete for the target mode.
3. Implement relocation for any supported relative/RIP-relative instructions, or continue rejecting them; add internal-branch mapping, branch-to-middle rejection and signed displacement overflow tests.
4. Implement a fail-closed trampoline builder and verify generated code.
5. Check all protection operations, verify writes/restores, and retain resources if restoration is uncertain.
6. Establish executable-code publication/cache semantics from an authoritative PS4 contract.
7. Establish callback quiescence before teardown/unmapping.
8. Validate every relevant firmware 14.02 API/ABI.
9. Obtain a genuinely independent code review; automated CI is not independent review.

**Disposition:** the bounded decoder adapter and conservative stolen-span preflight planner are implemented, host-tested, and included in the PRX build graph. Live detour integration remains blocked.
