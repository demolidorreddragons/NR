# REA compiler-path findings — PSBC / GFX8

Updated: 2026-11-20  
Pinned compiler: `PS4-OpenGNM/opengnm-psbc` at `28697f007862427ebec54388ec972ce6e3545912`  
Pinned OpenGNM headers: `4b295ca54c82c83acf308d1c646a2dfa9ae57350`

## Verified from the pinned compiler source

1. The CLI defaults to PS5. Its `-n` switch explicitly selects `PSBC_TARGET_PS4_NEO`; the help text describes this as PS4 Pro (NEO/GFX8). The shader workflow's compiler invocation includes `-n`, so it does not silently compile for the default RDNA2 target.
2. The PS4 NEO target setup selects `GFX8`, `CHIP_TONGA`, and `neo=true`. The GFX8 compiler limits in this revision include wave64 VGPR allocation and 4 SIMDs per CU in the compiler model.
3. The GFX8 setup branch does not set `has_packed_math_16bit` to true (unlike its GFX10+ branch). Therefore, do not claim native packed FP16 arithmetic from this target setup. Inspect emitted ISA and compiler lowering before deciding whether the FP16 kernel is actually native, lowered, or rejected.
4. PSBC's Mesa format build defines `u_format_gen.h` as a generated target in `src/util/format/meson.build`: run `u_format_table.py u_format.yaml --enums`, capturing stdout to the generated header.

## Current build evidence

- The first PSBC build failed because `u_format_gen.h` had not been generated; explicit Mesa generation was added to CI.
- Later clean-runner attempts exposed additional generated Mesa headers/sources and Vulkan include-order issues; deterministic generation steps and Mesa-first Vulkan include precedence were added.
- Run `44675286673` on commit `ddd8ce6bf3509440079502485ab9bc600439dd0e` completed dependency checkout, compiler configuration and all explicit Mesa generation steps, then failed during C compilation:
  `include/mesa/vulkan/vulkan_core.h:13641:10: fatal error: vk_video/vulkan_video_codec_av1std_encode.h: No such file or directory`.
- The failure is now narrowed to the missing `vk_video` header set expected by this pinned Mesa Vulkan header. No compiler executable or GNM shader binaries are claimed.
- Because the host compiler build failed, SPIR-V compilation, PS4 NEO GFX8 shader compilation and GNM container/metadata reporting were skipped.

## Vulkan video-header dependency correction

The failed run showed that the Mesa Vulkan headers at the pinned PSBC revision include `vk_video/vulkan_video_codec_av1std_encode.h`, which is absent from Vulkan-Headers `v1.3.296`. The GCN8 workflow now pins Vulkan-Headers `v1.4.328` and asserts that this exact header exists before compiling PSBC. This is a dependency compatibility correction only; the next CI run must establish whether the full PSBC host build succeeds. The newer headers are not evidence of Vulkan runtime use on PS4.

## Required follow-up from compiler output

Once PSBC builds:
- Compile the FP32 baseline first and inspect its GFX8 code object/container.
- Record actual wave mode, VGPR/SGPR counts, scratch use, LDS use, user SGPR layout, resource slots and descriptor class from emitted metadata.
- Compare the FP16 shader's emitted ISA with FP32; source-level `float16_t` or extension use is not proof of native FP16 instructions.
- Confirm the E4M3 encode/decode shaders are numerical conversion routines implemented in supported GFX8 instructions, not NVIDIA FP8 hardware operations.
- Confirm shader source → SPIR-V → lowered ISA semantics against CPU reference tests.
- Keep SPIR-V validation, GNM container validation, shader metadata inspection, semantic differential testing, and real PS4 Pro dispatch as separate acceptance gates.

## Explicit non-claims

This source audit does not prove that the compiled binaries execute on the console, that PSBC's standalone descriptor layout matches the eventual GNM runtime bindings, or that the selected Tonga/GFX8 target perfectly models every PS4 Pro hardware stepping. Those require emitted-binary inspection and hardware validation.


## Build blocker: AMD packet headers (2026-11-20)

The GCN8 compiler workflow run `44675683282` was inspected from its job log. The pinned PSBC build generated the Mesa register, NIR, ACO, and SPIR-V files successfully, then failed at the first C compilation unit:

```
src/amd/common/sid.h:12:10: fatal error: amd_cp_packets_gfx11.h: No such file or directory
```

This is a missing code-generation step, not evidence of a GFX8 ISA incompatibility. The pinned PSBC source's `src/amd/common/meson.build` defines `amd_packet_headers` custom targets for both `gfx11` and `gfx12`. Each target runs `src/amd/packets/parse_cp_pm4_table_data_json.py` with the GFX11/GFX12 packet JSON and opcode header inputs, then emits `src/amd/common/amd_cp_packets_gfx11.h` and `amd_cp_packets_gfx12.h`. The Makefile's `GENERATED` list does not include these two headers.

