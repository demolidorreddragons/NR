# Cooperative title-specific rendezvous — design and feasibility audit

**Date:** 2026-11-22  
**Repository:** `demolidorreddragons/NR`  
**Branch:** `feature/gcn8-numeric-kernels`  
**Baseline HEAD:** `c2d0a6d1eb05820f14b45550d59c5673e5382a94`  
**Scope:** static source inspection and protocol design only  
**Disposition:** feasible only for a title that explicitly participates; **not implementable as a generic GoldHEN PRX rendezvous**

## 1. Executive decision

The global `scePthreadSuspendAll()/scePthreadResumeAll()` path remains unsupported: the project has no firmware 14.02 implementation evidence, no postcondition contract, and no proof of the per-consumer instruction-stream handoff required for cross-modifying code.

A cooperative rendezvous is a more defensible direction, but it is not something a generic observer PRX can impose on an arbitrary closed game. Every thread that may execute the target must participate in a title-owned protocol, and thread creation/exit must be covered. The inspected current consumer, `platform/ps4/goldhen/plugin/main.c`, has no title adapter, participant registry, safe-point callback, thread-factory interception, or rendezvous handshake. It installs the upstream SDK's 14-byte detour from `plugin_load()` and joins only its own input worker during unload.

Therefore this audit does **not** implement a rendezvous or enable a replacement detour. For the current generic plugin deployment, the safe disposition is to keep NR-owned detour work disabled and not claim that the existing SDK detour has become safe. A cooperative implementation can be reconsidered only after a named title supplies and integrates the participant contract below.

## 2. Source inspection of the current integration

The current `main.c` establishes these facts:

- The plugin skips the two Neural Rendering TITLE_ID values, but otherwise installs `sceGnmSubmitAndFlipCommandBuffers` during `plugin_load()`.
- The hook installation calls `Detour_DetourFunction()` with `DetourMode_x64`; the SDK writes a 14-byte absolute jump.
- No game-thread registration or safe-point API is called.
- No game-thread creation/exit path is owned or wrapped by this PRX.
- `plugin_unload()` stops and joins only `g_input_thread`, then restores/unmaps the SDK detour resources.
- There is no participant count, per-thread acknowledgement, epoch, park state, or per-consumer release callback.

Joining the input worker does not stop title threads and does not prove that no title thread is in the hook, stub, or trampoline. This matches the earlier integration/teardown audit; no new runtime behavior is inferred here.

## 3. Cooperative protocol — required contract

The following is a design contract, not an assertion that an Orbis wait primitive or implementation already exists.

### 3.1 Participant ownership and complete coverage

A title adapter must own or mediate **all** threads capable of reaching the target:

1. The initial/main thread registers before it can enter the relevant render path.
2. Every worker is registered before its entry routine can execute title code.
3. Registration remains active until the thread has stopped executing title code and has completed its exit handshake.
4. Thread creation and exit are serialized against rendezvous generation changes. A thread cannot become runnable between the participant snapshot and the patch window without joining that generation.
5. Threads created by libraries, middleware, system callbacks, job systems, or third-party modules must either be covered by the title's thread-lifecycle integration or be proven unable to reach the target.
6. If complete coverage cannot be demonstrated, acquisition fails closed. A best-effort participant list is not a process-wide execution barrier.

A generic PRX that only observes GNM submission cannot establish this ownership. Interposing one known thread-creation API is not sufficient unless every thread-creation path is accounted for.

### 3.2 Safe-point entry

Each registered thread must call a title adapter safe-point routine at bounded intervals and at explicitly selected locations that:

- are outside the target instruction span, hook callback, generated stub, and trampoline;
- hold no allocator, loader, renderer, job-system, or other lock that the patch coordinator may need;
- have no partially completed operation whose invariants require another participant to run;
- cannot return through an in-flight path into the target before completing the release handoff;
- do not execute from a mapping that the transaction may unmap or reprotect.

The routine checks a monotonically increasing rendezvous generation. When a new generation is requested, the thread publishes its acknowledgement with release ordering and parks using a separately validated wait/wake primitive. It must not hold the participant-registry lock while parked.

