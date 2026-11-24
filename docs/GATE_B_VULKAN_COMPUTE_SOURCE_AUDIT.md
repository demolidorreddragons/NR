# Gate B — independent Vulkan/GNM compute-path source audit

Date: 2026-11-24

## Scope and provenance

Inspected the public source of `PS4-OpenGNM/vulkan-ps4` at commit
`f4d940b723771b3fe637e8ac0704bb5d6ab19622`. This is a separate implementation
built on OpenGNM; it is not part of the NR production runtime and is not a
Sony SDK source or firmware 14.02 ABI record. Findings below are source-level
observations only.

Primary files:
- `src/vk_ps4_pipeline.c`
- `src/vk_ps4_command.c`
- `src/vk_ps4_descriptor.c`
- `src/vk_ps4_queue.c`
- `src/vk_ps4_memory.c`
- `tests/test_validation.c`

## Positive evidence: a compute path is actually wired in source

Unlike the earlier GNM smoke test, this implementation contains a source-level
Vulkan compute path:

1. `vk_ps4_CreateComputePipelines` compiles the compute stage, derives the
   shader-code pointer from the GNM binary header, calls
   `sceGnmCsRegsSetAddress`, and extracts CS input-usage slots.
2. `vk_ps4_CmdBindPipeline` calls `sceGnmDrawCmdSetCsShader` for the compute
   bind point.
3. `vk_ps4_CmdBindDescriptorSets` has a compute branch selecting
   `GNM_STAGE_CS` and emitting Vsharp/Tsharp/Ssharp/Pointer user-data commands.
4. `vk_ps4_CmdDispatch` emits
   `sceGnmDrawCmdDispatchDirect(&cmd->gnm_cmd, x, y, z, 0)`.
5. `vk_ps4_QueueSubmit` submits recorded command buffers through
   `sceGnmSubmitCommandBuffers`; the queue family is logged but does not
   select a separate compute submit API. Thus this implementation appears to
   issue compute dispatch packets through the same GNM DCB submission path,
   rather than proving a dedicated compute queue.
6. The validation source creates a compute pipeline, binds a descriptor set,
   records `vkCmdDispatch(1,1,1)`, and records a compute-write to host-read
   pipeline barrier.

This is materially stronger than a plan document: the code path is implemented
at source level. It still does not establish that the code has been successfully
executed on PS4 Pro, or on firmware 14.02.

## Blocking defects and unproven contracts in that reference

### A. The PM4 user-data fix is required

The compute descriptor path invokes the four OpenGNM stage setters with
`GNM_STAGE_CS`. The pinned OpenGNM source originally emitted
`SET_SH_REG` without the compute shader-type bit for those generic setters.
NR's PR #65 patch corrects that specific source defect and has passed host
application/build tests. The external Vulkan source must be paired with that
patched OpenGNM revision before it can be considered a viable candidate.

### B. Compute pipeline code lifetime / GPU address

The Vulkan source retains compiled CS binary storage in `pipe->cs_binary`,
because `cs_regs` points into that allocation. This addresses lifetime until
pipeline destruction, but the inspected code uses the project's allocator and
does not itself prove that the returned address is valid GPU VA, resident, or
within the target's required address range on retail firmware 14.02.

### C. Descriptor memory and resource residency

The compute descriptor path passes descriptor objects to OpenGNM user-data
setters. It does not establish from the inspected code alone that each backing
buffer/image is registered/resident in the way required by retail firmware
14.02. Resource registration APIs in the pinned OpenGNM Orbis backend are
stubs. A mapped pointer being numerically usable in a PM4 packet is not
sufficient proof of residency.

### D. Synchronization is not yet safe enough for NR

The queue implementation emits EOP cache-flush/timestamp events for semaphore
and fence labels. However:
- `vk_ps4_QueueWaitIdle` polls the EOP label in an unbounded busy loop on PS4.
- `vk_ps4_WaitForFences` must be checked separately before adopting this path;
  no bounded timeout/recovery proof is established by this audit.
- Semaphore wait submission failure can fall through without a reliable
  blocking wait.
- The shared epilogue command buffer is reused for waits/signals; serialization
  and in-flight lifetime requirements need proof before concurrent submits.
- `vk_ps4_CmdPipelineBarrier` emits a cache-flush EOP event, but the source
  does not prove all required compute-to-host cache visibility semantics on
  firmware 14.02.

Therefore the reference's synchronization code must not be copied into NR as-is.

### E. The hardware test is not evidence

The validation test records a compute dispatch and barrier, but the inspected
source is a test harness, not a recorded PS4 Pro run with output comparison.
A successful host build or a printed “CmdDispatch OK” only proves command
recording, not GPU execution. No deterministic output hash or full-buffer
readback evidence was found in the inspected sources.

## NR candidate contract after this audit

A minimal future harness can be based on a single GNM DCB submitted through
the Orbis submit wrapper, with:
- one GCN8 compute shader and known local size;
- one output allocation whose CPU mapping and GPU address are separately
  tracked and range-checked;
- compute shader register programming;
- CS user-data pointer emitted with the corrected compute shader-type bit;
- direct dispatch packet in the same DCB;
- an EOP completion label and a bounded kernel wait (no unbounded spin);
- explicit GPU-to-CPU visibility operation or documented coherent mapping;
- full output-buffer comparison and SHA-256 recorded from the PS4 Pro.

This is a candidate design, not a validated ABI or execution sequence. The
firmware-facing submit signature, command-buffer address expectations,
resource placement/residency, event wait API, cache policy, and actual output
must still be verified on the exact target.

## Gate B decision

This audit advances the design from “dispatch builder candidate only” to
“independent implementation contains an end-to-end compute command-recording
path, with important correctness and hardware-validation gaps.” It does not
close Gate B.

Gate B remains **BLOCKED** until the exact firmware 14.02 ABI/placement/wait
contract is established and a controlled PS4 Pro run proves bounded completion
and deterministic readback. No console dispatch, PRX activation, PKG install,
or console-state change was performed by this audit.

Source:
https://github.com/PS4-OpenGNM/vulkan-ps4/tree/f4d940b723771b3fe637e8ac0704bb5d6ab19622


## Additional synchronization-source inspection

Inspected `src/vk_ps4_sync.c` at the same pinned Vulkan-PS4 commit.

- `vk_ps4_WaitForFences` implements a timeout deadline using a monotonic clock; after an initial spin phase it sleeps for 1 microsecond between polls. This is a candidate bounded CPU wait when a finite timeout is supplied. The implementation still needs review for clock/timeout edge cases and firmware behavior.
- `vk_ps4_QueueWaitIdle` is materially different: its PS4 path loops on the EOP label with no timeout, cancellation, or recovery. It is unsuitable as the NR harness wait primitive.
- Fence labels are allocated as WC Garlic direct memory and written by EOP. That is useful implementation evidence, but it does not establish that this exact label mapping/cache behavior is valid on NR's target firmware 14.02.
- `vk_ps4_QueueSubmit` reuses a single device epilogue DCB for wait and signal writes. A minimal NR harness should avoid concurrent submits and use a single in-flight submission until command-buffer lifetime and serialization are independently established.

Consequently, the source audit now identifies a possible finite-timeout fence-wait implementation pattern, but does not certify it. NR must not use `QueueWaitIdle`'s unbounded loop or copy the synchronization code without the target-specific checks above.
