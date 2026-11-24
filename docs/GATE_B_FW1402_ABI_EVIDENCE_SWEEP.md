# Gate B — firmware 14.02 ABI evidence sweep

Date: 2026-11-24

## Objective

Determine whether public SDK headers, OpenOrbis declarations, OpenGNM's published
API reference, emulator HLEs, or public reverse-engineering reports provide a
firmware-14.02-qualified ABI for the first compute path. This is a read-only
source/documentation audit. It does not authorize console probing or dispatch.

## Evidence classification

| Evidence | What it establishes | What it does not establish |
|---|---|---|
| OpenOrbis `GnmDriver.h` at `0a1aaf9dd4a92695538bdeb09fb056d06dd11725` | Names exports including `sceGnmDispatchDirect`, `sceGnmDispatchInitDefaultHardwareState`, `sceGnmMapComputeQueueWithPriority`, `sceGnmDingDong`, `sceGnmUnmapComputeQueue`, and `sceGnmSubmitCommandBuffers`. | Most compute declarations are old-style empty-parameter `void name();` declarations. They do not encode argument count/types, return type, calling convention, reservation units, or firmware version. |
| OpenGNM pinned source/API docs | Typed candidate signatures and local PM4 builder behavior; Orbis backend forwards selected calls to firmware exports. | No exact import/NID or ABI capture tied to retail FW 14.02; no proof the target export resolves or behaves as documented. |
| shadPS4 HLE pinned source | Emulator model for compute init, queue mapping, doorbell, dispatch, and compute wait packet generation. | Not Sony firmware; not a retail FW 14.02 trace or ABI record. |
| Reverse-engineered shipping-title report | Evidence that a commercial PS4 title uses compute dispatch, ACE queue mapping/doorbell, and EOP/event-based synchronization. | Different title/firmware baseline; incomplete exact ABI and no NR hardware result. |
| Public FW 14.02 kernel-offset references | Some kernel payload projects explicitly identify FW 14.02 kernel build/offset support. | Kernel offsets do not identify `libSceGnmDriver` export NIDs, prototypes, GPU driver semantics, or GNM compute ABI. |

## Specific conclusions

1. The inspected OpenOrbis header is not a typed ABI source for the compute
   functions. For example, `sceGnmDispatchDirect()`,
   `sceGnmDispatchInitDefaultHardwareState()`,
   `sceGnmMapComputeQueueWithPriority()`, and `sceGnmDingDong()` are
   declared as empty-parameter `void` functions. This is insufficient to
   safely call them from NR and must not be treated as authoritative ABI.

2. OpenGNM's public reference gives useful candidate signatures:
   - `sceGnmDispatchDirect(uint32_t*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)`
   - `sceGnmDispatchInitDefaultHardwareState(uint32_t*, uint32_t)`
   - `sceGnmMapComputeQueueWithPriority(uint32_t, uint32_t, uintptr_t, uint32_t, uint32_t*, uint32_t)`
   - `sceGnmDingDong(uint32_t, uint32_t)`
   - `sceGnmComputeWaitOnAddress(uint32_t*, uint32_t, uintptr_t, uint32_t, uint32_t, uint32_t)`

   These are library-maintained declarations, not a Sony SDK header or a
   firmware-14.02 import capture. Their use in a target binary still requires
   symbol resolution and calling-convention verification.

3. The OpenOrbis header's typed declarations for
   `sceGnmSubmitCommandBuffers` and `sceGnmSubmitDone` are more useful than
   its compute declarations, but they still do not qualify export availability,
   argument semantics, or completion behavior on FW 14.02.

4. Public evidence found for FW 14.02 kernel support concerns kernel offsets
   and payload compatibility, not the GNM userland driver ABI. No inspected
   public source provided a 14.02-specific `libSceGnmDriver` NID table,
   import stub/prologue, symbol-resolution trace, or retail call trace for the
   compute functions above.

5. Therefore neither the DCB compute route nor the ACE queue route can yet be
   selected as the NR hardware path solely from public source. The title
   reverse-engineering report makes ACE credible; OpenGNM's local DCB packet
   builders make DCB credible. Neither route has the target ABI and memory
   contracts needed for a safe dispatch.

## Required evidence to close this ABI sub-gate

A read-only, target-specific evidence package must identify, for the chosen
route:

- exact FW 14.02 `libSceGnmDriver` export/NID or import resolution for each
  called function;
- function prototype, PS4 calling convention, return/error semantics and
  command-buffer size units;
