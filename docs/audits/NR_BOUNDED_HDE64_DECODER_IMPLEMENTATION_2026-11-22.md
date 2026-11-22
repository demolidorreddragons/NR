# NR bounded HDE64 decoder adapter — implementation and host validation

**Date:** 2026-11-22  
**Repository:** `demolidorreddragons/NR`  
**Branch:** `feature/gcn8-numeric-kernels`  
**CI run:** [host-tests #1504](https://github.com/demolidorreddragons/NR/actions/runs/45055797287)  
**Tested commit:** `643c467bd3fbf80840d4b4748c50753d92945c96`  
**Pinned GoldHEN SDK:** `bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`

## Implementation

Added:

- `platform/ps4/goldhen/plugin/hde64_bounded.h`
- `platform/ps4/goldhen/plugin/hde64_bounded.c`

The PRX Makefile now compiles the adapter as a distinct object. The live GNM hook does not call it.

The API accepts a pointer plus an explicit byte count, copies at most 15 caller-declared bytes into a zero-initialized 32-byte scratch window, invokes the pinned upstream HDE64 decoder on the scratch buffer, and rejects zero-length, incomplete, overlong, or decoder-error results.

The normalized result reports:

- instruction length and upstream HDE64 metadata;
- relative immediate offset and width;
- displacement offset and width;
- CALL, JMP, conditional branch, LOOP, unsupported-relative, or no-relative classification;
- ModRM, SIB and address-size-override indicators;
- conservative RIP-relative classification for ModRM `mod=00,r/m=101`, disp32, without address-size override.

The CI harness now calls this project adapter rather than the earlier test-local prototype.

## Host test coverage

The harness retains the five raw-upstream guard-page fault reproductions and verifies that the NR adapter rejects each truncated span without faulting.

Additional metadata vectors cover:

1. CALL rel32.
2. JMP rel8.
3. Jcc rel8 and rel32.
4. LOOP rel8.
5. RIP-relative MOV load.
6. RIP-relative store with a trailing immediate, validating displacement-offset calculation.
7. SIB-based absolute disp32, which must not be classified as RIP-relative.
8. Address-size override, which must not be classified as RIP-relative.
9. disp8 memory operand.

CI run `45055797287` reports the bounded-decoder step successful, followed by successful installer tests, GCN8 contract checks, CMake build, and CTest. The telemetry job completed successfully. The separate PS4 evidence-retrieval job is not part of decoder validation.

## Critical contract and limitations

This is a bounded **adapter around the legacy decoder**, not a wholly independent x86 decoder.

The caller must guarantee that the complete supplied byte span is readable and remains stable during the copy. The adapter cannot validate arbitrary pointer provenance, mapping permissions, lifetime, concurrent unmapping, or query/read TOCTOU. The zero-padded scratch window contains the upstream decoder's speculative reads; the decoded length check ensures the accepted instruction itself fits within the caller-supplied span.

The metadata is not relocation. In particular:

- unsupported relative encodings are identified but not rewritten;
- no branch target remapping for branches into the stolen instruction span exists;
- no displacement overflow handling or trampoline code generation exists;
- no live target prologue has been captured or measured on firmware 14.02.

The adapter is compiled into the PRX but is not connected to `install_gnm_hook`. The existing upstream detour path remains unchanged and must not be enabled on the strength of these tests.

## Build and hardware status

The host CI job passed for the tested commit. The PS4 PRX workflow triggered from the preceding build-integration commit was still pending when this report was authored; no PRX build result is claimed here. No PKG was staged or installed, and no PRX was activated on the console.

## Remaining gates before any hook integration

1. Establish a trustworthy readable-span acquisition contract on PS4, including mapping lifetime and concurrency.
2. Add instruction/branch support policy and relocation tests for every accepted encoding, including internal branches and branch-to-middle rejection.
3. Implement a fail-closed trampoline builder with signed displacement range checks.
4. Check all protection operations, verify writes/restores, and retain resources if restoration is uncertain.
5. Establish executable-code publication/cache semantics from an authoritative PS4 contract.
6. Establish callback quiescence before teardown/unmapping.
7. Validate every relevant firmware 14.02 API/ABI.
8. Obtain a genuinely independent review; automated CI is not independent review.

**Disposition:** useful bounded-input adapter and normalized decode metadata are now present and host-tested. Live detour integration remains blocked.
