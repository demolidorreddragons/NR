# GNM memory and submission API findings

Updated: 2026-11-20  
Pinned upstream: \`PS4-OpenGNM/opengnm@4b295ca54c82c83acf308d1c646a2dfa9ae57350\`

This is an API-surface investigation, not proof of compute execution or a validated memory ABI for the Neural Rendering shaders.

## Direct-memory allocation evidence

The pinned \`src/helpers.c\` implements the Orbis path using:

- \`sceKernelGetDirectMemorySize()\`
- \`sceKernelAllocateDirectMemory(searchstart, searchend, len, alignment, memorytype, &offset)\`
- \`sceKernelMapDirectMemory(&mapped, len, protection, flags, offset, alignment)\`
- \`sceKernelMunmap(mapped, len)\`
- \`sceKernelReleaseDirectMemory(offset, len)\`

The helper checks power-of-two alignment, rounds requested size with overflow checks, releases the direct-memory allocation if mapping fails, and clears its structure on failure/release.

The existing VideoOut helper uses \`GNM_DIRECT_MEMORY_TYPE_WC_GARLIC\` (value 3 in the pinned helper header) and \`GNM_PROT_CPU_GPU_RW\` for display buffers. This is evidence for that VideoOut path only. It does **not** prove that the same pool/protection is appropriate for compute storage buffers, nor that a CPU mapped pointer can be passed unchanged as a GPU virtual address.

## Addressing and descriptor limitations

- \`GnmDirectMemory\` retains both a CPU mapping and a direct-memory offset. The helper does not establish a general CPU-pointer-to-GPU-VA translation contract for arbitrary compute buffers.
- Alignment, address ownership, descriptor address encoding, cache policy and visibility must be validated for the exact compute allocation path.
- Run `44739871394` now supplies the first compiler metadata: one `PTR_INDIRECTRESOURCETABLE` slot at start register 2, eight user SGPRs for tiled GEMM, and four SPIR-V storage bindings logged at descriptor-table offsets 0/64/128/192. This still does not distinguish the exact runtime V# vs RW# descriptor encoding or prove a 16-byte V# is the correct storage-buffer ABI.
- No cache flush/invalidate sequence has been proven for the NR compute buffers.

## Command recording and submission evidence

The pinned API exposes:
- \`sceGnmDrawCmdSetCsShader\` and \`sceGnmDrawCmdSetVsharpUserData\` helpers for command recording.
- \`sceGnmDrawCmdDispatchDirect\` for a direct-dispatch PM4 packet.
- \`sceGnmSubmitCommandBuffers\` for DCB/CCB submission.
- \`sceGnmSubmitDone\` for submit completion.

The OpenGNM hardware smoke documentation demonstrates a VideoOut graphics submission and completion path. It is not a compute-dispatch demonstration. The GNM driver reference describes \`sceGnmDispatchDirect\` as taking explicit thread-group dimensions, while the command-buffer helper's PM4 packet must be reconciled with the compiled shader's local workgroup size before use. Do not mix invocation counts and workgroup counts.

The existence of declarations, wrappers or a successful import-link smoke test is not runtime proof. The NR repository still has no isolated GNM compute harness, no confirmed queue lifecycle for this compute use, and no PS4 Pro dispatch result.

## Task status impact

- Task 71: partial compiler evidence now identifies `PTR_INDIRECTRESOURCETABLE` at user-data register 2; descriptor class/encoding and runtime binding remain unresolved.
- Task 72: blocked until the RW descriptor bytes and GPU address encoding are independently established.
- Task 73: candidate direct-memory allocation APIs and a VideoOut memory example identified; compute memory type/protection remains unapproved.
- Task 74: CPU mapping and direct-memory offset are visible in the helper, but GPU VA semantics remain blocked.
- Task 75: upstream helper offers an allocation/release candidate; no NR compute buffer lifecycle has been implemented or hardware-validated.
- Task 76: cache coherency remains blocked.
- Tasks 78–80: command recording and submit APIs are identified; compute bindings, submission lifecycle and isolated harness remain blocked.

## Sources

- \`src/helpers.c\`, \`include/gnm_helpers.h\`
- \`tests/hardware_smoke.c\`
- \`docs/examples/hardware-smoke.md\`
- \`docs/reference/driver.md\`
- \`docs/guides/rendering-pipeline.md\`

All references above are from the pinned OpenGNM revision stated at the top.