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
load/store instructions. The pinned PSBC source now confirms the compiler-side ABI: each storage
binding is a 16-byte V# descriptor in descriptor set 0, at byte offset N×64.
The table pointer is the `PTR_INDIRECTRESOURCETABLE` user-data pointer at
register 2 and occupies one 32-bit SGPR in this binary. A PRX link probe showed that OpenGNM's
`sceGnmDrawCmdSetPointerUserData` is a source helper, not a firmware export
(`ld.lld` reports it undefined when treated as an import); its generic
implementation also uses `SET_SH_REG` without the compute-engine shader-type
bit. NR therefore adds `pne_gcn8_set_cs_indirect_resource_table_pointer`, a
local PM4 writer that targets CS user register 2 and sets
`PKT3_SHADER_TYPE_S(1)`. This is a compile/link candidate only; it has not been
executed on hardware. Writable V# field values, GPU VA mapping and cache
policy remain unverified.

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
register 2. The OpenGNM helper `sceGnmDrawCmdSetPointerUserData` writes two 32-bit
words, but that generic behavior is **not correct for this compiled shader**.
The raw slot control byte is `0x00` (`registercount=0`), PSBC documents
`address32_hi=0`, and the emitted ISA loads the table base from SGPR `s2`;
SGPR `s3` is used for scalar shader parameters. Writing a high pointer word
to register 3 would corrupt live shader data.

NR therefore emits a three-dword compute `SET_SH_REG` packet with exactly one
data dword: header `0xC0017602`, register offset `0x242` (CS user register
2), and the low 32-bit table address. No high dword is written. The previous
four-dword/`0xC0027602` packet test in run `44744971532` is superseded; the
corrected one-dword packet host/PRX revalidation is pending. The PS4 wrapper
statically asserts that the OpenGNM PM4 macros and GFX8 register definitions
match the host packet constants. This still does not prove GPU acceptance.

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
code range, resource slot, CS registers and address alignment. Run
`44743915113` passed this parser against all five emitted `.sb` files and then
passed the byte-level Python report verifier. That verifier compared actual
source/SPIR-V/container/GCN/ISA bytes and re-decoded raw CS register fields via
the pinned Mesa GFX8 register database. The upstream `verify_sb.py` remains
the CRC verifier. This is host-side evidence only; no GPU load or dispatch.

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
7. Emit dispatch dimensions as **workgroup counts**, not work-item counts.
   AMD's GCN `DISPATCH_DIRECT` packet defines DIM_X/Y/Z as dimensions of the
   thread-group array; the OpenGNM helper writes these fields directly. For
   the tiled GEMM (8×8 local size, 16×16 output tile), use
   `ceil(N/16) × ceil(M/16) × 1` groups.
8. Submit using a supported compute submission path; wait for completion.
9. Perform the documented GPU-to-CPU cache transition before reading output.
10. Compare output against the independent CPU reference and record error,
    latency, and firmware/toolchain identifiers.

## Current execution gate

The compiler-side descriptor-table ABI and shader metadata parser are now sufficiently concrete to define the **next** implementation boundary, but not to cross it blindly. The runtime gate remains closed until a reversible hardware probe establishes, independently, all of the following:

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
- No shader resource binding has been exercised on a PS4 Pro.


## Direct-memory SDK references

- OpenOrbis `libkernel.h` declarations:
  https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/0a1aaf9dd4a92695538bdeb09fb056d06dd11725/include/orbis/libkernel.h
- OpenOrbis framebuffer sample (allocation and mapping example; not a compute
  memory contract):
  https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/0a1aaf9dd4a92695538bdeb09fb056d06dd11725/samples/_common/graphics.cpp
- OpenOrbis Libkernel notes:
  https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/0a1aaf9dd4a92695538bdeb09fb056d06dd11725/docs/MD/PS4%20Libraries/Libkernel.md

## V# descriptor codec shared with host tests — 2026-11-22

The compute harness now uses `host/vsharp_codec.cpp` to pack the four V# dwords instead of constructing the `GnmBuffer` bitfields independently. The PS4 compute build links this same source file. Host golden tests cover the exact `R32_FLOAT` input descriptor using `GNM_MEMORY_READONLY` and output descriptor using `GNM_MEMORY_UNCACHED`; the memory-type field encoder is also shared and tested. Latest host CI run `45015947339`, job `137188178578`, passed 9/9 tests. PS4 PRX/PKG run `45015947338` completed successfully: its build log confirms compilation/linking of `vsharp_codec.o` and `compute_smoke.o`; independent PKG QA job `137188551284` passed PC-only checks.

This validates byte encoding consistency only. It does not validate the runtime meaning of the memory type, writable SSBO acceptance, cache policy, GPU VA mapping, low-4-GiB table placement, or compute dispatch. The initial PRX build exposed a missing include path (run `45015714818`); this was corrected in commit `82719c1392bab7fe00fb2301220c3b80de82e7d7`. A second audit found that the GoldHEN plugin Makefile also needed to compile/link `vsharp_codec.o`; fixed in commit `c31d3ab2a70e8e7e548a4931323f1627ed2d7396`. PS4 PRX/PKG run `45015947338` and its independent PC package QA completed successfully. The separate GCN8 shader/compute PKG run `45015947332` completed successfully (jobs `137188671170` and `137189591652`). The artifact ZIP was downloaded and rehashed; ELF/OELF/MAP were independently reopened. The compute ELF has only `libkernel.so`, the OELF import table contains only `libkernel.prx` / `libkernel` with 19 system NIDs, and the map confirms the shared V# codec object is linked. The package payload passed the workflow's PkgTool extraction check. This validates build/link and binary import isolation, not writable V# semantics, cache policy, GPU VA, or hardware dispatch. The compile-time compute gate remains closed; the diagnostic PKG must not be installed.
## Atualização 2026-11-22 — Sony import audit e GNM-Load-Only

`tools/audit_ps4_self_imports.py` audita OELF/fake SELF depois de `create-fself`, validando `DT_SCE_IMPORT_LIB`, `DT_SCE_NEEDED_MODULE`, NID strings e relocações. O compute eboot deve excluir import GNM; o GNM-Link eboot exige o NID `yvZ73uQUqrk`; o PRX principal exige `xbxNatawohc`.

O GNM-Load-Only agora resolve via `sceKernelDlsym`, sem chamar, cinco exports incluindo `sceGnmSubmitAndFlipCommandBuffers`. Build/QA PC-only do run `45018436422` passou. A resolução real desse quinto export e o comportamento do loader continuam `NOT_RUN` até teste autorizado no PS4.