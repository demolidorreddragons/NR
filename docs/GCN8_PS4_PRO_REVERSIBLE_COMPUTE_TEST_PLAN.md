# PS4 Pro GCN8 compute — reversible test plan (DRAFT)

**Status: NOT APPROVED FOR CONSOLE EXECUTION.** This is preparation only. It does not authorize PRX deployment, PKG installation, plugin activation, edits to `plugins.ini`, changes under `/data/GoldHEN`, save/app data access, or persistent system modifications.

## Exact first test target

- GPU: PS4 Pro NEO / AMD GCN8.
- Shader: `gemm_fp32_wave64_tiled`.
- Artifact: [workflow run 44739871394](https://github.com/demolidorreddragons/NR/actions/runs/44739871394), artifact ID `14630496349`.
- Artifact ZIP SHA-256: `058f96b86356391653effb744aaec367859245d10de48ac7f93f2b3a02e534f8`.
- GCN code SHA-256: `5dd8cba2710502d5c1f930e4e0ddbe779a56b168900f1040c0e95b28c0f3f8bc`.
- LocalSize 8×8×1; each workgroup covers a 16×16 output tile.
- Dispatch grid: `ceil(N/16) × ceil(M/16) × 1` workgroups.
- Resource ABI: set 0 indirect table; four 16-byte V# entries at offsets 0, 64, 128, 192; table size 208 bytes.
- Table pointer: one 32-bit CS user-data SGPR at register 2 (`registercount=0`, `address32_hi=0`); register 3 must remain untouched for scalar shader parameters, and the entire table must fit below 4 GiB.

## Hard gates — no dispatch until all pass

1. **Artifact integrity:** verify `.sb` and code hashes; run the independent byte-level verifier and C++ parser against the exact binary to be loaded.
2. **CS registers:** parse the seven `GnmCsStageRegisters` words, copy code to GPU-visible memory, then patch `computepgmlo/hi` using `sceGnmCsRegsSetAddress` with a 256-byte-aligned GPU code address below 48 bits.
3. **Memory mapping:** prove CPU mapping versus GPU-visible VA. Record physical allocation offset, mapped pointer, size, alignment and protection. Do not pass the physical allocation offset as a V# base address without proof.
4. **Low-address table:** demonstrate a 208-byte GPU-visible table whose complete range is non-zero, 4-byte aligned and below 4 GiB. Emit only the low address dword into CS user register 2; never write register 3 or truncate a 64-bit pointer.
5. **Writable V# descriptors:** independently establish base address, stride, record count, data format, number format and descriptor memory/cache type. `sceGnmCreateConstBuffer` and `sceGnmCreateVertexBuffer` set `GNM_MEMORY_READONLY`; they are not approved for writable output without proof.
6. **Cache visibility:** validate the exact CPU→GPU flush and GPU→CPU invalidate/barrier sequence for the selected memory pool.
7. **Submission/completion:** prove command-buffer ownership, submit return contract, completion signal/wait, timeout and recovery. Imports link, but NR has not called them.
8. **Numerical oracle:** define tolerance before execution; compare with the host FP64 reference, check guard regions, and capture input/output/register/dispatch/completion evidence.

## Staged procedure

### Stage A — read-only preflight
- Record console model, firmware, GoldHEN version and GPU/runtime context through an already approved read-only diagnostic route.
- Confirm an approved reversible way to execute a standalone harness exists. No FTP, package install, plugin activation or network path is assumed.
- Confirm no unrelated GPU workload is active. Abort if an ephemeral launch route is unavailable.

### Stage B — temporary memory-map probe (no shader dispatch)
- Only after separate approval: allocate one temporary 64 KiB-aligned direct-memory block with explicit CPU/GPU read-write protection and candidate Garlic memory type.
- Record physical allocation offset and mapped virtual pointer separately; verify alignment, bounds and address width; release immediately.
- This does not establish the correct V# cache/memory type.

### Stage C — descriptor construction (no shader dispatch)
- Allocate a temporary low-4-GiB table range.
- Construct four V# descriptors using independently accepted field values at offsets 0/64/128/192; leave the 48-byte gaps untouched.
- Read back the 208-byte table through the CPU mapping and compare exact bytes with the host-generated expected table. Release on any failure.

### Stage D — one minimal compute dispatch
- Only after all hard gates pass, allocate code, input A/B/C and output buffers.
- Initialize small deterministic matrices; fill C with NaNs for the `beta=0` regression.
- Copy shader code, patch parsed CS registers, bind CS, emit the table pointer with `pne_gcn8_set_cs_indirect_resource_table_pointer`, and dispatch the calculated workgroup grid.
- Submit and wait using the validated completion path; invalidate/read back output; compare with the host reference and check guard regions.
- Capture hashes, register state, addresses, dispatch dimensions, output and completion status.

### Stage E — cleanup and stop conditions
- Release every temporary allocation and command buffer; do not save state or write persistent configuration.
- Stop on allocation mismatch, table address ≥4 GiB, descriptor/cache uncertainty, command-buffer overflow, submission error, timeout, GPU fault, unexpected registers or numerical mismatch.
- Do not retry automatically after a GPU fault; recovery requires a separate explicit decision.

## Current blockers

| Area | Evidence | Remaining blocker |
|---|---|---|
| Shader binary | Five GFX8 `.sb` files, CRC/container checks, raw metadata and LLVM Tonga ISA | Independent report verifier rerun pending; REA-5 not executed |
| CS registers | Seven register words parsed from real `.sb`; address patch host-tested | No GPU code allocation/address patch on console |
| Resource table | 16-byte V# layout; four offsets and 208-byte size host-tested | Writable V# field values and descriptor cache policy |
| Memory | Direct-memory APIs and Garlic candidate identified | Compute GPU VA, mapping semantics and low-4-GiB placement |
| Cache | API surface under investigation | Exact flush/invalidate/barrier sequence |
| Command buffer | CS pointer PM4 writer and GNM API imports compile/link | No command buffer submitted or dispatched |
| Completion | Submit/submit-done imports link | Fence/label wait, timeout and recovery not implemented |
| QA | Host numeric reference and malformed-input tests | No REA-5 review or PS4 Pro readback |

**Approval boundary:** this plan authorizes no console action. Separate explicit approval is required for any ephemeral harness deployment or dispatch. PKG 006 remains not approved for installation.