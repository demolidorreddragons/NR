# PS4 thread suspension candidate audit — detour rendezvous

**Date:** 2026-11-22  
**Repository:** `demolidorreddragons/NR`  
**Branch:** `feature/gcn8-numeric-kernels`  
**Scope:** public API/header/source evidence only  
**Disposition:** candidate identified; **not approved for runtime use**

## Executive result

A potentially relevant API family was found in the exact OpenOrbis v0.5.4 header used by this project:

- `scePthreadSuspendAll()`
- `scePthreadResumeAll()`
- `scePthreadSuspend()`
- `scePthreadResume()`

The pinned header declares the all-thread functions as `void scePthreadSuspendAll();` and `void scePthreadResumeAll();`, with no parameters, result code, comments, timeout, or documented postcondition. The pinned header blob SHA-1 is `33be396ac17076541acadafca04c8683c2383a5f`.

The PS4 Developer Wiki also indexes FreeBSD-style symbols `pthread_suspend_all_np`, `pthread_resume_all_np`, `pthread_suspend_np`, and `pthread_resume_np` in libkernel. This makes the FreeBSD non-portable pthread suspension family a useful lead, but symbol-name similarity is not proof that Sony's `scePthread*` wrappers are aliases or preserve FreeBSD semantics.

**No PS4 14.02 contract was found** that proves which threads are suspended, whether the calling thread is excluded, whether all target-process threads are covered, whether suspension has completed before return, what happens to newly created threads, how nested/concurrent suspend operations behave, or whether the API can be safely used by a GoldHEN PRX in a game process.

Therefore this API is recorded as a research candidate only. It is not wired into the host coordinator or PRX, and must not be used as the install/remove execution gate.

## Evidence inspected

### 1. Exact OpenOrbis v0.5.4 declarations

Source: `OpenOrbis/OpenOrbis-PS4-Toolchain/include/orbis/libkernel.h`, tag `v0.5.4`.

Relevant declarations:

```c
void scePthreadResume();
void scePthreadResumeAll();
void scePthreadSuspend();
void scePthreadSuspendAll();
```

The declarations have no argument lists and return `void`. Nearby query/protection declarations are:

```c
int32_t sceKernelMprotect(const void *, size_t, int);
int32_t sceKernelMunmap(void *, size_t);
int32_t sceKernelQueryMemoryProtection(void *, void **, void **, int32_t *);
int32_t sceKernelVirtualQuery(const void *, int32_t, OrbisKernelVirtualQueryInfo *, size_t);
```

The header comments for these functions are empty. The OpenOrbis libkernel documentation describes the library as exposing Sony-wrapped pthread functionality, but does not document suspend-all behavior. This source is a declaration set, not a firmware contract.

### 2. FreeBSD reference semantics — informative, not transferable proof