### 3.3 Acquire algorithm (install or removal)

The coordinator must:

1. Preallocate all memory, buffers, logs, and rollback state before requesting a rendezvous. No allocation, logging, dynamic loading, or other potentially locking work is permitted while participants are parked.
2. Acquire the title's thread-lifecycle gate so registration/exit cannot race the participant snapshot.
3. Publish a new generation and request every registered participant to enter its safe point.
4. Wait for an acknowledgement from every participant in the frozen generation, with a bounded deadline.
5. Verify that every participant is parked at a safe point and that the participant set has not changed.
6. For removal, additionally prevent new hook entries and wait for the hook/trampoline in-flight count to reach zero. The counter alone is not the rendezvous; all participants must still be parked before executable resources are reclaimed.
7. Only after all acknowledgements and independent platform gates pass may the transaction enter its patch/restore window.

Any missing participant, timeout, generation mismatch, thread lifecycle race, or failed wait operation means **no target mutation**. The coordinator releases the request and returns a precise failure. If it cannot prove that every participant has left the parked state safely, it must retain all potentially reachable resources and report a poisoned state; it must not free memory optimistically.

### 3.4 Release and cross-modifying-code handoff

A parked thread must not simply wake and immediately continue its old instruction stream. For each participant, the release path must:

1. observe completion of the producer's target-byte writes and publication step;
2. perform the platform-validated consumer-side serializing handoff after that observation and before returning to title code that could fetch the modified stream;
3. acknowledge completion of the handoff for the same generation;
4. only then leave the rendezvous routine.

The AMD64 architecture manual distinguishes cross-modifying code from self-modifying code and describes a synchronous producer/consumer protocol in which the consumer performs a serializing instruction after the producer signals completion. The exact instruction sequence, compiler constraints, and Orbis/firmware requirements must be established for this target; this design intentionally does not select or emit CPUID, MFENCE, a cache API, or an inline-assembly sequence by guess.

The transaction cannot report the window released until every participant has completed this handoff. A wake signal by itself is not proof.

## 4. Install and removal are separate generations

### Install generation

- All participants are parked outside the target and generated-code paths.
- Mapping identity/lifetime and target bytes are revalidated under a proven mapping-ownership contract.
- The patch is written and read back while execution is excluded.
- Executable-code publication is completed under a validated Orbis contract.
- Every participant completes its consumer-side serializing handoff before the coordinator reports installation active.

### Removal generation

- Stop admitting new hook entries before requesting participant parking.
- Park all participants at safe points.
- Verify no in-flight callback, stub, or trampoline execution remains.
- Restore and read back original bytes and permissions while execution is excluded.
- Complete code publication and each participant's consumer-side handoff.
- Only after every acknowledgement may trampoline/stub mappings be reclaimed.

If any postcondition is uncertain, retain potentially reachable executable memory. Never unmap a trampoline merely because the plugin's own worker thread joined.

## 5. Failure and concurrency rules

| Condition | Required behavior |
|---|---|
| Participant missing or unregistered | Reject before target mutation |
| Thread created during generation | Thread-lifecycle gate must prevent it becoming runnable, or acquisition fails |
| Thread exits during generation | Exit handshake must preserve generation accounting; otherwise fail closed |
| Safe point reached while holding a forbidden lock | Adapter contract violation; reject this safe point |
| Timeout before all acknowledgements | Cancel generation; no patch; prove all participants resumed |
| Timeout/uncertainty after target mutation | Keep execution excluded if possible; rollback; otherwise POISONED and retain resources |
| Hook callback count does not drain | Do not restore or unmap |
| Consumer handoff fails or is unobservable | Do not release the generation as successful; retain resources |
| Participant set changes unexpectedly | Abort before mutation or poison after mutation |
| Plugin unload requested while generation is active | Defer unload until a proven completed generation; otherwise retain code/resources |

