# NR x64 detour integration and teardown audit — 2026-11-22

## Scope

- Branch: `feature/gcn8-numeric-kernels`
- Audited HEAD before this report: `a0643a43a349e2eb8fe1545069c53d4fbab1d1d3`
- Consumer: `platform/ps4/goldhen/plugin/main.c`
- Existing contract test: `tests/test_goldhen_hook_contract.py`
- Build integration: `.github/workflows/ps4-prx.yml`
- SDK pin: `GoldHEN/GoldHEN_Plugins_SDK@bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`

The repository operating rules and project operational rules were read before this audit. This report is source-level only; no PS4 runtime or hardware operation was performed.

## Findings

### NR-DETOUR-INT-001 — NR teardown cannot observe restore failure

The current `remove_gnm_hook()` calls `Detour_Destroy()`, then unconditionally calls `sceKernelMunmap()` for `TrampolinePtr`, then zeroes the entire detour structure.

At the pinned SDK revision, `Detour_Destroy()` returns `void`; it calls `Detour_RestoreFunction()`, also `void`, and then unmaps `StubPtr` regardless of whether `sceKernelMprotect()` succeeded or the original target bytes were restored. The restore routine ignores the protection call's return value and does not verify restored bytes.

Consequently, the NR caller cannot distinguish:
- hook successfully removed and original bytes restored;
- protection change failed and target may still branch to the trampoline;
- write partially/incorrectly completed;
- target bytes were changed by another actor.

The caller then unmaps the trampoline and clears its only bookkeeping. This is an unsafe teardown contract. The existing wrapper cannot make upstream teardown transactional from outside the SDK.

### NR-DETOUR-INT-002 — install-failure cleanup also assumes SDK destroy is safe

If `Detour_DetourFunction()` returns null while `StubPtr` is non-null, `install_gnm_hook()` calls `Detour_Destroy()`. It subsequently unmaps `TrampolinePtr` and clears the structure. In the upstream implementation, failure can occur after target protection was changed, and some allocation-failure paths leak resources or leave altered protection state. The caller has no status channel to determine whether cleanup restored the target.

This is not fixed by checking `StubPtr` or `TrampolinePtr` for null. Those pointer checks establish allocation state, not target restoration.

### NR-DETOUR-INT-003 — current host contract test does not exercise detour semantics

`tests/test_goldhen_hook_contract.py` is a source-text assertion test. It checks that the x64 SDK API is selected, certain cleanup calls exist, and the own-title guard precedes hook/thread startup. It does not compile or execute a detour implementation and does not test:
- bounded decoder input;
- instruction boundaries or relocation;
- target protection failures;
- allocation failures after partial setup;
- byte verification or rollback;
- restore failure and resource retention;
- concurrent hook callbacks during unload;
- executable-code publication/cache synchronization.

Its PASS must not be interpreted as validation of the underlying detour implementation.

### NR-DETOUR-INT-004 — no proven safe unload/quiescence protocol

`plugin_unload()` stops and joins the input worker, then calls `remove_gnm_hook()`. Joining the input worker does not establish that no game thread is currently executing the GNM hook or its trampoline. No callback reference count, thread rendezvous, epoch/quiescence barrier, or equivalent protocol is present in the inspected code.

Unmapping the trampoline while a callback or a thread already redirected through it is active can invalidate an executing instruction stream. This is a separate lifecycle risk from byte restoration.

## Build integration observations

The workflow checks out the SDK at the fixed revision and builds its static hook library before linking the PRX. The upstream Makefile compiles all `source/*.c`, including `Detour.c` and `HDE64.c`. A safe NR-owned implementation therefore needs an explicit build integration change: it must not accidentally keep calling the upstream `Detour_DetourFunction` path for the GNM hook, and it must not create duplicate symbols or silently shadow SDK APIs.

The current workflow is a Linux hosted PRX/PKG build; it is not a PS4 LAN runner and does not prove firmware runtime behavior.

## Implementation gate / unresolved platform facts

A replacement is not safe to enable until the following are resolved from authoritative PS4/FreeBSD documentation or platform-specific evidence:

1. A safe way to establish and read a bounded executable-memory window at the target, including page boundaries. HDE64's pointer-only API cannot provide this guarantee.
2. Exact executable page protection semantics and a reliable way to preserve/restore original permissions.
3. An authoritative executable-code publication/cache synchronization contract for the target firmware.
4. A safe quiescence strategy for threads that may be inside the target, hook, or trampoline during install/removal.
5. A decoder/relocator with bounded input and a tested supported-instruction policy.

Until then, do not add an apparently safer wrapper around the same SDK calls and do not enable a partially transactional detour. The safe behavior is to keep the GNM hook disabled when these platform contracts are unavailable.

## Required test additions before enablement

- A compiled host-testable detour core separated from PS4 syscalls by an injected operations interface.
- Deterministic decoder vectors and exact byte-boundary tests.
- Failure injection for each allocation, protection transition, write, verification, rollback, and unmap.
- Tests proving that uncertain restoration retains every resource still reachable by the target.
- Concurrency tests proving unload waits for quiescence or deliberately retains the hook/resources.
- Generated trampoline byte inspection and range/overflow tests.
- PRX build/import QA and a genuinely independent source review. Hardware validation remains a separate gate.

## Disposition

- Confirmed: NR's current caller cannot observe upstream restore failure and unmaps the trampoline regardless.
- Confirmed: the existing hook contract test is source-text based and does not exercise detour semantics.
- Confirmed: worker-thread join is not a hook-callback quiescence proof.
- Not established: the historical console crash was caused by any of these findings.
- No source code, PRX, PKG, PS4 configuration, or console data was changed by this audit.
