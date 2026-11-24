# Gate B — Neo-mode GNM provider and read-only census correction

Date: 2026-11-24  
Branch: `engineering/opengnm-compute-userdata-pm4`  
Scope: source-only audit; no target probe or console interaction.

## Executive finding

The earlier target-census outline searched only for `libSceGnmDriver`. That is incomplete for PS4 Pro titles running in Neo mode. New public evidence identifies a distinct provider, `libSceGnmDriverForNeoMode`, and shows that some compute-related imports can be supplied by that variant rather than the normal driver.

The read-only census specification is therefore corrected: enumerate loaded modules once, inspect **both** GNM providers when present, and resolve each candidate symbol against each already-loaded provider. Do not load either module. Do not assume every PS4 Pro process is currently using Neo mode; provider selection is title/runtime-mode dependent.

## Evidence

### 1. Distinct normal and Neo-mode providers

The OpenGNM rewrite plan's firmware layout explicitly lists both:

- `libSceGnmDriver.sprx` — normal GNM driver;
- `libSceGnmDriverForNeoMode.sprx` — NEO/Pro variant.

This is a project-maintained firmware-layout reference, not a dump of the user's firmware 14.02.  
Reference: https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/OPENGNM_REWRITE_PLAN.md

A separate PS4 Pro software changelog states that games running in Neo mode use the different graphics driver `libSceGnmDriverForNeoMode`. Its reported hardware test was firmware 11.02, so it corroborates the provider distinction but does not qualify firmware 14.02.  
Reference: https://store.brewology.com/ps4/ps4-ambilight/changelog/

### 2. Retail-title import evidence and a newly identified dispatch NID

The AnyPS5 issue for inFAMOUS Second Son (CUSA00223, update 01.07) reports that 47 imports recorded under the title's `libSceGnmDriver` import library are supplied only by AnyPS5's `libSceGnmDriverForNeoMode` export implementation. Its list includes:

- `sceGnmDispatchDirect` — candidate NID `0BzLGljcwBo`;
- `sceGnmMapComputeQueueWithPriority` — candidate NID `A+uGq+3KFtQ`.

The issue separately says the title's import metadata names `libSceGnmDriver`, not `libSceGnmDriverForNeoMode`; the distinction above is about which provider the AnyPS5 implementation currently supplies, not a claim that the title's import table names the Neo module.

This adds a candidate NID for `sceGnmDispatchDirect`, which was not established in the earlier public-source sweep. The earlier candidate NIDs remain:

| Function | Candidate NID | Provenance |
|---|---|---|
| `sceGnmDispatchDirect` | `0BzLGljcwBo` | AnyPS5 CUSA00223 import audit |
| `sceGnmDispatchInitDefaultHardwareState` | `nF6bFRUBRAU` | Earlier two public title import maps |
| `sceGnmMapComputeQueue` | `29oKvKXzEZo` | Earlier two public title import maps |
| `sceGnmMapComputeQueueWithPriority` | `A+uGq+3KFtQ` | Earlier maps; also AnyPS5 CUSA00223 |
| `sceGnmUnmapComputeQueue` | `ArSg-TGinhk` | Earlier two public title import maps |
| `sceGnmDingDong` | `bX5IbRvECXk` | Earlier two public title import maps |
| `sceGnmSubmitDone` | `yvZ73uQUqrk` | Earlier two public title import maps |
| `sceGnmSubmitCommandBuffers` | `zwY0YV91TTI` | Earlier TLG import map only |

Sources:
- https://github.com/boykopovar/AnyPS5/issues/17581
- https://arjixwastaken.github.io/anyps5-catalog/titles/CUSA00223/

**Limitations:** the AnyPS5 export split is an emulator implementation/import-audit result, not a Sony export-table capture from the user's firmware. Title imports and NID maps do not prove that a symbol is exported by either provider on retail FW 14.02, nor do they provide a complete function ABI. Do not infer NIDs for functions absent from this table.

### 3. Kernel module-census API shape

The public ps4dev PS4 SDK headers expose these declarations:

```c
int sceKernelGetModuleList(SceKernelModule *array,
                           size_t size,
                           size_t *available);
int sceKernelGetModuleInfo(SceKernelModule handle,
                           SceKernelModuleInfo *info);
int sceKernelDlsym(SceKernelModule handle,
                  const char *symbol,
                  void **address);
```

