# GFX8 compute shader artifact — run 44739871394

**Commit:** `cf2f737ab42be937941ede74256e5706b29775e8`  
**Workflow:** [GCN8 shader compiler and GEMM binary](https://github.com/demolidorreddragons/NR/actions/runs/44739871394)  
**Artifact:** `nr-gcn8-compute-shaders-gfx8`, ID `14630496349`  
**Artifact ZIP SHA-256:** `058f96b86356391653effb744aaec367859245d10de48ac7f93f2b3a02e534f8`  
**Compiler:** PSBC `28697f007862427ebec54388ec972ce6e3545912`, OpenGNM headers `4b295ca54c82c83acf308d1c646a2dfa9ae57350`.

## Verified in CI

- Pinned PSBC host compiler built successfully on Ubuntu 24.04.
- All five GLSL sources compiled to SPIR-V and passed `spirv-val --target-env vulkan1.1`.
- All five outputs were converted with PSBC `-s compute -n` (PS4 NEO/GFX8).
- All five `.sb` files passed the pinned upstream `verify_sb.py` structural/CRC checks.
- NR's stricter validation resolved each `chunkUsageBaseOffsetDwords` to the actual input-usage table at `0x58`; each table is one `PTR_INDIRECTRESOURCETABLE` slot, start register 2.
- SPIR-V `LocalSize` matched the three GNM compute thread dimensions for all five shaders.
- LLVM 18.1.3 disassembled all five code ranges with `--triple=amdgcn-- --mcpu=tonga`; five raw `.gcn.bin` and five `.isa` files were uploaded.
- The negative corrupted-CRC control was rejected as expected.

## Per-shader results

| Shader | `.sb` bytes | GCN code bytes | Chunk offset (dwords) | CRC32 | VGPR allocation | SGPR allocation | LDS allocation | Scratch/thread | ISA lines |
|---|---:|---:|---:|---|---:|---:|---:|---:|---:|
| `gemm_fp32_wave64_tiled` | 1,096 | 948 | 238 | `1f11651b` | 16 | 32 | 2,048 B | 0 B | 192 |
| `gemm_fp32` | 440 | 292 | 74 | `1fa780f6` | 8 | 32 | 0 B | 0 B | 56 |
| `gemm_fp16_accum` | 512 | 364 | 92 | `922291e1` | 8 | 32 | 0 B | 0 B | 67 |
| `e4m3_decode` | 1,004 | 856 | 215 | `ffb3f51a` | 8 | 32 | 0 B | 0 B | 147 |
| `e4m3_encode` | 688 | 540 | 136 | `c3e62f64` | 8 | 32 | 0 B | 0 B | 106 |

The one-slot pointer has a hard maximum of 255 dwords (1,020 bytes from `OrbShdr` to the slot-table start). The current tiled kernel uses 238 dwords / 948 code bytes, leaving 17 dwords (68 bytes) of margin. CI retains the fail-closed overflow guard; do not enlarge or reconfigure this shader without revalidating the field.

## Tiled kernel register snapshot

For `gemm_fp32_wave64_tiled`:

- GNM `OrbShdr` offset: `0x410`; code size: 948 bytes; input-slot offset: 238 dwords; slot resolves to `0x58`.
- GNM thread dimensions and SPIR-V LocalSize: `8 × 8 × 1` (one wave64 workgroup).
- `COMPUTE_PGM_RSRC1` raw: `0x002c00c3`; decoded GFX8 VGPR field 3 → 16 allocated VGPRs, SGPR field 3 → 32 allocated SGPRs.
- `COMPUTE_PGM_RSRC2` raw: `0x00020990`; decoded LDS field 4 → 2,048 bytes, user-SGPR count 8, scratch disabled, trap disabled.
- Input usage: `PTR_INDIRECTRESOURCETABLE`, API slot 0, start register 2.
- GCN code SHA-256: `5dd8cba2710502d5c1f930e4e0ddbe779a56b168900f1040c0e95b28c0f3f8bc`.
- ISA SHA-256: `62b6b46fab5b5c94eed6c3b7a25561f205d6e4dd7fd0c5e12d2fbd393de394a4`.

The report decodes register bitfields using the pinned Mesa `src/amd/registers/gfx8.json`; the allocations are compiler metadata estimates, not measured occupancy or runtime allocations.

## Tool provenance

- Host: Linux 7.0.0 Azure runner, glibc 2.39; Python 3.12.3; GCC 13.3.0.
- glslang: 15.1.0.
- SPIRV-Tools: v2025.1.
- LLVM MC: 18.1.3, AMDGCN Tonga target.
- PSBC CLI: 0.1.0; binary SHA-256 `7bb89bc0bd73bcb691f7fafa64086040257b76b5a99ced932bd17a63c389e2bf`.
- NR PSBC patch script SHA-256: `4a532ba441601ff56d58c6d86c0a5f1fdf1250e3af745c9d588bda07b9f632ea`.

## Explicit limits

This is the first successful **host-side GFX8 code-object/container and ISA extraction** milestone. It does not prove:
- correct GNM RW descriptor class or full storage-buffer descriptor ABI;
- correct GPU virtual-address mapping, memory protection, cache coherence or command submission;
- successful runtime load/dispatch or numerical output on a PS4 Pro;
- native FP16 arithmetic or FP8 hardware support;
- REA-5 independent clean-room review (still **NOT EXECUTED**);
- integration of the 71-block graph, neural inference, frame input/output or renderer.

No console dispatch, PKG installation, plugin activation or persistent console modification occurred.