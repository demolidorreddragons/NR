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
