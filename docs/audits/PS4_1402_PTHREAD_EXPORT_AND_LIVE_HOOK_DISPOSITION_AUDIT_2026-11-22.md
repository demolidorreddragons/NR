# PS4 14.02 pthread export evidence and live-hook disposition audit

**Date:** 2026-11-22  
**Repository:** `demolidorreddragons/NR`  
**Branch:** `feature/gcn8-numeric-kernels`  
**Baseline HEAD:** `d6ee4f3a3fcd20e27425dd24b534ec984c61e062`  
**Scope:** public static evidence review and current-source consistency check  
**Disposition:** suspend-all remains blocked; current generic PRX still installs the unvalidated SDK detour

## 1. Executive decision

The new public-source pass confirms that the names `scePthreadSuspendAll` and `scePthreadResumeAll` appear in public PS4 libkernel symbol indexes and in OpenOrbis declarations. It does **not** produce a firmware-14.02 export record, NID, module identity, wrapper address, disassembly, or behavioral contract. Public 14.02 research from ps4debug-NG reports checks against decrypted retail binaries, but the inspected source only publishes offsets for thread-creation helpers—not suspend/resume-all.

The candidate remains **not approved** as a transaction gate. No implementation or runtime experiment is justified by this evidence.

A separate source-consistency finding is important: the current `platform/ps4/goldhen/plugin/main.c` still invokes `Detour_DetourFunction(..., DetourMode_x64)` in `plugin_load()` for every non-Neural-Rendering title after the title-ID check. That is the upstream SDK's 14-byte x64 detour path. The cooperative-rendezvous design report correctly says no title participant protocol exists and that the existing SDK detour has not become safe, but the source has not yet been changed to enforce a fail-closed policy. This audit records that discrepancy; it does not silently claim the hook is disabled.

## 2. Evidence findings

### 2.1 PS4 Developer Wiki symbol catalog

The public PS4 Developer Wiki libkernel symbol index lists both `scePthreadSuspendAll` and `scePthreadResumeAll`, adjacent to individual suspend/resume and other pthread symbols.

This is symbol-name evidence only. The inspected page does not provide a firmware-14.02 NID/export tuple, module variant, address, function body, ABI details, or postcondition. A catalog entry cannot prove that a retail 14.02 module exports the function under a specific NID or that the function covers every thread in the process.

### 2.2 OpenOrbis declaration surface

The OpenOrbis libkernel header declares:

```c
void scePthreadResume();
void scePthreadResumeAll();
void scePthreadSuspend();
void scePthreadSuspendAll();
```

The declarations carry empty comments and do not document parameters, result values, timeout, ownership, wait semantics, or thread coverage. The header is a toolchain declaration, not a Sony firmware implementation contract.

### 2.3 Public 14.02 reverse-engineering project

The ps4debug-NG README reports comparison of the 14.02 retail kernel and all three libkernel variants (ordinary, web, system) against decrypted retail binaries. Its inspected `kdebugger/source/proc.c` publishes 14.00/14.02 offsets for:

- `scePthreadAttrInit`
- `scePthreadAttrSetstacksize`
- `scePthreadCreate`
- `_thr_initial`

Those are thread-creation helpers. They are not suspend/resume-all exports and cannot be repurposed as guessed offsets. The inspected source contains no suspend-all implementation or call site. The repository's provenance statement is useful evidence that binary analysis was performed, but does not substitute for the absent export records and disassembly.

### 2.4 Similar implementations excluded

A GitHub exact-name search surfaced PS5 emulator/clean-room code and generic SDK stubs. These are excluded from PS4 retail conclusions. In particular, an emulator stub or PS5 wrapper does not establish PS4 firmware 14.02 behavior.

### 2.5 FreeBSD analogue

FreeBSD's `pthread_suspend_all_np` documentation remains a hazard reference only. Its semantics cannot be assigned to Sony's `scePthreadSuspendAll` wrapper without implementation-level evidence. No alias/equivalence is inferred.

## 3. Current NR source consistency finding

