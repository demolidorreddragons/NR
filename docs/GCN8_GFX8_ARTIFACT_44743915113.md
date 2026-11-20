# GFX8 compute artifact — run 44743915113

**Workflow:** [GCN8 shader compiler and GEMM binary](https://github.com/demolidorreddragons/NR/actions/runs/44743915113)  
**Commit:** `5141ed44fe3202dbdec78f0a69700d34ef76e039` (workflow head)  
**Artifact:** `nr-gcn8-compute-shaders-gfx8`, ID `14631648737`  
**Artifact ZIP SHA-256:** `a722c2e67e8138b086cd8c6569850bb054277298339b4a23ea8e45d9ce337586`  
**Compiler pins:** PSBC `28697f007862427ebec54388ec972ce6e3545912`; OpenGNM `4b295ca54c82c83acf308d1c646a2dfa9ae57350`; SPIRV-Headers `09913f088a1197aba4aefd300a876b2ebbaa3391`; Vulkan-Headers `v1.4.328`.

## CI acceptance evidence

- PSBC host compiler built successfully; malformed CLI input was rejected.
- All five GLSL sources compiled to SPIR-V and passed `spirv-val --target-env vulkan1.1`.
- All five shaders converted using PSBC `-s compute -n` for PS4 NEO/GFX8.
- All five `.sb` containers passed upstream structural/CRC verification and NR's input-usage pointer checks.
- The C++ `shader_binary_contract` parser passed against all five real `.sb` files, validating NEO compute headers, CS register blocks, the indirect resource-table slot, the 8-bit slot pointer and code ranges.
- `verify_gfx8_report.py` independently cross-checked the report against actual `.sb`, `.spv`, `.gcn.bin`, `.isa` and source bytes; it also re-decoded `COMPUTE_PGM_RSRC1/2` from raw registers using the pinned Mesa GFX8 register database and checked derived allocations.
- LLVM 18.1.3 disassembled all five GCN code ranges for AMDGCN Tonga.
- The negative corrupted-CRC control was rejected.

## Per-shader artifact

| Shader | `.sb` bytes | GCN code bytes | Slot offset (dwords) | CRC32 | VGPRs | SGPRs | LDS | Scratch/thread |
|---|---:|---:|---:|---|---:|---:|---:|---:|
| `gemm_fp32_wave64_tiled` | 1,096 | 948 | 238 | `1f11651b` | 16 | 32 | 2,048 B | 0 B |
| `gemm_fp32` | 440 | 292 | 74 | `1fa780f6` | 8 | 32 | 0 B | 0 B |
| `gemm_fp16_accum` | 512 | 364 | 92 | `922291e1` | 8 | 32 | 0 B | 0 B |
| `e4m3_decode` | 1,004 | 856 | 215 | `ffb3f51a` | 8 | 32 | 0 B | 0 B |
| `e4m3_encode` | 688 | 540 | 136 | `c3e62f64` | 8 | 32 | 0 B | 0 B |

All five binaries declare `PTR_INDIRECTRESOURCETABLE`, API slot 0, start register 2. For tiled GEMM, the code occupies 948 bytes and the slot pointer is 238 dwords, below the 255-dword limit by 17 dwords (68 bytes). The workgroup LocalSize is 8×8×1.

## Tiled GEMM register snapshot

- `COMPUTE_PGM_RSRC1`: `0x002c00c3` → 16 VGPRs, 32 SGPRs.
- `COMPUTE_PGM_RSRC2`: `0x00020990` → 2,048 B LDS, 8 user SGPRs, scratch disabled, trap disabled.
- Wavefront: 64; code SHA-256 `5dd8cba2710502d5c1f930e4e0ddbe779a56b168900f1040c0e95b28c0f3f8bc`.
- ISA SHA-256 `62b6b46fab5b5c94eed6c3b7a25561f205d6e4dd7fd0c5e12d2fbd393de394a4`.

## Explicit limits

This is host/compiler evidence only. It does **not** prove GPU memory allocation/mapping, writable V# descriptor field values, cache coherence, CS binding on hardware, command submission, completion, numerical output on PS4 Pro, native FP16/FP8 support, full 71-block inference, renderer integration, or independent REA-5 approval. REA-5 remains **NOT EXECUTED**. No console dispatch, PKG installation, plugin activation or persistent console modification occurred.