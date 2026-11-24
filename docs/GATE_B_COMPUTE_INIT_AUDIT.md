# Gate B — compute initialization and ABI cross-check

Date: 2026-11-24

## Scope and provenance

This follow-up cross-check compares the pinned OpenGNM source
`PS4-OpenGNM/opengnm@4b295ca54c82c83acf308d1c646a2dfa9ae57350`
with the independent Vulkan-PS4 implementation
`PS4-OpenGNM/vulkan-ps4@f4d940b723771b3fe637e8ac0704bb5d6ab19622`
and the currently published OpenGNM API reference. It is source/documentation
evidence only. None of these references is a Sony SDK ABI capture or a
retail PS4 Pro firmware 14.02 execution trace.

## New blocking finding: compute default state is not initialized by the reference path

In the pinned Vulkan-PS4 source:

- `vk_ps4_BeginCommandBuffer` initializes each primary command buffer with
  `sceGnmDrawCmdInitDefaultHardwareState(&cmd->gnm_cmd)`.
- The pinned OpenGNM implementation of that helper reserves 256 dwords and
  calls `sceGnmDrawInitDefaultHardwareState350`.
- That is the GFX 3.50 graphics default-state initializer. It is not the
  compute initializer `sceGnmDispatchInitDefaultHardwareState`.
- The Vulkan-PS4 compute path later binds a CS and emits a direct dispatch,
  but the inspected command-buffer begin path does not emit the compute
  default-state initializer.

This is a concrete gap in the independent source reference: its compute
recording path is present, but the inspected primary-command-buffer
initialization path only emits graphics default state. The reference therefore
cannot be treated as a validated minimal compute sequence, even at source
level. NR must not copy this initialization sequence as sufficient for
compute.

The exact firmware-facing `sceGnmDispatchInitDefaultHardwareState` ABI,
availability and required ordering on retail firmware 14.02 remain unverified.
A candidate harness must explicitly account for compute default state, but
must not call the firmware-facing initializer until its export, ABI and
command-buffer reservation semantics are verified for the target.

## ABI reference cross-check — useful signatures, not target qualification

The current OpenGNM API reference publishes these typed signatures:

- `sceGnmDispatchDirect(uint32_t* cmdbuf, uint32_t size, uint32_t threads_x,
  uint32_t threads_y, uint32_t threads_z, uint32_t flags)`
- `sceGnmSetCsShader(uint32_t* cmdbuf, uint32_t size,
  const uint32_t* cs_regs)`
- `sceGnmSubmitCommandBuffers(uint32_t count, void* const dcb_gpu_addrs[],
  uint32_t* dcb_sizes_in_bytes, void* const ccb_gpu_addrs[],
  uint32_t* ccb_sizes_in_bytes)`
- `sceGnmDispatchInitDefaultHardwareState(uint32_t* cmdbuf, uint32_t size)`
- `sceGnmComputeWaitOnAddress(uint32_t* cmdbuf, uint32_t size,
  uintptr_t addr, uint32_t mask, uint32_t cmp_func, uint32_t ref)`

These signatures help define a typed candidate interface and clarify that
command-builder size parameters are expressed in dwords in the published
dispatch/compute-wait APIs, while submit size arrays are in bytes. They do not
prove the exact retail firmware 14.02 export ABI, symbol availability,
calling behavior, return semantics, or reservation bounds. The published
reference describes the API surface but does not identify a 14.02 ABI capture.

The generic backend remains host-only and its submit path is explicitly a
no-op. A successful generic build or CTest run cannot validate any firmware
entry point.

## Updated minimal harness requirements

Before any hardware dispatch is authorized, the candidate must now include
all of the following, with each item separately evidenced:

1. Compute default-state initialization, with verified FW 14.02 symbol,
   signature, size units, required reservation, and ordering.
2. CS program address and resource registers, including the compute
   `SET_SH_REG` shader-type bit covered by PR #65.
3. CS user-data descriptor writes, GPU-visible addresses, alignment,
   residency and lifetime for shader, descriptor, output, DCB and completion
   storage.