The cooperative rendezvous design at `docs/audits/NR_COOPERATIVE_TITLE_RENDEZVOUS_DESIGN_2026-11-22.md` states that the generic PRX has no participant registry, title safe points, thread-lifecycle mediation, per-thread acknowledgement, or consumer-side handoff.

The current `platform/ps4/goldhen/plugin/main.c` confirms those absences, but also confirms that:

1. `plugin_load()` calls `install_gnm_hook()` for every process not identified as either Neural Rendering TITLE_ID.
2. `install_gnm_hook()` calls `Detour_Construct(..., DetourMode_x64)` and `Detour_DetourFunction(...)`.
3. `plugin_unload()` joins only the PRX's own input worker before `remove_gnm_hook()`.
4. There is no title adapter or validated rendezvous before install/removal.

Therefore the source still has a live generic SDK detour despite the design audit's conclusion that no safe generic rendezvous exists. This is not evidence that the hook has failed on hardware; it is a mismatch between the documented safety disposition and the code path. The SDK's unchecked protection/restore/cleanup behavior and missing unload quiescence remain as previously audited.

**Required disposition:** do not represent the current generic hook as safe or rendezvous-protected. A follow-up engineering change should fail closed for titles without an explicitly integrated adapter, or replace the hook with a supported title/plugin API. That change needs its own focused diff and host/source tests. No PS4 runtime test is implied.

## 4. Updated evidence matrix

| Gate / question | Evidence found | Result |
|---|---|---|
| Public PS4 symbol names | Developer Wiki lists suspend-all and resume-all names | Names corroborated; no firmware identity |
| OpenOrbis API declaration | Void/no-argument declarations with empty comments | Surface only |
| Exact FW 14.02 NID/module/export tuple | Not present in inspected sources | **BLOCKED** |
| FW 14.02 wrapper address/body/disassembly | Not present in inspected sources | **BLOCKED** |
| Per-thread stop completion and coverage | No PS4-specific implementation contract | **BLOCKED** |
| Resume ownership/nesting/concurrency/new-thread semantics | No PS4-specific implementation contract | **BLOCKED** |
| Per-consumer instruction serialization on release | No evidence found | **BLOCKED** |
| Generic title participant rendezvous | No participant adapter in current PRX | **BLOCKED** |
| Current generic hook disabled in source | `plugin_load()` still installs SDK x64 detour outside NR title IDs | **NOT SATISFIED** |

## 5. Engineering decision and next step

- Do not call `scePthreadSuspendAll` or `scePthreadResumeAll` in the PRX or transaction adapter.
- Do not infer a NID/address from symbol names, adjacent offsets, PS5 code, or FreeBSD behavior.
- Keep the host transaction core separate from the live SDK hook.
- Resolve the source/documentation mismatch with a fail-closed title-adapter gate before claiming generic detour safety.
- Continue only with a trustworthy, legally accessible 14.02 export table and wrapper body if such material becomes available. Even then, the wrapper must be traced to the underlying primitive and its thread coverage, wait semantics, ownership, and consumer-side serialization.
- If no supported title adapter can be integrated, use a supported title/plugin API or leave the hook disabled for that title.

## 6. Sources

- PS4 Developer Wiki libkernel symbol index: https://www.psdevwiki.com/ps4/Libkernel
- OpenOrbis libkernel header: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/include/orbis/libkernel.h
- ps4debug-NG README and firmware-support notes: https://github.com/Pharaoh2k/ps4debug-NG/blob/master/README.md
- ps4debug-NG source thread-creation offset consumer: https://github.com/Pharaoh2k/ps4debug-NG/blob/master/kdebugger/source/proc.c
- FreeBSD 14.0 analogue documentation: https://man.freebsd.org/cgi/man.cgi?apropos=0&manpath=FreeBSD+14.0-RELEASE+and+Ports&query=pthread_suspend_all_np&sektion=3

## 7. Change boundary

Documentation only. No executable source, PRX, PKG, workflow, plugin registration, console configuration, or PS4 state changed. No firmware binary or third-party payload was downloaded or executed. No hardware test was performed. The existing source-level hook path remains present and is explicitly recorded above; no package is approved for installation by this audit.
