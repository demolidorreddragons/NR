# NR x64 detour transaction state-machine design

**Date:** 2026-11-22  
**Repository:** `demolidorreddragons/NR`  
**Branch:** `feature/gcn8-numeric-kernels`  
**Design baseline:** `5253748be1d77491b5c69814602508f4175a2fd5`  
**Status:** design only — not implemented, not hardware validated

## Purpose and disposition

This document converts the existing decoder, relocation, memory-query, and lifecycle findings into a fail-closed transaction contract for a future NR-owned x64 detour. It deliberately does not replace the SDK detour, modify `main.c`, enable a hook, or claim that a safe PS4 implementation is currently possible.

**Current disposition remains BLOCKED.** The live hook still calls the pinned upstream GoldHEN SDK detour. The bounded decoder/planner are not connected to it. No operation described here authorizes a PS4 test, plugin activation, or console configuration change.

## New critical observation: patch publication is itself a concurrency boundary

The pinned SDK's x64 absolute jump is 14 bytes and is written as a multi-byte memory copy after a protection change. A 14-byte write is not an atomic instruction-stream replacement. Even if all bytes are eventually correct and rollback works, another game thread can fetch or execute a partially written instruction stream while installation is in progress.

Therefore callback reference counting during teardown is not sufficient. A future implementation needs a proven **install-time execution exclusion/rendezvous** that prevents any thread from executing the target range while its bytes are changed, and a separate **removal-time quiescence** that proves no thread remains in the hook, stub, or trampoline before reclamation. A mutex used only by the installer is not such a rendezvous because arbitrary game threads do not acquire it.

No supported PS4/GoldHEN primitive establishing this property has yet been identified. This is an additional hard gate, not a claim that a specific crash was caused by a torn patch.

## Required state machine

The implementation must expose explicit states to its caller; it must not collapse partial failure into a null pointer or a generic boolean.

| State | Meaning | Allowed next states |
|---|---|---|
| `EMPTY` | No target mutation and no owned executable resources | `PREFLIGHTED`, `ABORTED` |
| `PREFLIGHTED` | Target identity, bounded span, original bytes, and original protection metadata captured under a proven stable-mapping contract | `RESOURCES_READY`, `ABORTED` |
| `RESOURCES_READY` | Trampoline and stub fully allocated, populated, range-checked, and code-publication validated; target still untouched | `PATCH_WINDOW_HELD`, `ABORTED` |
| `PATCH_WINDOW_HELD` | A platform-proven rendezvous excludes execution of the target range; no target bytes changed yet | `TARGET_WRITE_STARTED`, `ABORTED` |
| `TARGET_WRITE_STARTED` | Target protection transition succeeded and target mutation may have partially occurred | `TARGET_VERIFIED`, `ROLLBACK_STARTED`, `POISONED` |
| `TARGET_VERIFIED` | Full patch bytes read back exactly; required code publication completed; original protection restored and verified | `ACTIVE`, `POISONED` |
| `ACTIVE` | Hook is installed; trampoline/stub and metadata remain owned | `REMOVE_WINDOW_HELD`, `POISONED` |
| `REMOVE_WINDOW_HELD` | Removal rendezvous prevents new entries and waits for all in-flight target/hook/trampoline execution to drain | `RESTORE_STARTED`, `POISONED` |
| `RESTORE_STARTED` | Original bytes are being restored while execution is excluded | `RESTORED_VERIFIED`, `POISONED` |
| `RESTORED_VERIFIED` | Original bytes and original protection are verified; no execution can still reference generated code | `RETIRED`, `POISONED` |
| `RETIRED` | Executable resources may be unmapped and metadata cleared | terminal |
| `ABORTED` | Preparation failed before any target mutation; owned temporary resources were safely released | terminal |
| `ROLLBACK_STARTED` | Recovery is attempting to restore the exact original target bytes/protection after mutation began | `ABORTED`, `POISONED` |
| `POISONED` | Target state, protection, publication, or execution reachability is uncertain | terminal for automatic cleanup; retain all potentially reachable executable resources |

A transition must be committed only after its postconditions are observed. The state must be stored before an operation whose failure could leave partial effects, so recovery knows the operation may have changed the target.

## Transaction invariants

