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


## 5. Follow-up: 14.02 retail-binary corroboration and implementation gap

A second static pass examined the public `ps4debug-NG` firmware-support notes. Its README states that its author checked the 14.02 kernel and all three libkernel variants (ordinary, web, and system) against decrypted retail binaries, with comparisons to 14.00 and 13.52. It also notes that the ordinary and system libkernel variants retain their 13.52 offsets at 14.00, while the web variant has distinct pthread entry points. This is useful evidence that decrypted 14.02 libkernel variants and symbol-offset work exist in the public research ecosystem.

It does **not** publish, in the inspected README material, a 14.02 `scePthreadSuspendAll` / `scePthreadResumeAll` NID-to-address mapping, disassembly, wrapper body, or behavioral analysis. The README's references to pthread entry points concern thread creation support and do not establish suspend-all behavior. Therefore it does not close any of the unknowns in this audit.

A GitHub code search for the exact symbol names returned:
- OpenOrbis header declarations only;
- generic PS4/PS5 SDK symbol stubs or declaration lists;
- a PS5 clean-room/emulator implementation where `scePthreadSuspendAll` is explicitly unimplemented and `scePthreadResumeAll` delegates to a FreeBSD-named helper. That PS5 project is not evidence for PS4 retail firmware and is excluded from the implementation conclusion;
- no inspected PS4 retail implementation or firmware-specific disassembly for either function.

The FreeBSD 14.0 manual confirms the analog semantics more precisely: `pthread_suspend_all_np()` suspends active threads except the caller, and warns that the caller must avoid non-async-signal-safe functions until resume unless all threads are known to be at safe points. FreeBSD 14.2's `pthread_resume_all_np()` scans active threads and resumes those previously suspended. These are useful hazard models only. The Sony wrappers may differ in scope, ownership, wait behavior, nesting, and implementation; no equivalence is inferred.

### Updated evidence matrix

| Question | New evidence | Decision |
|---|---|---|
| Do decrypted 14.02 libkernel variants exist in public reverse-engineering work? | ps4debug-NG README reports comparison against decrypted retail 14.02 kernel and all three libkernel variants | Corroborated at project-report level |
| Exact 14.02 suspend-all/resume-all NIDs and module/library identity? | Not present in inspected README or SDK declarations | **BLOCKED** |
| Exact wrapper implementation / disassembly? | No PS4 retail body found in inspected public sources | **BLOCKED** |
| Per-thread stop acknowledgement and return semantics? | No PS4-specific contract or implementation evidence | **BLOCKED** |
| Resume ownership, nested/concurrent behavior, newly created thread coverage? | No PS4-specific evidence | **BLOCKED** |
| Consumer-side serializing handoff before patched instruction fetch? | No evidence of a per-thread callback or mandatory serialization on resume | **BLOCKED** |

### Revised next step

The useful next static task is now narrowly scoped: obtain a legally accessible, already-decrypted 14.02 `libkernel.sprx` variant and its export metadata from a trustworthy source, identify the exact export records for both functions, then inspect the wrapper implementation and any referenced libthr/kernel routines. This must remain offline/static analysis; do not download or execute unknown payloads, do not ask the user to dump firmware merely to advance a speculative lead, and do not run a console experiment without a separate bounded plan and explicit authorization.

Even a complete wrapper disassembly would still need to establish the underlying thread-domain coverage and consumer-side serialization. If the implementation simply delegates to an opaque kernel/libthr primitive with no such guarantee, the detour gate remains unsupported.


## 6. Follow-up: inspect the actual public 14.02 libkernel-offset consumer

The current `Pharaoh2k/ps4debug-NG` source was inspected directly, rather than relying only on its README.

In `kdebugger/source/proc.c`, the 14.00 and 14.02 cases are grouped for the ordinary `libkernel.sprx` and system `libkernel_sys.sprx` thread-creation helper offsets. The code also has a separate 14.00/14.02 case for `libkernel_web.sprx`. The symbols resolved by this table are only:

- `scePthreadAttrInit`
- `scePthreadAttrSetstacksize`
- `scePthreadCreate`
- `_thr_initial`

These offsets support creation of a worker thread in a target process. They are **not** offsets for `scePthreadSuspendAll` or `scePthreadResumeAll`, and the source contains no implementation or call site for either suspend-all function. Repository-scoped code searches for both exact Sony names and `pthread_suspend_all_np` returned no matches in this project.

This narrows the earlier evidence statement: the public project provides a concrete, source-level 14.02 reference for three libkernel variants' *thread-creation* entry points, but it does not provide the suspend/resume symbols or semantics needed by the detour gate. Its README's claim that 14.02 libkernel variants were checked against decrypted retail binaries is the project's own provenance statement; it is not a substitute for publishing the underlying binary, export table, or disassembly.

### Evidence matrix — source-level check

| Artifact | What it establishes | What it does not establish |
|---|---|---|
| ps4debug-NG README | Project reports checking 14.02 kernel and all three libkernel variants against decrypted retail binaries | No suspend-all NID, export identity, implementation, or behavioral contract |
| ps4debug-NG `kdebugger/source/proc.c` | Explicit 14.00/14.02 offsets for thread-creation helpers in ordinary, web, and system libkernel variants | No `scePthreadSuspendAll` / `scePthreadResumeAll` offsets or calls |
| Repository-scoped exact-name search | No suspend-all implementation in the inspected repository | Does not prove no such implementation exists elsewhere |
| OpenOrbis v0.5.4 header | Public declaration names and void/no-argument surface | No firmware-specific NID, wrapper body, or postcondition |

### Engineering consequence

Do not repurpose the thread-creation offsets as guessed suspend-all offsets, and do not call internal addresses inferred from neighboring symbols. The candidate remains **not approved**. The next useful artifact must specifically expose the 14.02 export records and code path for the two suspend/resume APIs (or an authoritative kernel/libthr implementation they delegate to). If that cannot be obtained from trustworthy, legally accessible static material, this branch of research should be marked unsupported and the project should proceed with a cooperative, title-specific rendezvous design rather than a speculative global suspend.

## 7. Change boundary

Documentation only. No third-party payload was downloaded, built, executed, or transferred. No firmware image or console dump was requested. No PRX, PKG, workflow, plugin configuration, or PS4 state was changed. No hardware test was performed.