4. One DCB and one in-flight submission only; no shared epilogue buffer or
   concurrent queue operation in the first harness.
5. Verified submit ABI and explicit error handling.
6. A completion mechanism with a finite wall-clock deadline, bounded polling
   or kernel event wait, timeout reporting and safe recovery. Do not use
   unbounded `QueueWaitIdle` polling.
7. A target-qualified GPU-to-CPU visibility/cache operation.
8. Full output-buffer readback, CPU reference comparison and SHA-256 recorded
   with console model, firmware/build, toolchain and artifact hashes.

## Gate B disposition

This audit closes the source-reference initialization question by identifying
an additional missing compute-state step in the independent Vulkan-PS4
implementation. It does not close the hardware/firmware questions.

Gate B remains **BLOCKED** until the exact FW 14.02 ABI and memory/residency
contracts are established and a controlled PS4 Pro run proves compute
completion and deterministic full-buffer readback. Gate A remains PASS
(artifact-only); Gate C remains NOT PASSED; Gate D remains NOT STARTED.

No console command buffer was submitted. No compute dispatch, PRX activation,
PKG installation or console-state change was performed.


## Additional compute memory, descriptor and visibility audit

Further inspection of the pinned Vulkan-PS4 source at
`PS4-OpenGNM/vulkan-ps4@f4d940b723771b3fe637e8ac0704bb5d6ab19622`
and OpenGNM at
`PS4-OpenGNM/opengnm@4b295ca54c82c83acf308d1c646a2dfa9ae57350`
reveals additional source-level blockers.

### 1. Direct-memory helper separates CPU mapping from the allocation offset

OpenGNM `sceGnmDirectMemoryAllocate` stores:
- `memory->mapped`: CPU virtual pointer returned by
  `sceKernelMapDirectMemory`;
- `memory->directmemory`: allocation offset returned by
  `sceKernelAllocateDirectMemory`.

The Orbis helper releases/unmaps these separately. In the pinned helper,
`directmemory` is otherwise not consumed by the production code outside
allocation/release; the only production allocation call site found in that
repository is VideoOut. The Vulkan-PS4 reference instead forms GNM buffer
addresses by casting/using `gnm_mem.mapped + offset`. That is an explicit
assumption that the CPU mapping is also the GPU-visible address. A separate
PS4 Mesa implementation provides precedent for a shared CPU/GPU VA design,
but neither source proves the translation/residency contract for our exact
resources on retail firmware 14.02. NR must track CPU VA, direct-memory
allocation offset, GPU address and residency as distinct fields until the
target ABI confirms their relationship.

### 2. Compute shader code is retained in the general allocator

The Vulkan-PS4 compute pipeline derives `cs_code` inside the compiled GNM
binary and passes it to `sceGnmCsRegsSetAddress`. It then retains the binary
with `vk_ps4_alloc` and later frees it with `vk_ps4_free`, explicitly because
the CS register structure contains an address into that allocation. The
inspected code does not place this shader allocation in a
`GnmDirectMemory` object, nor does it demonstrate GPU mapping, residency,
alignment or lifetime relative to in-flight dispatch. Retaining a CPU pointer
is necessary for lifetime but is not evidence that the code is a valid,
resident GPU allocation on FW 14.02.

### 3. Pointer descriptor tables are allocated as host-side arrays

The descriptor implementation allocates per-binding `buffers`,
`textures` and `samplers` arrays through `vk_ps4_alloc` /
`vk_ps4_free`. In its compute bind branch, pointer-based input-usage slots
are programmed with `sceGnmDrawCmdSetPointerUserData` using those arrays
directly. The inspected code does not copy these tables into direct memory or
otherwise prove that the host allocator's storage is GPU-addressable and
resident. This is a concrete source-contract gap, not just a missing
firmware-documentation citation. Any NR harness must put pointer tables in a
verified GPU-visible allocation and keep them alive until completion.

The same descriptor path builds buffer descriptors from
`gnm_mem.mapped + memory_offset + offset`, reinforcing the unverified
CPU-mapping-equals-GPU-address assumption described above.

