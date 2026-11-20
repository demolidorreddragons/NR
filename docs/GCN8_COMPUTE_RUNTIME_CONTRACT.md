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

## First compiled GFX8 metadata evidence — run 44739871394

The first successful five-shader compiler artifact is recorded in
`docs/GCN8_GFX8_ARTIFACT_44739871394.md`. For the tiled FP32 shader, the
compiler emits one `PTR_INDIRECTRESOURCETABLE` input-usage slot at API slot 0,
starting at user-data register 2. `COMPUTE_PGM_RSRC2` reports 8 user SGPRs,
wave64, LDS allocation 2,048 bytes, and no scratch. The compiled workgroup is
8×8×1, matching SPIR-V LocalSize.

PSBC's compiler log maps SPIR-V storage bindings 0–3 to descriptor-table byte
offsets 0, 64, 128 and 192 (16-byte descriptor payloads reported for each
binding). The emitted ISA uses scalar loads from the table pointer and buffer
load/store instructions. This is useful ABI evidence, but it does not yet
prove the exact Orbis runtime descriptor class, descriptor encoding, GPU VA
semantics or the setter API. In particular, do not equate
`PTR_INDIRECTRESOURCETABLE` with four directly bound V# descriptors.

The resource-allocation fields are decoded from the pinned Mesa GFX8 register
JSON. They are compile-time allocations, not runtime occupancy measurements.
REA-5 has not independently reviewed these artifacts.

## Resource-binding constraint

`GnmBuffer` is a 16-byte V# descriptor, and the pinned PSBC source explicitly lowers storage-buffer descriptors to 16-byte V# entries in an indirect resource table. The compiled GNM metadata identifies that table pointer at user register 2. The remaining work is to validate writable V# fields, GPU-address encoding, cache policy and exact table allocation/registration before implementing binding. Do not treat the compiler's descriptor-layout contract as proof of runtime memory visibility.

## Descriptor-table layout inferred from the pinned PSBC implementation

The pinned PSBC source `libpsbc/psbc_compile.c` documents and implements the
descriptor ABI used by these standalone shaders:

- descriptor set 0 only;
- binding N begins at byte offset N × 64;
- uniform/storage buffer and texel-buffer descriptors are 16-byte V# entries;
- sampled/storage images are 32-byte T# entries; samplers are 16-byte S# entries;
- combined image samplers occupy 48 bytes (T# at +0, S# at +32);
- the table pointer is passed in the 32-bit user SGPR named by
  `PTR_INDIRECTRESOURCETABLE`; PSBC sets `address32_hi=0`, so the table must
  be below 4 GiB.

For the current GEMM shader, PSBC logs four storage-buffer bindings at offsets
0, 64, 128 and 192, with 16-byte descriptors; the descriptor table size is
208 bytes. The GNM metadata says `PTR_INDIRECTRESOURCETABLE` starts at user
register 2. The pinned OpenGNM helper `sceGnmDrawCmdSetPointerUserData`
writes a host pointer as two 32-bit words to the requested CS user-data slot;
its implementation maps slot 2 to the compute user-data register pair 2:3.
This gives a concrete command-recording candidate for the table pointer,
subject to confirming the actual GPU VA and runtime behavior on hardware.

The host-only implementation `platform/ps4/gcn8/host/resource_table_contract.*`
now mirrors this layout and rejects unsupported sets, unsorted/overlapping
bindings, size overflow, unaligned pointers, and table ranges crossing 4 GiB.
For the four GEMM storage bindings it computes offsets 0/64/128/192 and total
size 208 bytes. Its CTest passed in run `44742071718`. It calculates table
layout only; it does not create the actual GPU descriptors or allocate the
table in GPU-visible memory.

OpenGNM's `GnmBuffer` is exactly 16 bytes and exposes base address, stride,
record count, format and memory-type fields. Its convenience
`sceGnmCreateConstBuffer` and `sceGnmCreateVertexBuffer` helpers set
`GNM_MEMORY_READONLY`; do not use them blindly for writable SSBOs. The
descriptor's correct writable-memory/cache policy, raw-dword format, record
count, GPU address encoding and cache visibility still require authoritative
validation. The descriptor class is now identified as V# by PSBC's own source,
but the final writable V# byte construction is not yet accepted.

## GNM compute binary metadata parser

`platform/ps4/gcn8/host/shader_binary_contract.*` now parses the pinned PSBC
PS4 NEO compute container layout without invoking PSBC. It validates the GNM
`Shdr` file header, compute/NEO target, CS header size, single indirect
resource-table slot, the 8-bit slot-table pointer, code range, common shader
size and raw seven-word `GnmCsStageRegisters` block. It returns the code
offset/length, slot-table offset/count, CRC field and register words.

The parser intentionally does not recompute CRC; the pinned upstream
`verify_sb.py` does that. The host helper
`set_cs_program_gpu_address` mirrors `sceGnmCsRegsSetAddress`: it replaces
the binary-relative `computepgmlo/hi` values with a 256-byte-aligned GPU code
address below 48 bits. Runtime use must copy the code to GPU-visible memory
first, then patch the register block; the parser itself never submits or
dispatches a shader.

The C++ test covers malformed headers, wrong stage/target, slot pointer/count,
code range, resource slot, CS registers and address alignment. The GFX8 CI
workflow also runs this parser against all five emitted `.sb` files before
the byte-level Python report verifier. This remains host-side evidence.

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

OpenGNM's own direct-memory wrapper/reference uses
`GNM_DIRECT_MEMORY_TYPE_WC_GARLIC` (write-combined Garlic) with
`GNM_PROT_CPU_GPU_RW` for GPU-visible allocations and documents 64 KiB
alignment for the display path. This is a concrete allocator candidate, not a
compute-buffer approval: the sample is a VideoOut/display allocation, not a
validated CS SSBO allocation. The `GnmMemoryType` values embedded in each
V# descriptor are a separate cache/access policy from the direct-memory pool
type; do not conflate them.

The PSBC compiler's indirect resource-table pointer is emitted as a 32-bit
user-SGPR address (`address32_hi=0`). Therefore the 208-byte table itself must
reside wholly below the 4 GiB GPU-address boundary, even though the individual
16-byte V# entries carry a 44-bit base address. The host contract checks this
range, but no PS4 allocation has yet demonstrated that a suitable low GPU VA
can be obtained.

The pinned OpenGNM helper implementation `src/helpers.c` allocates with
`sceKernelAllocateDirectMemory`, then maps the returned physical allocation
offset with `sceKernelMapDirectMemory`. Its `GnmDirectMemory` structure keeps
both the mapped virtual pointer and the allocation offset. The OpenOrbis
libkernel reference describes the first output as a physical address and the
second call as mapping that physical range into virtual memory. A separate
Vulkan-on-PS4 implementation uses the mapped pointer as the `GnmBuffer`
base-address input, while retaining the physical offset for release. This is
a useful implementation candidate, not a verified NR compute result; do not
pass the allocation offset directly to `sceGnmBufSetBaseAddress` without
proving its address semantics.

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

## Current execution gate

The compiler-side ABI is now sufficiently concrete to define the **next** implementation boundary, but not to cross it blindly. The runtime gate remains closed until a reversible hardware probe establishes, independently, all of the following:

- a GPU-visible allocation/mapping pair valid for a compute buffer;
- construction of one writable V# descriptor whose base address and stride are accepted by the generated shader;
- descriptor-table placement below the compiler's 32-bit table-pointer limit;
- command-buffer initialization, CS shader state and pointer user-data recording;
- submit/completion behavior and CPU-visible output after the documented cache transition.

The first hardware probe must use a tiny scalar/write-read shader, not GEMM or the 71-block network. It must have a bounded allocation, a unique test address, deterministic expected output, timeout/recovery handling, and no installer/plugin changes. Failure at any gate leaves the runtime unmodified.

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