- compute-init reservation and actual emitted dword count;
- submit/doorbell ownership and ordering;
- GPU virtual-address mapping, alignment, residency and resource lifetime;
- completion primitive, finite timeout, failure handling and CPU-visible
  readback/cache operation.

A symbol name in a header or an emulator implementation is not sufficient.
Any target probe must be explicitly approved, read-only until the isolated
compute harness is separately authorized, and must not change unrelated
console state.

## Gate B disposition

This sweep closes the public-source ABI search subtask, not Gate B itself.
The public sources examined do not supply a firmware-14.02-qualified compute
ABI. The remaining requirement is target-specific evidence and a controlled
PS4 Pro compute result with deterministic full-buffer readback.

Gate B remains **BLOCKED**. Gate A remains PASS (artifact-only); Gate C
remains NOT PASSED; Gate D remains NOT STARTED.

No console interaction, command-buffer submission, compute dispatch, PRX
activation, PKG installation, or console-state change was performed.


## New finding: public retail-title import maps expose candidate GNM NIDs

A deeper lookup of public PS4 title import-name maps found a concrete set of
NID-to-symbol mappings for the GNM driver. The mappings are present in both
the public `theorder1886-port` import-name database at revision
`4270135b99a32788b2fa4429fe85b76f38e2d5c9` and the
`HayatoG/tlg_pc` port import-name database at revision
`ee3327d7c1a1c107a16327d7c7a33c6c148b6dce`.

| Function | Candidate NID | Evidence |
|---|---|---|
| `sceGnmDispatchInitDefaultHardwareState` | `nF6bFRUBRAU` | Both title import maps |
| `sceGnmMapComputeQueue` | `29oKvKXzEZo` | Both title import maps |
| `sceGnmMapComputeQueueWithPriority` | `A+uGq+3KFtQ` | Both title import maps |
| `sceGnmUnmapComputeQueue` | `ArSg-TGinhk` | Both title import maps |
| `sceGnmDingDong` | `bX5IbRvECXk` | Both title import maps |
| `sceGnmSubmitDone` | `yvZ73uQUqrk` | Both title import maps |
| `sceGnmSubmitCommandBuffers` | `zwY0YV91TTI` | TLG port import map |

These are useful new identifiers for a **read-only target symbol census**:
if the target's loaded `libSceGnmDriver` can be inspected safely, a probe
can record module identity and resolve the candidate NIDs without invoking
the compute functions. This narrows the symbol-discovery task substantially.

Important limitations:

- These NIDs are derived from public retail-title import maps and public
  symbol-name databases, not from a Sony FW 14.02 SDK package or a captured
  FW 14.02 export table.
- The two port repositories may share upstream title/import metadata; their
  agreement is corroboration of the mapping data, not two independent
  firmware measurements.
- NID presence in a title import map does not prove that the target firmware
  exports the symbol, that the module is loaded in the same state, or that the
  function prototype/calling convention is correct.
- `sceGnmDispatchDirect` is present in a title import-name list but no
  corresponding NID mapping was established in the inspected mapping excerpt.
  Do not infer or guess its NID.
- No function was called, no symbol was resolved on the user's console, and
  no hardware probe was run in this session.

### Prepared read-only target census sequence

If/when the user's existing approved read-only PS4 evidence channel is
available, the next safe target-side step is limited to:

1. Read firmware/build identity and enumerate loaded kernel modules.
2. Identify the exact loaded module name for the normal or Neo GNM driver.
3. Record module handle, segment base/size and module identity using the
   existing read-only kernel module-info APIs.
4. Resolve the candidate NIDs above only if the supported resolver API can be
   used without loading a module or changing console state; record success or
   failure and resolved addresses.
5. Do not call any resolved GNM function, submit command buffers, map compute
   queues, allocate GPU resources, activate PRX, or install a package.
6. Redact console identifiers and network details from the evidence artifact.

This read-only census can establish symbol availability and module identity.
It still cannot prove GPU VA/residency, completion, cache visibility, or
compute correctness. Those remain separate Gate B requirements and require
an explicitly authorized isolated hardware validation.

### Updated disposition

The public-source ABI subtask now has concrete candidate NIDs for the
compute initializer and ACE queue/doorbell lifecycle, rather than names alone.
This is a meaningful narrowing of the remaining target probe. It is **not**
a firmware-14.02 ABI qualification and does not authorize a dispatch.

Gate B remains **BLOCKED** pending target-side read-only symbol evidence,
memory/residency and synchronization contracts, and a controlled PS4 Pro
compute result with deterministic full-buffer readback.