### 4. Flush/invalidate and pipeline barrier implementations are not proof of visibility

In `vk_ps4_memory.c`, `vk_ps4_FlushMappedMemoryRanges` and
`vk_ps4_InvalidateMappedMemoryRanges` return success without performing an
operation. The comments claim Garlic write-combined and Onion CPU-coherent
behavior, but the source does not provide a firmware-14.02 cache-policy
qualification or a per-memory-type visibility proof.

In `vk_ps4_CmdPipelineBarrier`, the implementation emits
`GNM_CACHE_FLUSH_AND_INV_TS_EVENT` with address 0 and
`GNM_DATA_SEL_DISCARD`, then ignores source/destination stage masks and
memory/buffer barrier arrays. This can be recorded as a cache-flush event
candidate, but it is not evidence of a correctly ordered compute-write to
CPU-read visibility sequence for a particular output allocation. It also
does not perform a CPU invalidate operation after completion.

### 5. Queue wait failure is not fail-closed

In `vk_ps4_QueueSubmit`, if a GPU wait-semaphore epilogue submission fails,
the implementation checks a CPU flag but then continues even if the flag is
false; the comment explicitly says it proceeds to avoid deadlock. The host
fallback similarly proceeds when the semaphore is not signaled. This is not a
safe synchronization contract for a deterministic compute harness. NR must
treat a failed wait submission as a hard failure and must not dispatch work
whose dependency was not satisfied.

### Updated blocking checklist

The source-level reference now has at least these independently identified
gaps in addition to the missing compute default-state initialization and
firmware ABI:

1. FW 14.02 compute-init symbol, signature, reservation and ordering.
2. Compute user-data PM4 shader-type fix (PR #65, host-tested only).
3. GPU-visible shader-code allocation, address translation, alignment,
   residency and in-flight lifetime.
4. GPU-visible descriptor-table allocation and lifetime; host-heap pointers
   must not be assumed GPU-visible.
5. Resource/output GPU address and residency contract.
6. Submit ABI and fail-closed dependency handling.
7. Bounded completion and recovery.
8. Target-qualified GPU-to-CPU cache visibility, not no-op flush/invalidate.
9. Full output readback, CPU reference comparison and deterministic hash on
   the target PS4 Pro / firmware 14.02.

These findings make the independent Vulkan source useful as an audit target,
but unsuitable as a ready-made compute harness. They do not establish that a
PS4 compute dispatch is impossible; they establish that this source path has
not supplied the missing memory/visibility contracts. Gate B remains BLOCKED.


### 6. The generic compute initializer is deliberately non-representative

The pinned generic backend implementation of
`sceGnmDispatchInitDefaultHardwareState` checks for at least four dwords,
writes only `PKT3_CLEAR_STATE` plus a `PKT3_NOP`, and returns 4. The API
test merely checks that the call does not crash and explicitly allows a zero
return on generic. This is a host placeholder, not a captured or synthesized
GFX8/Neo compute-state sequence. It cannot be used to size, validate or
approve the retail compute preamble.

On the Orbis side, `driver_orbis.c` declares the firmware-facing
`sceGnmDispatchInitDefaultHardwareState(uint32_t*, uint32_t)` as an external
symbol. The source does not provide a local Orbis packet builder or a
firmware-specific reservation constant for this function. The declaration
therefore establishes only the expected symbol name and source-level
prototype; it does not show that the import resolves on firmware 14.02 or
define its actual write bound/return behavior. No NR call site has been added.

The documented API reference says the dispatch initializer should precede
dispatch, but that is OpenGNM documentation, not an FW-14.02 trace. The
candidate must continue to reserve and validate the actual returned dword
count on target, with a proven maximum reservation before any submission.



### 7. Independent GFX8 compute-init packet candidate found in shadPS4

A further cross-check was made against
`shadps4-emu/shadPS4@5c3b6910b99ea4e4d019533490137fe80e400306`,
`src/core/libraries/gnmdriver/gnmdriver.cpp`, function
`sceGnmDispatchInitDefaultHardwareState`, and
`src/core/libraries/gnmdriver/gnmdriver_init.h`.

The emulator's HLE implementation provides a concrete GFX8/Neo candidate:
- requires a reservation of `HwInitPacketSize = 0x100` dwords and returns
  that fixed size;
- emits compute-type `SET_SH_REG` writes for
  `COMPUTE_STATIC_THREAD_MGMT_SE0/SE1`;
- on Neo mode also writes `SE2/SE3`;
- writes `COMPUTE_RESOURCE_LIMITS = 0x170`;
- emits an `ACQUIRE_MEM` packet;
- pads the remaining reservation with a `NOP` packet.

This is materially more useful than OpenGNM Generic's four-dword
`CLEAR_STATE + NOP` placeholder: it gives NR an independently implemented
Neo-specific packet shape and a concrete 256-dword reservation candidate.
It also confirms that the compute-init packets must use the compute shader
type, not graphics.

However, shadPS4 is an emulator HLE implementation. Its source does not
establish that this exact sequence is byte-for-byte the retail Sony
implementation on firmware 14.02, nor that all return/reservation semantics
are identical on that firmware. The 256-dword value is therefore an
evidence-backed candidate for the isolated host contract and future target
probe, not permission to submit it to hardware. No initializer was copied
into NR's production dispatch path and no console test was performed.

Pinned source:
- https://github.com/shadps4-emu/shadPS4/blob/5c3b6910b99ea4e4d019533490137fe80e400306/src/core/libraries/gnmdriver/gnmdriver.cpp
- https://github.com/shadps4-emu/shadPS4/blob/5c3b6910b99ea4e4d019533490137fe80e400306/src/core/libraries/gnmdriver/gnmdriver_init.h

The target-specific gate remains blocked until a read-only FW 14.02
symbol/ABI check and an approved, isolated PS4 Pro validation establish the
reservation, emitted dword count, submit behavior and completion/readback
contract. The emulator source narrows the uncertainty; it does not close it.


### 8. Retail-title reverse-engineering provides a second compute submission model

A separate reverse-engineering report was inspected:
`HayatoG/tlg_pc@ee3327d7c1a1c107a16327d7c7a33c6c148b6dce`,
`analysis/02_rendering_gnm.md`, sections 2–3 and 6. It describes call sites,
imports and queue objects recovered from a PS4 title binary. This is not an
SDK specification and confidence labels in the report must be preserved.

The report identifies two distinct compute models:

**Graphics/command-buffer compute**
- `JsDeviceContext::Dispatch` calls `sceGnmDispatchDirect` on the DCB after
  `PrepareDispatch`.
- Compute work is also used for resource copies, clears and depth copies.

**Asynchronous compute queue**
- A `JsComputeContext` maps a compute queue through
  `sceGnmMapComputeQueueWithPriority`; the report identifies pipe 1, queue 4,
  priority 0 and a 0x400-dword ring for the device context.
- Three frame contexts are identified on pipe 2, queues 0/1/2, priority 1.
- A separate Gnmx `ComputeQueue` is mapped on pipe 0, queue 3, with a 1 MiB
  ring for dispatch-draw.
- The compute path calls `sceGnmDispatchInitDefaultHardwareState`,
  `sceGnmDispatchDirect`, then submits through Gnmx
  `ComputeQueue::submit` -> `sceGnmDingDong`; it unmaps with
  `sceGnmUnmapComputeQueue`.
- The report identifies `StallUntilSignaled` and `AsyncMap` as the
  synchronization/readback call sites. It does not recover a bounded timeout
  contract or prove a complete cache visibility sequence.

This is stronger than an emulator-only reference: it is evidence that a
shipping-title binary contains real compute queue setup, dispatch, doorbell,
unmap and CPU readback paths. It materially expands the Gate B design space:
NR must not assume that the DCB submit path is the only possible compute
submission model. A dedicated ACE/compute-queue harness is now a candidate
alternative, while the simpler DCB dispatch remains another candidate.

Limitations:
- The report is reverse engineering of a different title and firmware
  baseline; it is not a direct observation of NR or of firmware 14.02.
- The reported pipe/queue/ring values are title-specific and must not be
  copied into NR without establishing queue availability, privilege/context,
  GDS allocation, ring mapping, doorbell and lifetime requirements.
- It does not expose the exact full arguments or success/error semantics for
  all calls, nor a verified timeout/cache contract.
- The report's own confidence marks distinguish confirmed call/import
  evidence from probable vtable interpretations; probable entries are not
  promoted to facts here.

Pinned report:
https://github.com/HayatoG/tlg_pc/blob/ee3327d7c1a1c107a16327d7c7a33c6c148b6dce/analysis/02_rendering_gnm.md

**Design consequence:** before implementing an NR queue path, resolve whether
the first hardware proof should use (a) one DCB with `sceGnmSubmitCommandBuffers`
or (b) an ACE queue with `sceGnmMapComputeQueueWithPriority` /
`sceGnmDingDong`. The retail-title report makes (b) credible, but does not
supply the missing FW-14.02 ABI, queue ownership or completion contract.
No queue is mapped and no work is submitted by NR.


#### Queue path details cross-checked against the same title report

The report's `docs/TECNICO.md` at the same pinned revision independently
summarizes the submit order:
1. submit the frame's asynchronous compute ACB through
   `ComputeQueue::submit -> sceGnmDingDong`;
2. write the graphics EOP/flip sequence;
3. call `sceGnmSubmitAndFlipCommandBuffers` for the graphics DCB/CCB pair;
4. a separate thread waits on the EOP equeue and calls
   `sceGnmSubmitDone`.

It also identifies three in-flight frame contexts, each with its own
`JsComputeContext`, and states that the compute queue is rung before the
graphics submission. This gives a concrete ordering/lifetime model for a
shipping title, but it is not a drop-in single-dispatch harness: it couples
compute ACB, graphics DCB/CCB, flip, EOP and frame rotation. NR should not
recreate that larger mechanism merely to prove a single compute output.

For the minimal NR proof, the useful takeaway is the separation of:
- command generation (PM4 written into a queue/ring);
- queue submission/doorbell (`sceGnmDingDong` for ACE);
- completion signaling/wait;
- CPU readback.

The actual title report identifies an `AsyncMap` readback call site and
`StallUntilSignaled`, but does not establish their bounded timeout or cache
coherency implementation. These remain explicit acceptance criteria.


### 9. Compute-ring constraints cross-check (emulator model only)

The same pinned shadPS4 HLE source implements
`sceGnmMapComputeQueue` with the following validation:
- `pipe_id < Liverpool::NumComputePipes`;
- `queue_id < Liverpool::NumQueuesPerPipe`;
- ring base aligned to 256 bytes;
- ring size expressed in dwords and constrained to a power of two;
- read-pointer address aligned to 4 bytes;
- returned virtual queue ID is the internal queue index plus one.

Its `sceGnmDingDong(vqid, next_offs_dw)` model treats the second argument as
a write offset in dwords, detects wraparound and submits the ring segment(s).
Its `sceGnmDispatchDirect` HLE accepts exactly 9 dwords and writes a compute
`DISPATCH_DIRECT` packet followed by a three-dword NOP.

These checks align with the broad shape of the retail-title reverse-engineered
queue report and the pinned OpenGNM local 9-dword dispatch builder. They are
useful candidate host invariants, but they are still emulator validation
rules, not verified Sony firmware 14.02 contracts. In particular, NR must
not hard-code shadPS4's pipe/queue counts or infer that its emulated ring
submission and synchronization behavior matches the console.

Reference:
https://github.com/shadps4-emu/shadPS4/blob/5c3b6910b99ea4e4d019533490137fe80e400306/src/core/libraries/gnmdriver/gnmdriver.cpp


## Independent retail-title compute evidence: The Order: 1886 (2026-11-24)

A second public reverse-engineering report, for **The Order: 1886** retail eboot, independently corroborates that PS4 compute is used through both graphics command buffers and asynchronous compute queues. This is stronger evidence of real-title call patterns than an emulator implementation, but it is still not a firmware-14.02 ABI record or a Neural Rendering hardware test.

Pinned report:
- Repository: `saxons-kicks/theorder1886-port`
- Revision: `4270135b99a32788b2fa4429fe85b76f38e2d5c9`
- Relevant files: `docs/PROGRESS.md`, `src/import_names.inc`, and the port's pinned shadPS4 HLE implementation.
- References:
  - https://github.com/saxons-kicks/theorder1886-port/blob/4270135b99a32788b2fa4429fe85b76f38e2d5c9/docs/PROGRESS.md
  - https://github.com/saxons-kicks/theorder1886-port/blob/4270135b99a32788b2fa4429fe85b76f38e2d5c9/src/import_names.inc
  - https://github.com/saxons-kicks/theorder1886-port/blob/4270135b99a32788b2fa4429fe85b76f38e2d5c9/gpu/shadps4/core/libraries/gnmdriver/gnmdriver.cpp

### What the title report adds

- The report identifies a compute queue using `sceGnmMapComputeQueueWithPriority`, followed by `sceGnmDispatchInitDefaultHardwareState`, compute dispatch, queue submit and `sceGnmDingDong`.
- It describes a compute-queue `ReleaseMem` label used as a GPU write marker, and a compute-write watch on a page where dispatches bind two 192-byte storage buffers. This supports the existence of a GPU-written memory/label completion path in the title's runtime.
- The report distinguishes GPU write activity from CPU readback and tracks buffer downloads/readback stalls. This reinforces that GPU completion, CPU visibility/invalidation, and copying data to the CPU are distinct phases.
- Its source-level HLE has `sceGnmComputeWaitOnAddress` emit a `WAIT_REG_MEM` PM4 packet. The HLE accepts a command-buffer size of 14 dwords, emits a 3-dword leading NOP, a 7-dword wait packet and a 2-dword trailing NOP. This is an emulator's packet construction, not proof of retail ABI or timeout semantics. The packet itself is a GPU-side wait and supplies no bounded CPU timeout/recovery policy.
- The HLE `sceGnmDingDong` implementation serializes submissions and calls an emulator `WaitGpuIdle()` before submitting the ACB. That is emulator scheduling behavior and must not be copied as a retail host-side synchronization contract.
- The report's retail-title imports include `sceGnmDispatchInitDefaultHardwareState`; its import map and binary analysis corroborate the function's use in a commercial title, but do not establish exact calling convention, reservation units, or FW 14.02 export resolution.

### Consequences for NR

1. Retain a single-in-flight ACB/queue candidate as a possible future path, but do not select pipe/queue IDs, ring size, priority, or GPU VA from another title.
2. Keep queue map, command construction, doorbell/submit, GPU completion, CPU cache visibility, and CPU readback as separately testable steps.
3. Do not treat `WAIT_REG_MEM` as a bounded timeout. Any CPU wait around a completion label/event must have an explicit monotonic deadline, finite polling/backoff, cancellation/error propagation, and recovery that does not leave an in-flight buffer reusable.
4. Do not use the emulator's synchronous `WaitGpuIdle()` behavior as evidence of retail queue semantics.
5. Do not assume the title's ReleaseMem label or memory placement is directly reusable. Its address space, cache policy, queue lifetime, and firmware context are not established for NR/FW 14.02.

### Evidence classification

| Claim | Classification |
|---|---|
| A commercial PS4 title uses compute queue mapping, compute init, dispatch, `sceGnmDingDong`, and GPU-written completion/readback-related paths | Reverse-engineering report of retail-title binary |
| `sceGnmComputeWaitOnAddress` packet shape and emulator wait behavior | shadPS4 HLE only |
| Exact Sony FW 14.02 ABI, queue allocation contract, timeout semantics, GPU residency/cache rules | Not established |
| NR compute execution on PS4 Pro | Not performed |

This evidence narrows the architectural choices but does not close Gate B.