The NR workflow was amended in commit `9a02161f1ea2ebb297609acbdbb9163b86510e98` to reproduce those two upstream Meson code-generation targets before `make`, and assert both outputs are non-empty. This is a targeted build-system correction based on the pinned source's own Meson definition; it does not patch compiler semantics or remove GFX11/GFX12 declarations.

**Validation state:** the correction has been committed, but no workflow run on this new commit has yet been observed. Therefore the PSBC host compiler build remains **BLOCKED / NOT YET REVALIDATED**. No SPIR-V-to-GFX8 compilation, `.sb` artifact, GNM metadata inspection, or PS4 Pro GPU execution is claimed.


## Build blocker: host sysconf feature detection (2026-11-20)

The GCN8 compiler workflow run `44676657053` reached C compilation after all Mesa generated-header steps passed, then failed in `src/util/os_misc.c` and `src/util/os_misc.c`'s `os_get_page_size` code path (the diagnostic names `os_sysinfo.c`) with `#error unexpected platform`. Inspection of the pinned source shows both branches are guarded by `HAVE_SYSCONF`; the Linux host provides POSIX `sysconf(_SC_PHYS_PAGES/_SC_PAGESIZE)`, but the manually-authored host Makefile configuration did not define the feature macro. The pinned repository's own `config.orbis.mak` also defines `HAVE_SYSCONF=1` for its Orbis cross-build, corroborating that this feature macro is part of the compiler project's expected configuration.

The NR workflow now defines `HAVE_SYSCONF=1` in its Linux-host compiler flags (commit `cb66861b05fbe9601300b3bce39f99cc212447d4`). This is scoped to the Linux CI host configuration; it does not change the PS4 target's runtime ABI or claim that PS4 supports the same host feature. The next workflow run must verify the correction and reveal any subsequent build blockers. No PSBC executable or shader binary is yet accepted.


The workflow now also compiles and executes a small Linux `sysconf` probe before compiling PSBC (commit `9b6a3b96ada08fb47afb5eed3563ae4b72a777d9`). This makes the host capability explicit instead of relying only on a preprocessor define. Host build flags and the PS4 NEO/GFX8 output target remain separate: the compiler executable is built for the Linux runner, while the later PSBC invocation uses `-n` to select PS4 NEO/GFX8 output.


## Numeric shader review — beta-zero NaN isolation (2026-11-20)

Source review found that the scalar FP32 and FP16-emulation GEMM shaders evaluated `beta * C` even when `beta == 0`. IEEE arithmetic makes `0 * NaN` produce NaN, so an unused/uninitialized C buffer could contaminate the output. Both shaders now conditionally avoid reading C when beta is zero (commits `07c83b144dd5712b4ac46cc227a1f6e9e0e468ce` and `92a3bc3fdfb925f5326532bc06da5c20dcbd94ee`). Host regression coverage now includes beta-zero/NaN isolation, E4M3FN signed zero, infinities, NaN, and tie-to-even boundaries. Host CI run `44677189726` completed successfully on commit `92a3bc3fdfb925f5326532bc06da5c20dcbd94ee`. This validates the host regression suite only; the shader compiler workflow has not yet produced GFX8 binaries.


## First successful host compiler build and GFX8 conversion blocker (2026-11-20)

