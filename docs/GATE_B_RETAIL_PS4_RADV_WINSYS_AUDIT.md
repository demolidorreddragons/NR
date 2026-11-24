# Gate B — independent retail-PS4 RADV winsys evidence

Date: 2026-11-24

## Purpose and evidence class

This note records a newly identified independent PS4 homebrew graphics stack:
`iHaiDeeZ/Crema` and its shared PS4 RADV/GNM ancestry (`DolphinPS4`,
`ps4-kit`). The public project description says RADV is adapted to submit
work directly to the PS4 AMD GPU. Crema's README explicitly says only the
original PS4 has been tested so far; PS4 Pro reports are requested. This is
therefore useful retail-PS4 implementation evidence, but **not** evidence for
the user's PS4 Pro or firmware 14.02.

Sources:
- Crema README: https://github.com/iHaiDeeZ/Crema
- Crema FAQ: https://github.com/iHaiDeeZ/Crema/blob/main/FAQ.md
- DolphinPS4 PS4 RADV winsys source:
  https://github.com/iHaiDeeZ/DolphinPS4/blob/879b3e57f3d3180bcb5f2234db3903688b28cd7d/port/mesa/ac_ps4_drm.c
- DolphinPS4 Mesa integration patch:
  https://github.com/iHaiDeeZ/DolphinPS4/blob/879b3e57f3d3180bcb5f2234db3903688b28cd7d/patches/mesa-ps4.patch
- Crema PS4 RADV winsys:
  https://github.com/iHaiDeeZ/Crema/blob/475b8d5bd3498dbd5cfefe8175f98d017a017739/port/mesa/ac_ps4_drm.c
- Shared PS4 kit overview: https://github.com/iHaiDeeZ/ps4-kit

## What this independently corroborates

The RADV PS4 winsys source declares and uses the kernel direct-memory API family:
- `sceKernelAllocateDirectMemory`
- `sceKernelMapDirectMemory`
- `sceKernelReleaseDirectMemory`

It also resolves GNM submission entry points by module handle and symbol name,
including `sceGnmSubmitCommandBuffers` and `sceGnmSubmitDone`. This is an
independent implementation pattern for obtaining direct memory and forwarding
command buffers to the GNM driver, rather than relying on the generic OpenGNM
stub submit implementation.

The source search also surfaced a GNM module lookup and symbol-resolution
path in the PS4 winsys. This corroborates that a practical retail homebrew
graphics stack resolves the GNM provider dynamically. It does not establish
the exact module provider selected on PS4 Pro Neo Mode or on firmware 14.02.

The public Crema overview states that rendering runs through Mesa RADV adapted
to submit work directly to the PS4 GPU. Its README says only the original PS4
has been tested so far. The separate xemu-ps4 porting notes report a RADV
startup self-test that performs a graphics draw and readback on PS4; this is
graphics-path evidence only, not compute dispatch evidence:
https://github.com/iHaiDeeZ/xemu-ps4/blob/main/docs/PORTING.md

## What it does not prove for Neural Rendering

This discovery does not close any of the target-specific Gate B requirements:

1. It is not a PS4 Pro test, and does not identify firmware 14.02.
2. The inspected public material does not establish that CPU virtual mappings
   are interchangeable with GPU virtual addresses for NR allocations.
3. It does not prove the residency/registration contract for NR shader code,
   descriptor arrays, or output buffers.
4. GNM submit symbol resolution does not by itself prove the exact FW 14.02
   prototype, argument units, accepted DCB/CCB address form, or provider
   selection between normal and Neo-mode modules.
5. The public source evidence here is principally a Vulkan graphics winsys.
   It does not demonstrate a standalone GNM compute dispatch with known-answer
   output, bounded completion, and full-buffer readback.
6. Graphics readback or a graphics startup self-test cannot substitute for a
   compute write → synchronization → CPU visibility test.

## Design consequences for NR

- Keep CPU mapping, direct-memory allocation offset, and GPU address as
  separately named values. Never silently treat one as another.
- The independent winsys is a source to inspect for allocator and provider
  resolution patterns, not code to copy wholesale into NR.
- Preserve separate normal/Neo provider discovery in the NR ABI census.
- Treat `sceGnmSubmitCommandBuffers` and `sceGnmSubmitDone` as candidate
  firmware entry points until the target provider and exact ABI are verified.
