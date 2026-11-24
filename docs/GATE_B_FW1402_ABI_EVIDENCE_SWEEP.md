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
