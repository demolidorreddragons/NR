# NR bounded HDE64 decoder adapter — implementation and host validation

**Date:** 2026-11-22  
**Repository:** `demolidorreddragons/NR`  
**Branch:** `feature/gcn8-numeric-kernels`  
**Tested commit:** `295851b90f772f91e49eb052b6e7bb44dc4620bc`  
**Host CI:** [host-tests #1507](https://github.com/demolidorreddragons/NR/actions/runs/45056033603) — telemetry job SUCCESS  
**Pinned GoldHEN SDK:** `bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`

## Implementation

Added:

- `platform/ps4/goldhen/plugin/hde64_bounded.h`
- `platform/ps4/goldhen/plugin/hde64_bounded.c`

The PRX Makefile compiles the adapter as a distinct object. The live GNM hook does not call it.

The API accepts a pointer plus an explicit byte count, copies at most 15 caller-declared bytes into a zero-initialized 32-byte scratch window, invokes the pinned upstream HDE64 decoder on the scratch buffer, and rejects zero-length, incomplete, overlong, or decoder-error results.

The normalized result reports:

- instruction length and upstream HDE64 metadata;
- relative immediate offset and width;
- displacement offset and width;
- CALL, JMP, conditional branch, LOOP, unsupported-relative, or no-relative classification;
- ModRM, SIB and address-size-override indicators;
- conservative RIP-relative classification for ModRM `mod=00,r/m=101`, disp32, without address-size override.

The pinned upstream HDE64 implementation does not correctly account for the disp32 field in the address-size-override ModRM `mod=00,r/m=101` form. The adapter therefore rejects this encoding when upstream metadata omits `F_DISP32`, rather than accepting a truncated decode. Operand-size-override rel16 branches are decoded but marked unsupported.

The CI harness calls this project adapter, not the earlier test-local prototype.

## Host test coverage and evidence

Run `45056033603` completed the telemetry job successfully. All steps passed, including the bounded-decoder harness, installer checks, GCN8 contract checks, CMake configure/build and CTest.

The bounded-decoder harness verifies:

1. The five known truncated-input guard-page faults still reproduce in raw upstream HDE64.
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

The test harness compiles the exact pinned SDK source revision above with the NR adapter using `-std=c11 -Wall -Wextra -Werror`.

## Critical contract and limitations

This is a bounded **adapter around the legacy decoder**, not a wholly independent x86 decoder.

The caller must guarantee that the complete supplied byte span is readable and remains stable during the copy. The adapter cannot validate arbitrary pointer provenance, mapping permissions, lifetime, concurrent unmapping, or query/read TOCTOU. The zero-padded scratch window contains the upstream decoder's speculative reads; the decoded-length check ensures the accepted instruction itself fits within the caller-supplied span.

The metadata is not relocation. In particular:

- unsupported relative encodings are identified but not rewritten;
- no branch target remapping for branches into the stolen instruction span exists;
- no displacement overflow handling or trampoline code generation exists;
- no live target prologue has been captured or measured on firmware 14.02.

The adapter is compiled into the PRX build as an unused module and is not connected to `install_gnm_hook`. The existing upstream detour path remains unchanged and must not be enabled on the strength of these tests.

## Build and hardware status

Host CI passed for the tested commit. The separate PS4 PRX/PKG build workflow `45055904514` was still running when this report was prepared; no PRX/PKG build result is claimed here. No PKG was staged or installed, and no PRX was activated on the console.

## Remaining gates before any hook integration

1. Establish a trustworthy readable-span acquisition contract on PS4, including mapping lifetime and concurrency.
2. Add instruction/branch support policy and relocation tests for every accepted encoding, including internal branches and branch-to-middle rejection.
3. Implement a fail-closed trampoline builder with signed displacement range checks.
4. Check all protection operations, verify writes/restores, and retain resources if restoration is uncertain.
5. Establish executable-code publication/cache semantics from an authoritative PS4 contract.
6. Establish callback quiescence before teardown/unmapping.
7. Validate every relevant firmware 14.02 API/ABI.
8. Obtain a genuinely independent review; automated CI is not independent review.

**Disposition:** a bounded-input adapter with normalized decode metadata is present, included in the PRX build graph, and host-tested. Live detour integration remains blocked.