A mutex around the patching thread is not a substitute. A callback refcount without a barrier is not a substitute. Suspending only the renderer thread is not sufficient if any other thread can execute the target.

## 6. Independent gates that cooperation does not solve

Even a perfect title-level rendezvous does not prove:

- **Mapping lifetime:** `sceKernelVirtualQuery` and `sceKernelQueryMemoryProtection` are point-in-time metadata queries, not a mapping lease. The title must prove the target mapping remains valid and stable for the transaction, or the adapter must reject it.
- **Protection semantics:** exact firmware 14.02 `sceKernelMprotect` behavior, original-permission capture, and restoration must be validated.
- **Code publication:** the required Orbis executable-memory publication/cache contract remains unknown.
- **Instruction relocation:** the target prologue must satisfy a complete, bounded, tested relocation policy; the current bounded planner rejects relative and RIP-relative instructions and is not a live relocator.
- **GoldHEN unload lifecycle:** the public loader evidence does not guarantee that plugin unload occurs after all title callbacks and generated-code references have drained.
- **External actors:** the title protocol cannot prevent privileged loader/kernel actions unless mapping ownership/lifetime is independently guaranteed.

The rendezvous is one gate in the transaction, not a replacement for the other gates.

## 7. Host-test plan before any platform adapter

A future title adapter must first provide deterministic host tests for:

1. complete participant registration and thread lifecycle races;
2. generation snapshot stability;
3. simultaneous requests and generation ownership;
4. timeout before any target mutation;
5. participant exit and creation at every protocol boundary;
6. forbidden-lock/safe-point assertions;
7. per-participant release handoff ordering;
8. no release success until all handoff acknowledgements arrive;
9. removal admission closure and in-flight callback drain;
10. failure injection before/after every wait, acknowledgement, write, restore, and release boundary;
11. poison/retention behavior when a participant cannot be proven quiescent;
12. deterministic traces under stress and race sanitizers.

These tests validate the abstract protocol only. A real Orbis wait/wake adapter, memory mapping contract, protection behavior, executable-code publication, ABI, and firmware-specific thread coverage require separate evidence. A hardware test is a later, separately authorized step.

## 8. Decision and next engineering step

**Current generic plugin:** no cooperative participant exists, so there is no valid rendezvous to call. Do not implement a fake barrier in the PRX and do not replace the SDK detour with a mutex, sleep, thread enumeration snapshot, or guessed suspend API.

**Viable path:** choose a named title whose source or supported plugin interface can be modified to register every relevant thread, integrate safe points and thread lifecycle, and own the target mapping lifetime. Only then define a title-specific adapter and host-test the protocol. If the target title cannot provide that cooperation, the hook-based integration is unsupported for that title; use a supported title/plugin API or leave the hook disabled.

This is a design recommendation, not a code change or permission to patch, stage, install, activate, or test anything on the PS4.

## 9. Sources and evidence

- Current NR consumer source, `platform/ps4/goldhen/plugin/main.c`, inspected at baseline HEAD.
- NR integration/teardown audit: `docs/audits/NR_X64_DETOUR_INTEGRATION_TEARDOWN_AUDIT_2026-11-22.md`.
- NR mapping and cross-modifying-code audit: `docs/audits/PS4_MAPPING_STABILITY_AND_CODE_PUBLICATION_AUDIT_2026-11-22.md`.
- NR pthread suspend audit: `docs/audits/PS4_PTHREAD_SUSPEND_RENDEZVOUS_AUDIT_2026-11-22.md`.
- AMD64 Architecture Programmer's Manual, Volume 2, revision 3.38, §7.6.1, pp. 201–202: https://www.scs.stanford.edu/~zyedidia/docs/x86/amd-manual.pdf
- OpenOrbis v0.5.4 libkernel declarations (API names/signatures only): https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.4/include/orbis/libkernel.h

## 10. Change boundary

Documentation only. No executable source, PRX, PKG, workflow, plugin registration, console configuration, or PS4 state changed. No firmware binary was downloaded, no payload was executed, and no hardware test was performed. No package is approved for installation by this audit.