- Require a compute-specific known-answer test; graphics rendering success is
  insufficient.

## Gate B decision

The new evidence strengthens the conclusion that direct-memory allocation,
dynamic GNM symbol resolution, and GNM command submission are used by an
independent PS4 homebrew graphics stack. It narrows the implementation search
space but does not establish NR's firmware 14.02 compute ABI, resource
residency, bounded completion, cache visibility, or deterministic compute
readback on PS4 Pro.

Gate B therefore remains **BLOCKED**. No target-side probe was created or
activated, no compute was submitted, and no PRX, PKG, or console state was
changed as part of this source audit.


## Deeper source inspection — allocator, VA mapping, submit, and fences

The pinned public source was inspected beyond symbol-name search at
DolphinPS4 commit
`879b3e57f3d3180bcb5f2234db3903688b28cd7d`. This reveals concrete
implementation mechanics and several reasons not to transplant it unchanged.

### Allocation and GPU address mapping

- `ac_drm_bo_alloc` rounds allocation sizes to the winsys' 16 KiB page
  granularity, allocates direct memory with
  `sceKernelAllocateDirectMemory`, and chooses memory type from the requested
  heap/flags: write-combined Garlic for VRAM-like allocations and write-back
  Onion for CPU/GTT-like allocations.
- `ac_drm_va_range_alloc` reserves software VA ranges in a VMA heap. Device
  initialization reserves process virtual address windows using
  `sceKernelReserveVirtualRange`.
- `ps4_va_op(AMDGPU_VA_OP_MAP)` maps the direct-memory offset at the exact
  requested VA using `sceKernelMapDirectMemory(..., PS4_MAP_FIXED, phys +
  offset, ...)`. The mapped pointer is stored as `b->cpu` and `b->last_va`;
  this is the source's explicit CPU-pointer/GPU-VA identity model.
- The winsys distinguishes a BO's direct-memory physical offset (`phys`) from
  its mapped virtual address (`last_va`). It also handles a constrained
  one-page alias/padding case and rejects unsupported aliases and sparse PRT
  mappings.
- It attempts to validate a known direct mapping with
  `sceKernelVirtualQuery` before enabling its address-range diagnostics. This
  structure is declared manually in the source and is not a verified FW 14.02
  ABI contract for NR.

This is materially stronger allocator evidence than the earlier generic
OpenGNM/Vulkan-PS4 references: it shows one coherent design joining direct
memory, reserved VA windows, fixed mappings, and BO handles. It remains an
implementation report for a different target/software stack, not proof that
NR may safely reuse the same flags, memory types, alignment, VA limits, or
protection constants on PS4 Pro FW 14.02.

### Submission and completion

- `ac_drm_cs_submit_raw2` accepts IB chunks only when `ip_type` is
  `AMDGPU_HW_IP_GFX`; it constructs DCB arrays and appends a separate fence IB.
  This is a single in-order GFX submission path, not evidence of an asynchronous
  compute queue or the FW 14.02 compute-queue ABI.
- The appended IB emits an `EVENT_WRITE_EOP` packet that writes a 64-bit
  sequence number to a fence page. The source describes the event as flushing
  and invalidating CB/DB and texture caches before the EOP write. The fence
  page is allocated in write-back Onion memory and mapped CPU/GPU read-write.
- Completion uses an acquire atomic load of the sequence counter. A wait can
  have an absolute deadline and return false on timeout; after 64 spins it
  sleeps on an EOP event queue (up to 1 ms) or briefly sleeps. However, several
  internal paths explicitly pass `INT64_MAX`, including fence-ring reuse and
  some memory-pressure recovery, so the implementation is not globally
  bounded.
- The return value of `sceGnmSubmitCommandBuffers` is checked, but the return
  value of `sceGnmSubmitDone` is ignored. The sequence is rolled back when
  the submit call fails.
- Sync-object waits in the submit parser wait until the requested point has
  been submitted to this in-order queue; they do not independently wait for
  GPU completion at that point. That loop has no deadline.
- BO unmaps, frees, and VA-range frees are deferred until the latest submitted
  sequence has completed. This is a useful lifetime-management pattern for
  avoiding reuse while the GPU may still access resources.