Its `SceKernelModuleInfo` layout contains a size field, a 256-byte module name, four segment records, a segment count and a 20-byte fingerprint. The segment records contain address, size and protection.

References:
- https://github.com/ps4dev/ps4sdk/blob/master/include/sce/kernel.h
- https://github.com/ps4dev/ps4sdk/blob/master/include/sce/types/kernel.h

The pinned shadPS4 HLE implementation models `sceKernelGetModuleList`'s second argument as a handle-array element capacity and `sceKernelDlsym` as a lookup against the specified already-known module handle. This is emulator behavior, not a verified retail FW 14.02 contract. The SDK header's `size` parameter unit and exact FW 14.02 structure/version semantics must be confirmed before a target implementation relies on them.

The existing DolphinPS4 probe also demonstrates module enumeration, module-info lookup and name-based `sceKernelDlsym`, but its fallback that calls `sceKernelLoadStartModule` is explicitly excluded from this census.

## Corrected read-only census procedure

1. Record firmware/build identity using the already-approved read-only evidence channel.
2. Enumerate currently loaded module handles using a capacity whose units and bounds are verified for the target API. Handle insufficient-capacity/error returns explicitly; never assume a truncated list is complete.
3. Query module info for each returned handle. Validate the reported structure size and ensure `segmentCount` does not exceed the four entries in the public structure before reading segment data.
4. Match both provider names independently:
   - `libSceGnmDriver`;
   - `libSceGnmDriverForNeoMode`.
   Do not treat absence of one as an error if the other is present.
5. If neither provider is already loaded, report `MODULE_NOT_PRESENT` and stop. Do not load a module.
6. For each provider found, attempt name-based lookup only for the following symbols, recording each result independently:
   - `sceGnmDispatchInitDefaultHardwareState`;
   - `sceGnmDispatchDirect`;
   - `sceGnmSubmitCommandBuffers`;
   - `sceGnmSubmitDone`;
   - `sceGnmMapComputeQueue`;
   - `sceGnmMapComputeQueueWithPriority`;
   - `sceGnmDingDong`;
   - `sceGnmUnmapComputeQueue`.
7. Record module name, handle, fingerprint, segment count, symbol lookup success/failure, and (when safely derivable) the resolved address as a module-relative offset. Redact console identifiers and unrelated absolute address data from the published artifact.
8. Do not pass guessed NIDs to `sceKernelDlsym`. The cited existing probe uses name-based resolution; NIDs remain provenance for title import identities until a target-supported NID lookup method is separately established.
9. Do not call any resolved GNM function. Do not map queues, allocate DirectMemory, write command buffers, submit work, load/unload modules, activate PRX, install PKG, or change unrelated console state.

## What this census can and cannot close

A successful census can establish, for the running process and firmware, which GNM provider module(s) are already loaded and whether the listed names resolve in each. It can materially narrow the provider/entry-point uncertainty.

It cannot establish:
- the exact prototype/calling convention or return semantics;
- whether a resolved function is safe to call in the current caller context;
- command-buffer reservation units or emitted dword bounds;
- GPU VA translation, residency, alignment or lifetime;
- queue ownership, submit/doorbell ordering or completion;
- cache visibility or deterministic compute output.

Those still require separate firmware-qualified evidence and an explicitly approved isolated hardware test.

## Gate B disposition

This is a concrete correction to the target evidence plan and a new candidate NID for `sceGnmDispatchDirect`. It does not close the ABI or hardware sub-gates.

Gate B remains **BLOCKED** pending target-specific FW 14.02 provider/symbol evidence, verified memory and synchronization contracts, and a controlled PS4 Pro compute run with deterministic full-buffer readback. Gate A remains PASS (artifact-only); Gate C remains NOT PASSED; Gate D remains NOT STARTED.

No target probe, GNM call, command submission, compute dispatch, PRX activation, PKG installation or console-state change was performed.


## Existing NR read-only workflows do not provide a live module census

The current NR evidence paths were inspected to determine whether the proposed census can be collected without adding target-side code:

