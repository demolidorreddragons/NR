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
