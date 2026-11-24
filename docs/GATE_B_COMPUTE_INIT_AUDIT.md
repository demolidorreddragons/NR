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
