# GCN8 Compute Runtime Contract (PS4 Pro)

Status: API reconnaissance / design contract. This is not an executable backend.

Pinned API reference:
- OpenGNM: `4b295ca54c82c83acf308d1c646a2dfa9ae57350`
- Header files: `include/gnm_commandbuffer.h`, `include/gnm_drawcommandbuffer.h`,
  `include/gnmdriver.h`, `include/gnm_buffer.h`.

## Confirmed public declarations

The pinned headers declare:
- `sceGnmCmdInit`, `sceGnmCmdReset`, and `sceGnmCmdAllocInside` for
  command-buffer storage management.
- `sceGnmDrawCmdSetCsShader` and
  `sceGnmDrawCmdSetCsShaderWithModifier` for CS register state.
- `sceGnmDrawCmdDispatchDirect` and
  `sceGnmDispatchDirect` for dispatch packet generation.
- `sceGnmSubmitCommandBuffers` and `sceGnmSubmitDone` for submission /
  completion entry points.
- `sceGnmDrawCmdSetVsharpUserData` for a `GnmBuffer` (V#) descriptor and
  `sceGnmDrawCmdSetPointerUserData` for pointer user data.
- `sceGnmComputeWaitOnAddress` and draw-command-buffer wait/event helpers.

These declarations are API evidence only. The compile/link smoke confirms
header compatibility and import-library symbol resolution in the CI toolchain;
it does not prove runtime behavior or firmware availability on every supported
firmware.

## Resource-binding constraint

`GnmBuffer` is a 16-byte V# descriptor. The presence of a V# setter does not
prove that GLSL `std430` storage-buffer variables emitted by the shader
compiler are represented by V# descriptors. Before implementing binding,
inspect the generated GFX8 compute shader's user-SGPR/resource metadata and
confirm the expected descriptor kind, slot numbering, and register layout.
Do not bind the four GEMM buffers by assumption.

## Memory-management boundary

The OpenOrbis SDK headers declare the following candidate libkernel calls:
- `sceKernelAllocateDirectMemory(searchStart, searchEnd, length, align, type, &physicalOffset)`;
- `sceKernelMapDirectMemory(&virtualAddress, length, protections, flags, physicalOffset, align)`;
- `sceKernelReleaseDirectMemory(physicalOffset, length)`.

The OpenOrbis sample `samples/_common/graphics.cpp` uses these APIs for
display/framebuffer memory. Its sample values (memory type `3`, protections
`0x33`, flags `0`) are evidence for that display path only; they are NOT
approved defaults for compute buffers. The project compile/link smoke now
checks the SDK declarations and import symbols for these three functions,
without invoking them.

Still unresolved before implementing the allocator:
- which direct-memory type and protections are valid for general compute
  buffers on supported GoldHEN/firmware combinations;
- whether the mapped virtual address is directly valid as a GPU address or
  requires a GPU address translation/registration step;
- alignment, size rounding, and address-width constraints for CS resources;
- CPU/GPU cache flush/invalidate operations and ownership transitions;
- lifetime rules while a submitted command buffer is in flight.

No allocator is implemented until these contracts are verified. Do not use
ordinary `malloc` addresses as GPU addresses.

## Required dispatch lifecycle

1. Obtain GPU-visible allocations and CPU mappings through a verified Orbis
   memory API (not yet selected).
2. Populate input/output data and perform the documented cache transition.
3. Build the exact resource descriptors and user-data table from shader
   metadata.
4. Initialize a command buffer in GPU-visible command memory.
5. Set CS registers from the generated shader binary metadata.
6. Bind resources using the descriptor form proven by the metadata.
7. Emit dispatch dimensions in workgroups, after confirming the generated
   shader's local size and the helper's packet semantics.
8. Submit using a supported compute submission path; wait for completion.
9. Perform the documented GPU-to-CPU cache transition before reading output.
10. Compare output against the independent CPU reference and record error,
    latency, and firmware/toolchain identifiers.

## Explicit non-claims

- No GPU memory has been allocated by this project runtime.
- No command buffer has been submitted by this project runtime.
- No GCN8 dispatch has been executed on a PS4 Pro.
- No cache-coherency behavior has been measured.
- No shader resource binding has been proven.


## Direct-memory SDK references

- OpenOrbis `libkernel.h` declarations:
  https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/0a1aaf9dd4a92695538bdeb09fb056d06dd11725/include/orbis/libkernel.h
- OpenOrbis framebuffer sample (allocation and mapping example; not a compute
  memory contract):
  https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/0a1aaf9dd4a92695538bdeb09fb056d06dd11725/samples/_common/graphics.cpp
- OpenOrbis Libkernel notes:
  https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/0a1aaf9dd4a92695538bdeb09fb056d06dd11725/docs/MD/PS4%20Libraries/Libkernel.md