- `.github/workflows/ps4-installed-app-readonly-audit.yml` runs after a successful `PS4 Control Plane Preflight` on the self-hosted Windows PS4-LAN runner. It uses FTP to list the installed NR app directory and retrieve the installed PKG, `param.sfo`, `eboot.bin`, and `ps4_neural_engine.prx`. It hashes those files and uploads a read-only artifact. It does not execute code in the PS4 process, enumerate loaded modules, call `sceKernelGetModuleInfo`, or resolve GNM symbols.
- `docs/PS4_WINDOWS_BRIDGE.md` describes the control-plane preflight as FTP LIST/GETSIZE/GET plus HTTP GET-only fingerprinting. It explicitly does not issue payloads, POST requests, BinLoader operations, PRX activation, or remote file writes.
- The installed-app audit is therefore useful for confirming which NR binary is installed, but it cannot return the live module handles or loaded GNM provider state required by this census.

The existing read-only workflow is **not** a hidden runtime probe and must not be represented as one. No suitable checked-in target-side module-census implementation was found in the inspected PR #63/PR #64 source context. Implementing one would require a separately reviewed target-side executable/PRX or another explicitly supported runtime inspection channel. Do not add or activate such code as part of this source-only audit.

This closes the search for an already-existing NR runtime census path. The smallest next target-side deliverable, if separately approved, is a bounded read-only module inventory using the APIs above, with both normal and Neo-mode GNM provider names and symbol lookups. It must not invoke any GNM function or allocate GPU memory.


## Cross-check: OpenGNM has a resolver, but it is not a census

The pinned OpenGNM `src/driver_orbis.c` contains `resolvegnmdriversym` and `opengnm_is_hle_runtime`. Both enumerate module handles with `sceKernelGetModuleList`, initialize `OrbisKernelModuleInfo.size = sizeof(info)`, and query module info. The resolver uses `strstr(info.name, "GnmDriver")`, so the match can include either the normal or Neo-mode provider. It tries name-based `sceKernelDlsym` on the first matching GNM module, then falls back to all loaded modules if the symbol is not found.

This confirms a practical source-level calling pattern and reinforces that a resolver should not hardcode the normal provider. It does **not** provide the required census artifact:
- it returns the first matching symbol pointer rather than recording every provider and every symbol result;
- it does not report both GNM modules independently;
- its fallback searches every module, obscuring which provider supplied a symbol;
- the failure diagnostics call `submitlog`, which appends to `/data/eden_ps4_runtime.log`, so the helper is not strictly read-only if those diagnostics execute;
- its fixed 128-handle array and `uasize` call are implementation choices, not a firmware-14.02-verified maximum or API-size contract.

Therefore this resolver is useful as an API-usage reference only. Do not copy it unchanged into the target census. A new census must use a bounded enumeration strategy, avoid file writes/logging, record both provider modules independently, and report all lookup results without invoking the resolved GNM functions.

Pinned source:
https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/src/driver_orbis.c


## New emulator-side Neo export work: useful negative evidence, not retail ABI

A public AnyPS5 commit dated 2026-11-19, `ab01341df0d5204d1de48d33d09421039aab2d4e`,
adds 28 remaining `libSceGnmDriverForNeoMode` calls while explicitly describing
its behavior as matching PS4 firmware **10.71**. Its summary includes workload
submits and compute-queue mapping. The associated change set touches the Neo
export table, typed API declarations, packet tests, retail-title tests, and a
debt note:
https://repo.or.cz/boykopovar-AnyPS5.git/commit/ab01341df0d5204d1de48d33d09421039aab2d4e

This is a useful new cross-check that the Neo provider's compute-queue and
submit surface is sufficiently distinct to require separate auditing. It is
also a strong reason **not** to transplant those implementations into NR:
the commit names 10.71 as its behavioral reference, is an emulator/HLE
implementation, and does not provide a 14.02 Sony module export capture or
PS4 Pro hardware validation. The associated AnyPS5 issue describes some
remaining Neo submit/queue-map paths as refusal-code behavior because that
emulator path does not consume a GNM command buffer; that is not evidence of
a successful retail queue submission:
https://github.com/boykopovar/AnyPS5/issues/17581

Classification:
- Confirms additional emulator implementation activity for Neo provider.
- Reinforces need to inspect normal and Neo providers separately.
- Does not prove FW 14.02 exports, ABI, queue semantics, or compute execution.
- No NR code or target behavior changed as a result.
