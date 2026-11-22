# GoldHEN Detour SDK audit — pinned revision

**Date:** 2026-11-22  
**Repository:** `GoldHEN/GoldHEN_Plugins_SDK`  
**Pinned revision:** `bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`  
**Inspected files:** `source/Detour.c`, `include/Detour.h`  
**NR branch at inspection:** `feature/gcn8-numeric-kernels`, baseline `d8fde034fc05b2cb2f4e25b85cb093708018e831`

## Finding DETOUR-001 — unchecked memory-protection changes

The pinned SDK ignores the return value of `sceKernelMprotect` before writing executable code in all relevant paths:

- `Detour_WriteJump64`: changes protection then unconditionally copies the 14-byte jump.
- `Detour_WriteJump32`: changes protection then unconditionally copies the 5-byte jump.
- `Detour_DetourFunction64`: changes protection on the target function and continues regardless of failure.
- `Detour_DetourFunction32`: same unchecked target protection.
- `Detour_RestoreFunction`: attempts to change protection and unconditionally restores bytes.

**Impact:** if a protection transition fails, the SDK can proceed to write into a non-writable mapping. Depending on the exact mapping and firmware behavior, this can fault or leave the detour partially installed/restored. This is a concrete source-level defect in the pinned dependency. It is a plausible crash contributor, not proof of the user's CE-34878-0 root cause.

## Finding DETOUR-002 — partial-allocation cleanup is incomplete

In `Detour_DetourFunction64`, the trampoline is allocated before the stub. If stub allocation fails, the function returns without releasing the trampoline. More importantly, the target function's protection is changed before stub allocation succeeds. The project wrapper can clean some returned/recorded pointers, but it cannot reliably repair every failure path internal to this SDK function.

The 32-bit path has analogous partial-allocation and protection-order concerns. NR currently selects x64, so the 64-bit path is the immediate scope.

## Finding DETOUR-003 — restore API has no failure result

`Detour_RestoreFunction` and `Detour_Destroy` return `void`. Callers cannot distinguish successful restoration from a failed protection transition. `Detour_Destroy` unmaps the stub after the restore attempt regardless of whether restoration succeeded.

## Required remediation design (not yet implemented)

Do not patch the dependency in place without a controlled source pin. Preferred approach is to vendor a minimal, reviewed NR-owned x64 detour implementation or maintain a pinned fork/patch with these properties:

1. Validate all arguments and decode the full instruction span before changing target memory.
2. Allocate trampoline and stub first; initialize and validate both before touching the target function.
3. Check every `sceKernelMprotect` result. On failure, do not write; release only allocations owned by the current attempt.
4. Preserve original bytes before mutation and make target mutation transactional: if writing/verification fails, attempt checked restoration and retain diagnostics if restoration cannot be proven.
5. Verify written jump bytes and trampoline contents before reporting installation success.
6. Restore original bytes only after a successful protection transition; do not unmap the only recovery stub if restoration failed.
7. Return explicit status from install/restore/destroy paths and test all injected failure points on host with mocked syscalls.
8. Keep instruction-cache synchronization requirements explicit; do not assume `mprotect` alone guarantees instruction visibility.
9. Keep hook installation disabled on the PS4 until firmware-specific behavior and safe concurrency/unload have been validated.

## Scope and evidence limits

- This audit inspected the exact pinned upstream source and header, not a runtime trace from firmware 14.02.
- No source code or console state was modified by this audit.
- No PS4 hook, PRX activation, installation, or hardware test was performed.
- The existing NR wrapper's x64 selection and trampoline cleanup do not resolve unchecked protection changes inside the SDK.
- This finding is separate from the Title ID exclusion guard in `platform/ps4/goldhen/plugin/main.c`.

## Decision

**DETOUR-001/002/003: CONFIRMED SOURCE FINDINGS.**  
**CE-34878-0 root cause: NOT CONFIRMED.**  
**PS4 runtime readiness: BLOCKED.**

Next engineering task: implement the reviewed NR-owned x64 detour behind the existing hook contract, add mocked failure-path tests, run host CI and the exact PS4 PRX/PKG build, then independent package QA. No staging or installation is implied.