Run `44679024542` completed the pinned PSBC host compiler build and link. The workflow had to generate NIR/ACO C sources before starting a fresh Make process (the Makefile's source wildcards are evaluated at parse time), generate Mesa's `format_srgb.c` and `gfx10_format_table.c`, and select BLAKE3's portable-only path because the standalone Makefile omitted the upstream x86 SIMD assembly objects. These are host build integration changes; they do not alter the PS4 shader instruction set.

The five NR compute sources compiled to SPIR-V and all five passed `spirv-val --target-env vulkan1.1`. GNM conversion then aborted on the tiled FP32 GEMM shader at:

```
assert(config->lds_size <= compiler_info->ac->lds_size_per_workgroup)
```

Inspection of the pinned `libpsbc/psbc_compile.c` showed that its standalone `setup_ac_info()` populated VGPR/SGPR limits but left `lds_size_per_workgroup` zero for every target. The authoritative pinned Mesa implementation in `src/amd/common/ac_gpu_info.c` sets this limit to 64 KiB for GFX7+ and 32 KiB for GFX6. The new local integration script `tools/apply_psbc_gfx8_fixes.py` sets the same value in the standalone target setup before building. This is a narrowly scoped correction to compiler capability metadata, not a change to the shader or an invented LDS size.

The same run showed the negative CLI test's 4-byte SPIR-V fixture reached NIR/ACO and caused a process crash; the previous shell gate accepted any nonzero exit and therefore misclassified the crash as rejection. The integration script now validates SPIR-V file length and the 5-word header before calling libpsbc, and the workflow requires a normal nonzero exit (not a signal/segfault) with no output file. The script hash is recorded in the GNM provenance report.

**Validation state:** the host compiler and SPIR-V gates passed in run `44679024542`; the LDS and CLI patches have not yet been revalidated. No GNM `.sb` artifact, ISA, register report, or hardware dispatch is accepted.


## First five PS4 NEO/GFX8 shader binaries emitted (run 44680087030)

The pinned PSBC host compiler now builds after the LDS metadata correction. The truncated-SPIR-V CLI negative test passes (normal diagnostic exit, no output binary, no signal crash). All five NR compute shaders compile to SPIR-V, pass `spirv-val --target-env vulkan1.1`, and are converted by PSBC `-n -s compute` into five non-empty `.sb` outputs. The SPIR-V provenance manifest and artifact upload also pass.

The next custom container/metadata step failed on the first tiled GEMM binary after the upstream pinned `tests/verify_sb.py` had already reported the GNM container structure and PS4 CRC valid. The failure was in NR's additional assertion: it assumed `chunkUsageBaseOffsetDwords` points immediately after the `GnmCsShader` header and before the GCN code range. Actual emitted metadata was `OrbShdr=0x990`, `chunkoff=78 dwords`, `numslots=1`, so the slot table offset is `0x858`, inside the code/chunk region. The format's offset is relative to OrbShdr; the correct bounds are `code_start <= slot_offset` and `slot_offset + num_slots*4 <= orbshdr_offset`. The workflow now uses those bounds and uploads available shader artifacts even if later QA fails.

**What this proves:** PSBC can emit five non-empty PS4 NEO/GFX8 compute shader containers from the five NR SPIR-V inputs; one actual container passed the upstream structural/CRC verifier. **What it does not prove:** complete validation of all five containers, independent register/ISA correctness, GNM resource binding, runtime dispatch, GPU execution, numerical correctness on PS4 Pro, or renderer integration. The corrected custom validator awaits CI rerun.


## Correction: GNM input-usage offset is invalid, not a validator false positive

The initial interpretation of `chunkUsageBaseOffsetDwords` as an arbitrary in-range pointer inside the GCN byte stream was too permissive and is withdrawn. The pinned OpenGNM header declares the field as an 8-bit value. The independent fpPS4 parser resolves the slot table as `OrbShdr - chunkUsageBaseOffsetDwords*4 - numInputUsageSlots*4`, meaning the stored offset points to the end of the slot table. The PSBC writer places the single `GnmInputUsageSlot` immediately after the 0x24-byte `GnmCsShader` header (absolute offset 0x58 in this binary), so the table ends at 0x5c. With `OrbShdr` at 0x990, the correct offset is `(0x990 - 0x5c)/4 = 589` dwords. The emitted field is only 8 bits and contains 78, which resolves to 0x854 (end of table) and is not the actual table at 0x58–0x5c.

This is a real metadata-generation defect in the pinned PSBC writer: it calculates a wider offset from the start of the slot table, then assigns it to an 8-bit field without checking range (and without accounting for the slot count in the end-pointer convention). The upstream `verify_sb.py` only checks that the resolved location is somewhere after the header; its CRC pass does not validate that the pointer resolves to the actual input-usage table. Therefore the prior statement that the custom check was a false rejection is withdrawn. A valid CRC and non-empty `.sb` do not make the container's resource metadata valid.

The NR validator now uses the independent end-of-table formula, rejects overflow beyond 255 dwords, and does not accept a pointer into the instruction byte stream. GNM binaries from run `44680087030` are diagnostic outputs only and are **not accepted for runtime**. The PSBC writer/layout must be corrected before task 67–70 can pass. The fix is not yet known to be safe; no speculative relocation of slot tables or code-size semantics will be accepted without format evidence and independent verification.


## Fail-closed compiler guard added

The PSBC integration patch now computes the offset to the **end** of the input-usage slot table, matching the independent resolver formula, and returns `PSBC_RESULT_INTERNAL_ERROR` instead of narrowing a value greater than `0xff` into the 8-bit field. The NR workflow also asserts that this guard is present in the patched compiler. This is a fail-closed safety fix, not a layout solution: the current five shaders still exceed the representable range, so the compiler is expected to refuse them until the GNM binary layout is corrected. CI revalidation is pending (workflow patch commit `7aea386791dd4d712640239414281450c2031190`). Tracking issue: [#8](https://github.com/demolidorreddragons/NR/issues/8).


### Offset interpretation caveat

The independent sources are not fully consistent: the OpenGNM header comment says `chunkusagebaseoffsetdwords` points to `OrbShdr - offset*4`, while fpPS4's `_calc_shader_slot()` subtracts `numInputUsageSlots` additional dwords to obtain the slot start. Therefore the expected offset is 590 dwords under the start-pointer interpretation or 589 under the end-pointer interpretation. Both are outside the 8-bit field and both conflict with the emitted value 78, which resolves to 0x858 or 0x854 respectively instead of the slot table written at 0x58..0x5c. The fail-closed guard uses the fpPS4 end-pointer formula, but the exact field semantics still require confirmation from a real PS4 sample or another independent parser. This uncertainty is part of blocker issue [#8](https://github.com/demolidorreddragons/NR/issues/8).


## Artifact-based resolution of the slot-pointer convention (run 44681174240)

Inspection of the uploaded diagnostic `.sb` files resolved the earlier start-vs-end ambiguity for the pinned PSBC/OpenGNM format. Four short shader binaries have the input-usage slot table at `0x58` and their stored offsets resolve exactly to that start address under the OpenGNM header comment's formula `OrbShdr - offset*4`: decode 215, encode 136, FP16 GEMM 92, scalar FP32 GEMM 74. The fpPS4 formula that subtracts the slot count resolves each of these four one dword too early (`0x54`), so it is not the correct interpretation for these emitted binaries and remains a discrepancy to investigate against real retail samples.

The tiled FP32 GEMM has `OrbShdr=0x990`, actual slot table `0x58..0x5c`, expected start-pointer offset `(0x990-0x58)/4=590), but the stored 8-bit field is 78. It therefore resolves to `0x858` in the code region. This confirms an 8-bit truncation bug in PSBC's writer, not a false-positive bounds check. The validator and fail-closed compiler guard have been corrected to use the start-pointer convention, corroborated by the four short outputs, and reject offsets above 255. The previous note saying the guard used the fpPS4 end-pointer interpretation is superseded by this artifact-based finding.


### Diagnostic artifact hashes — run 44681174240

Artifact `nr-gcn8-compute-shaders-gfx8` (ID `14604397439`) ZIP SHA-256: `015855d5d8177056f0f7babf235ef87e62e5f294dc771fd47b3610a71ba45426`. The five emitted `.sb` files are diagnostic only:

| Shader | Bytes | SHA-256 |
|---|---:|---|
| `gemm_fp32_wave64_tiled.sb` | 2,504 | `530ba36073bc1e0f1a564039016d987f6d7bda90e373291a8509c8576d2f196b` |
| `gemm_fp32.sb` | 440 | `d0f22febb2a94524a52ed59f8b77b04c2c28eb2a69828e81a0cfc2d9ef60e9e2` |
| `gemm_fp16_accum.sb` | 512 | `b3a2734bd6008806e2bac44f32b11bc5183b2534189b2cebd06329f04286bf52` |
| `e4m3_decode.sb` | 1,004 | `268b0ff3c750a873afee6f355290ee31dd0712d3c01853513e6ecdd3cc86e03e` |
| `e4m3_encode.sb` | 688 | `5be6431766df62ab812f197511273481c91453526bd1990f8f5e57685e1ed0f2` |

These hashes identify the exact outputs used to derive the pointer offsets above. They do not indicate GNM metadata acceptance, runtime compatibility, or hardware execution.


## Fail-closed GNM offset guard verified in CI — run 44682060810

The latest pinned compiler workflow successfully applied the start-pointer guard, rebuilt PSBC, passed the malformed-SPIR-V CLI test, generated all five SPIR-V modules, and passed `spirv-val --target-env vulkan1.1`. During GNM conversion, the tiled shader was refused with the exact diagnostic:

```
PSBC: GNM input-usage offset 590 dwords exceeds 8-bit field; refusing invalid shader binary
Shader compilation failed: internal error
```

No `.sb` file was emitted. The GNM diagnostic artifact (ID `14605031783`, ZIP SHA-256 `50f3d70d9042b91ed9ec8db960da908ed0698f1a05fc53ca854a3934b48f6554`) contains only the five validated SPIR-V modules: `e4m3_decode`, `e4m3_encode`, `gemm_fp16_accum`, `gemm_fp32`, and `gemm_fp32_wave64_tiled`. The separate SPIR-V artifact (ID `14604462717`) also passed upload. This confirms the fail-closed behavior; it does not solve the long-shader GNM layout. The workflow remains failed at GNM conversion by design until issue [#8](https://github.com/demolidorreddragons/NR/issues/8) is resolved.