The FreeBSD 12.1-STABLE manual (a closer version-family reference for this project's `x86_64-pc-freebsd12-elf` target) and the FreeBSD 14.0 manual both document `pthread_suspend_all_np()` as suspending all active threads except the calling thread. Both document the important warning that, until threads resume, the caller must not use non-async-signal-safe functions (other than the matching resume function), unless measures ensure all threads are suspended at safe points.

This warning matters for a detour transaction: a suspended game thread may be stopped while holding allocator, loader, stdio, or other process locks. The suspending PRX thread must not assume it can safely call allocation, logging, dynamic-loader, or other potentially locking routines while other threads remain suspended. Preparation and all fallible allocations therefore must occur before any such suspension if a future, validated implementation ever uses this mechanism.

However, the FreeBSD manual describes `pthread_suspend_all_np`, not Sony's `scePthreadSuspendAll`. It cannot establish Orbis behavior or firmware 14.02 behavior.

### 3. Public symbol index

PS4 Developer Wiki's libkernel symbol index lists both the `pthread_*_np` names and the `scePthread*` names. This supports the existence of a related API family in the public symbol catalog, but does not establish implementation equivalence, calling convention details beyond the OpenOrbis declarations, or lifecycle guarantees.

### 4. GoldHEN lifecycle evidence

The public GoldHEN plugin SDK/repository material inspected previously documents plugin entry points and detour helpers, but does not provide a contract that `plugin_unload` is called only after every game thread has left every plugin callback, stub, and trampoline. No general unload quiescence guarantee was established.

## Required unknowns before this can be considered

1. Exact firmware 14.02 export/NID/module identity and ABI for both suspend-all and resume-all.
2. Whether the API suspends all user threads in the current process, excludes the caller, and waits until every target is actually stopped before returning.
3. Behavior for threads created concurrently with the suspension operation.
4. Nesting, ownership, and concurrency semantics when multiple callers request suspension.
5. Whether resume-all resumes only threads suspended by the matching caller/operation or may disturb unrelated suspension state.
6. Behavior when a target thread is inside a syscall, signal handler, loader, allocator, or kernel callback.
7. Whether it is safe to call from a GoldHEN PRX callback context and whether GoldHEN's own management threads are in scope.
8. Whether all threads capable of executing the patched address are in the same pthread domain/process and covered by the primitive.
9. A safe, bounded recovery path if the patch operation fails while threads are suspended, including guaranteed resume and error telemetry without invoking unsafe library routines.
10. Separate proof that mappings remain stable and code publication/cache synchronization are correct. Thread suspension alone does not establish a mapping lease or cache-coherency contract.

## Engineering decision

- Do not add calls to `scePthreadSuspendAll` / `scePthreadResumeAll` in `main.c`, the detour adapter, or the host coordinator.
- Do not infer a successful rendezvous from a void-returning API call.
- Do not run a PS4 experiment merely to discover undocumented behavior; that would require a separate explicit, bounded hardware-test plan and authorization.
- Keep the live hook on its current upstream path and keep the NR-owned transaction core host-only.
- Continue with static evidence gathering: resolve exact exports and firmware ABI from trustworthy 14.02 artifacts, inspect relevant libkernel implementation/reverse-engineering evidence, and determine whether the primitive's documented semantics actually satisfy the install and removal contracts.
- If the semantics cannot be established, require an explicit cooperative per-title rendezvous or another proven mechanism; do not substitute a process-wide suspend guess.

## Consequence for the transaction state machine

The existing `PATCH_WINDOW_HELD` and `REMOVE_WINDOW_HELD` callbacks remain abstract contracts. The new API discovery narrows the research question but does not close either gate. The following remain separate:

- **Install exclusion:** no thread may fetch/execute any byte in the target span during a multi-byte patch.
- **Removal quiescence:** no new callback may enter and no in-flight execution may reference the target, hook, stub, or trampoline before reclamation.
- **Mapping stability:** the target mapping and readable span must remain valid across query, decode, write, and verification.
- **Code publication:** generated code and target changes must be visible to instruction fetch under a verified PS4 cache contract.

A global thread suspension primitive might be relevant to the first two, but only after its exact Orbis contract is proven. It does not solve the latter two.

## Sources

- OpenOrbis v0.5.4 header: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.4/include/orbis/libkernel.h
- OpenOrbis libkernel documentation: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.4/docs/MD/PS4%20Libraries/Libkernel.md
- FreeBSD 12.1-STABLE `pthread_suspend_all_np(3)`: https://manpage.me/?manpath=FreeBSD+12-CURRENT+and+Ports&q=pthread_suspend_all_np
- FreeBSD 14.0 `pthread_suspend_all_np(3)`: https://man.freebsd.org/cgi/man.cgi?apropos=0&manpath=FreeBSD+14.0-RELEASE+and+Ports&query=pthread_suspend_all_np&sektion=3
- PS4 Developer Wiki libkernel symbol index: https://www.psdevwiki.com/ps4/Libkernel
- GoldHEN public source-status note: https://github.com/GoldHEN/GoldHEN

## Change boundary

This audit changes documentation only. No executable source, workflow, PRX, PKG, plugin registration, or console state is changed. No PS4 call or hardware test was performed.