These details identify candidate patterns for NR (EOP sequence fences,
deferred destruction, and a single in-order submission queue) while also
exposing non-portable or unsafe details (ignored submitDone result,
unbounded internal waits, and a queue implementation that accepts GFX IBs
only). No PM4 packet or timeout behavior is promoted to a FW 14.02 contract.

### Compute applicability and hardware mismatch

The inspected winsys identifies its target as Liverpool GCN gfx7 and its
submission parser accepts only `AMDGPU_HW_IP_GFX`. Its surrounding RADV port
therefore provides evidence for graphics-queue submission and the general
ability of a Vulkan stack to carry command streams; it does not establish a
standalone GFX8/NEO compute queue, the compute shader initialization ABI, or
the required NR buffer-to-compute-to-buffer known-answer path. Crema's README
also says only the original PS4 has been tested, not PS4 Pro.

The source's module loader calls `sceKernelLoadStartModule` for
`libSceGnmDriver.sprx`. NR must not copy that behavior into a census or
probe: the current Gate B constraints require passive inspection of already
loaded normal and Neo-mode providers and prohibit loading a missing module.
The source's provider path is a useful contrast, not authorization to activate
a module or change console state.

### Updated engineering disposition

1. Retain the source as a reference for the *shape* of a coherent direct-memory
   winsys, not as an ABI/constant copy source.
2. Do not transplant its GFX7 packet values, page/memory-type constants,
   `sceKernelVirtualQuery` structure, or module-loading behavior.
3. NR still needs passive, target-specific FW 14.02 provider/export evidence
   for both normal and Neo-mode drivers, followed by validated GPU-address
   and residency semantics.
4. NR still needs bounded fail-closed submission/completion, cache visibility,
   and deterministic full-buffer compute readback on the PS4 Pro.
5. No target probe, GNM call, compute submission, PRX activation, PKG install,
   or console-state change was performed during this source inspection.


## Cross-check: OpenGNM's newer multi-queue plan is not compute-queue proof

A separate review of the pinned OpenGNM commit
`4b295ca54c82c83acf308d1c646a2dfa9ae57350` found a recent
`VULKAN_PS4_PLAN.md` entry marking multi-queue support DONE. It describes
Family 0 as graphics+compute+transfer and Family 1 as compute+transfer,
calling the latter "async compute via ACE", and says a VVL test exercises
two-family device creation and queue retrieval.

That statement must not be promoted to Gate B evidence:

- The pinned commit's complete recursive Git tree contains 183 entries and
  does not contain a `vulkan-ps4/` implementation directory or any
  `vk_ps4_queue.c`, compute queue backend, or Vulkan compute test source.
  The referenced plan is a roadmap/status document, not an implementation
  artifact in this pinned repository tree.
- The actual pinned OpenGNM generic backend explicitly implements
  `sceGnmMapComputeQueue` and
  `sceGnmMapComputeQueueWithPriority` as
  `GNM_ERROR_UNSUPPORTED`; `sceGnmDingDong` is a no-op. Its API test
  accepts unsupported as a valid generic-host outcome and only checks
  linkage/return behavior.
- The generic `sceGnmDispatchDirect` builder emits SET_SH_REG,
  DISPATCH_DIRECT and NOP packets into a host buffer. That validates a
  source-level packet builder only; it does not submit to a compute queue,
  execute a shader, or produce GPU output.
- The Orbis backend declares firmware-facing compute queue entry points,
  but declarations/extern references alone do not establish that they are
  exported by either FW 14.02 provider, nor their exact ABI/semantics.
- The plan's VVL queue-family test, even if available elsewhere, is described
  as device creation and queue retrieval. It does not establish successful
  compute queue mapping, ACE execution, completion, or GPU readback.

Sources at the exact pinned commit:
- `VULKAN_PS4_PLAN.md`:
  https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/VULKAN_PS4_PLAN.md
- Generic backend:
  https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/src/driver_generic.c
- Generic API tests:
  https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/tests/test_api.c
- Orbis declarations:
  https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/src/driver_orbis.c

### Consequence

The earlier retail-title reports remain the only inspected evidence here for
real applications using asynchronous GNM compute queues. OpenGNM contributes
candidate declarations and a generic PM4 dispatch builder, not a validated
compute queue implementation. Gate B therefore still requires a target-specific
FW 14.02 provider/ABI census and a validated PS4 Pro compute execution path.
No firmware function was called and no console state was changed.
