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

The GCN8 compiler workflow run `44676657053` reached C compilation after all Mesa generated-header steps passed, then failed in `src/util/os_misc.c` and `src/util/os_misc.c`'s page-size path (`os_get_page_size`) with `#error unexpected platform`. Inspection of the pinned source shows both branches are guarded by `HAVE_SYSCONF`; the Linux host provides POSIX `sysconf(_SC_PHYS_PAGES/_SC_PAGESIZE)`, but the manually-authored host Makefile configuration did not define the feature macro. The pinned repository's own `config.orbis.mak` also defines `HAVE_SYSCONF=1` for its Orbis cross-build, corroborating that this feature macro is part of the compiler project's expected configuration.

The NR workflow now defines `HAVE_SYSCONF=1` in its Linux-host compiler flags (commit `cb66861b05fbe9601300b3bce39f99cc212447d4`). This is scoped to the Linux CI host configuration; it does not change the PS4 target's runtime ABI or claim that PS4 supports the same host feature. The next workflow run must verify the correction and reveal any subsequent build blockers. No PSBC executable or shader binary is yet accepted.


The workflow now also compiles and executes a small Linux `sysconf` probe before compiling PSBC (commit `9b6a3b96ada08fb47afb5eed3563ae4b72a777d9`). This makes the host capability explicit instead of relying only on a preprocessor define. Host build flags and the PS4 NEO/GFX8 output target remain separate: the compiler executable is built for the Linux runner, while the later PSBC invocation uses `-n` to select PS4 NEO/GFX8 output.