1. **No target mutation during preparation.** Decode, classify, allocate, generate, validate, and publish trampoline/stub before changing target protection or bytes.
2. **Stable source window.** The decoder's input must be readable and stable for the entire copy. A successful `sceKernelVirtualQuery` or `sceKernelQueryMemoryProtection` call alone does not pin a mapping and does not close query/read TOCTOU.
3. **Fail-closed relocation.** Until a complete relocator is implemented and tested, reject every relative control-flow instruction, unsupported relative encoding, and RIP-relative operand. Also reject any instruction class not explicitly covered by the accepted-copy policy.
4. **No guessed original protection.** Preserve and verify actual target permissions using a documented API contract. If the firmware contract is unavailable or ambiguous, stop before mutation.
5. **Install rendezvous required.** No multi-byte target write may occur while another thread can execute/fetch from the target range. The primitive and its firmware 14.02 contract must be evidenced.
6. **Check every syscall.** Every map, protect, unmap, and relevant query result is checked. A successful call is not inferred from a non-null pointer.
7. **Read-back verification.** Verify generated code before publication, target patch bytes after install, and original target bytes after restore.
8. **Publication contract required.** Executable-code publication and any required cache synchronization must be established from authoritative PS4/FreeBSD or validated firmware evidence. Do not insert a guessed cache-flush API.
9. **Rollback is not assumed successful.** Any failed protection transition, partial write, byte mismatch, failed restore, or uncertain mapping state moves to recovery; failed recovery moves to `POISONED`.
10. **Never free reachable code.** In `POISONED`, retain trampoline, stub, metadata, and any mapping that a possibly installed target may still reference. Log a precise failure state; do not clear the structure.
11. **Removal requires quiescence.** Stop new callback entries, wait for all in-flight callbacks and any thread executing generated code, restore/verify target, and only then unmap resources.
12. **No silent upstream fallback.** Failure of the NR-owned path must not call the existing SDK detour as a fallback.

## Operation interface required for host tests

A future host-testable core should depend on an injected operations table rather than directly calling PS4 syscalls. At minimum it needs explicit operations for:

- acquire/release a stable target execution/read window (not just query metadata);
- query and preserve original mapping/protection attributes;
- allocate/free trampoline and stub mappings;
- write and read back generated code;
- publish executable code according to the verified platform contract;
- enter/leave install-time target execution exclusion;
- change and verify target protection;
- write and read back target bytes;
- enter removal gate, prevent new entries, and drain in-flight execution;
- emit durable state/error telemetry.

The interface must distinguish an operation that failed without side effects from one that may have partially succeeded. A plain `bool` return is insufficient for operations such as target writes and protection changes unless the contract guarantees all-or-nothing behavior.

## Mandatory failure-injection matrix

The host core is not eligible for PRX integration until tests inject failure before and after every side-effect boundary:

| Failure point | Required assertion |
|---|---|
| Mapping query / stable-window acquisition | No target mutation; reject malformed, cross-region, or unstable spans |
| Decoder / planner | No allocation or target mutation |
| Trampoline allocation / stub allocation | Release only resources known not to be reachable |
| Generated-code write / verification | Target untouched; failed resources safely released |
| Code publication | Target untouched; do not proceed to patch |
| Install rendezvous | Target untouched |
| Protection change to writable | Target untouched; verify actual protection before cleanup |
| Partial target write | Attempt exact rollback while execution remains excluded |
| Patch read-back mismatch | Roll back; if rollback cannot be proven, poison and retain resources |
| Original-protection restore | Poison and retain resources if current protection is uncertain |
| Install rendezvous release | Do not report ACTIVE if execution exclusion state is uncertain |
| Removal gate / drain | Do not restore or unmap while any callback/code execution may remain |
| Original-byte restore / read-back | Retain all reachable resources on any uncertainty |
| Unmap trampoline / stub | Preserve bookkeeping for any mapping whose unmap result is uncertain |

Tests must additionally cover page-boundary spans, concurrent mapping changes, internal branch targets, branch-to-middle rejection, signed displacement overflow, all accepted instruction encodings, and deterministic state-transition traces.

## Evidence required before platform adapter work

The following are unresolved and block implementation against the PS4 runtime:

- documented or validated 14.02 semantics for `sceKernelVirtualQuery`, `sceKernelQueryMemoryProtection`, and `sceKernelMprotect`;
- a mapping-lifetime/read-stability guarantee that closes query/read TOCTOU;
- a supported install-time rendezvous preventing execution during a multi-byte target patch;
- a removal-time quiescence primitive that covers target, hook, stub, and trampoline;
- authoritative executable-code publication/cache semantics;
- exact target ABI/prologue and a supported relocation policy;
- a recovery policy for failed protection restoration that is safe across plugin unload.

Public OpenOrbis declarations establish API names and signatures only. The inspected public GoldHEN loader source does not establish a general callback-quiescence guarantee. Neither source is sufficient evidence for the missing runtime contracts.

## Implementation sequence after the gates are resolved

1. Freeze the operation contracts and state-transition postconditions.
2. Implement the host-only transaction core with injected operations and deterministic fault injection.
3. Add generated trampoline/stub verifier and relocation policy tests.
4. Obtain a genuinely independent review of the host core and evidence bundle.
5. Implement the PS4 operations adapter only after authoritative platform contracts are documented.
6. Build PRX and inspect imports/ABI; keep the hook disabled by default.
7. Only after explicit approval, plan a controlled hardware test. Host/CI success is not hardware approval.

## Change boundary

This document is an architecture/design deliverable only. It does not alter executable code, workflows, PRX, PKG, plugin registration, `plugins.ini`, or PS4 state. No package was staged or installed, no PRX was activated, and no console test was run.
