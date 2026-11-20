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
- Run `44739871394` supplies the first compiler metadata: one `PTR_INDIRECTRESOURCETABLE` slot at start register 2, eight user SGPRs for tiled GEMM, and four SPIR-V storage bindings at table offsets 0/64/128/192. Pinned PSBC source explicitly defines storage-buffer entries as 16-byte V# descriptors, with binding N at N×64 and the table pointer in a 32-bit user SGPR. This closes the compiler-side descriptor class/layout question; writable V# field policy, GPU VA encoding and runtime visibility remain open.
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

- Task 71: PASS at compiler-ABI scope: PSBC source defines 16-byte V# storage descriptors in a 64-byte binding stride; compiled metadata identifies the indirect table pointer at user register 2, and OpenGNM pointer-user-data maps slot 2 to CS registers 2:3.
- Task 72: partial: `GnmBuffer` is 16 bytes and its fields are known, but writable raw-dword format, memory/cache policy, GPU VA encoding and hardware visibility remain unvalidated.
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

## PSBC descriptor-table contract — source-level confirmation

The pinned `opengnm-psbc/libpsbc/psbc_compile.c` defines the descriptor layout used by the NR SPIR-V shaders: descriptor set 0 only; binding N at byte offset N×64; storage/uniform buffer and texel-buffer entries are 16-byte V#; sampled/storage images are 32-byte T#; samplers are 16-byte S#; combined image samplers are 48 bytes. It sets `address32_hi=0` for the indirect table pointer, requiring the table GPU address below 4 GiB. For the four GEMM SSBOs, PSBC logs table size 208 bytes and offsets 0, 64, 128 and 192. The GNM metadata declares `PTR_INDIRECTRESOURCETABLE` at user register 2; OpenGNM's pointer-user-data helper maps slot 2 to the CS register pair 2:3.

This is sufficient to specify the compiler-side V# table layout. It does not validate the writable V# descriptor fields/cache policy or GPU memory visibility. The convenience `sceGnmCreateConstBuffer` and `sceGnmCreateVertexBuffer` helpers set `GNM_MEMORY_READONLY`; neither is approved for writable SSBO output without further proof.


## Compute user-data pointer helper link failure and local packet writer

PRX link smoke run `44742912402` compiled the GNM API declarations but failed at link with `undefined symbol: sceGnmDrawCmdSetPointerUserData`. Inspection of pinned OpenGNM shows this is implemented in `src/drawcommandbuffer.c` as a convenience helper, not exported by `libSceGnmDriver`. Its generic persistent-register writer also omits `PKT3_SHADER_TYPE_S(1)`, while OpenGNM's dedicated `setshregcompute` adds that bit for compute-engine SET_SH_REG packets.

NR now implements `pne_gcn8_set_cs_indirect_resource_table_pointer` in `platform/ps4/gcn8/gnm_resource_binding.cpp`. It validates the full non-zero/aligned table range below 4 GiB, checks the two-register slot bounds and command-buffer capacity, then emits a four-dword SET_SH_REG packet targeting `R_00B900_COMPUTE_USER_DATA_0 + 2*4`, with compute shader type bit set, low address word and high word zero. The PRX workflow compiles this local helper into the smoke ELF instead of importing the non-exported convenience symbol. Runtime behavior remains untested; CI revalidation pending.
