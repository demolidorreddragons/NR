# REA specialists — GCN8 unblock team

Updated: 2026-11-20  
Branch: `feature/gcn8-numeric-kernels`

## Added specialist responsibilities

These are project workstreams, not claims that separate autonomous agents or external contractors have been hired.

### REA-1 — Mesa / PSBC compiler reverse engineering

**Mission:** understand the pinned `opengnm-psbc` compiler internals and establish the exact SPIR-V-to-GFX8 lowering and binary metadata contract.

Tasks:
1. Trace compiler architecture and the GFX8/PS4 NEO code-generation path.
2. Keep PSBC, Mesa-derived source, OpenGNM and SPIR-V dependency revisions pinned.
3. Capture each compiler failure and distinguish source defects, generated-file omissions and dependency/API mismatches.
4. Review compiler options and confirm the `-n` PS4 NEO target is selected.
5. Once the compiler builds, compile the five NR compute shaders and inspect emitted GNM metadata: stage, registers, resource slots, user SGPRs, wave mode and descriptor class.
6. Reject conclusions not supported by compiler source or emitted binaries.

### REA-2 — GCN8 ISA / GNM binary and resource ABI

**Mission:** reverse engineer the GFX8 compute binary contract and connect it to the Orbis GNM command path without assuming Vulkan semantics map directly.

Tasks:
1. Trace the GNM shader-container format and GCN8 ISA encoding.
2. Decode actual CS register metadata, wave mode, VGPR/SGPR, LDS, scratch, user SGPRs and dispatch geometry.
3. Determine whether each shader resource is a V# buffer descriptor, RW# image/buffer descriptor, or another supported form; do not infer this from GLSL `std430`.
4. Cross-check descriptor encoding, alignment, address units and resource slots against pinned OpenGNM sources and compiler output.
5. Produce a minimal compute bring-up contract: allocation, GPU VA, mapping, cache ownership, command recording, submit, completion and error recovery.
6. Keep hardware-only claims blocked until a reversible PS4 Pro dispatch is observed.

### REA-3 — Compiler build systems / host-toolchain integration

**Mission:** make the pinned compiler build reproducibly from a clean host runner, separating upstream build-system requirements from NR-specific workarounds.

Tasks:
1. Audit the pinned PSBC Makefile/Meson/CMake assumptions and enumerate every generated source/header, including transitive Mesa and Vulkan registry outputs.
2. Verify generator inputs, Python modules, host compiler versions, linker flags and include precedence; pin versions where they affect reproducibility.
3. Fix dependency layout and missing-header integration from upstream source-of-truth manifests. Do not paper over errors with arbitrary include paths or copied system headers.
4. Add clean-runner CI checks that prove generated files are present, nonempty and deterministic (hash/diff check where practical).
5. Keep build caching optional; a cache miss must still produce a successful clean build.
6. Own the host build pipeline and its logs. Do not claim shader compilation or PS4 execution merely because the host compiler executable links.

**Boundary:** REA-3 owns build reproducibility and host-toolchain integration; REA-1 owns compiler internals, target selection and emitted compiler behavior.

### REA-4 — SPIR-V frontend / shader-lowering verification

**Mission:** prove that each NR shader's SPIR-V semantics survive PSBC's GFX8 lowering, before any runtime or performance claims.

Tasks:
1. Audit GLSL source, glslang flags, SPIR-V version/capabilities/extensions, memory model, descriptor decorations, layouts and specialization constants.
2. Build per-shader validation fixtures and preserve SPIR-V disassembly alongside compiler outputs.
3. Compare source-level intent, SPIR-V, PSBC/NIR lowering and emitted GCN ISA for address calculations, bounds, barriers, memory ordering, data types and rounding.
4. Create small differential tests against the existing CPU reference for FP32 GEMM, FP16 conversion/accumulation and E4M3 encode/decode.
5. Flag unsupported operations or silent lowering changes; never equate a successful SPIR-V validation or GNM container parse with semantic correctness.
6. Produce a shader-by-shader compatibility matrix for GFX8 and explicitly mark native FP16/packed math only when ISA evidence proves it.

**Boundary:** REA-4 owns semantic correctness from shader source through SPIR-V and lowered ISA; REA-2 owns decoding the hardware ABI and runtime resource contract.

## Current verified compiler blocker

The PSBC build in run `44675286673` (commit `ddd8ce6bf3509440079502485ab9bc600439dd0e`) passed dependency checkout, compiler configuration and all explicit Mesa code-generation steps. Compilation then failed because the Mesa-maintained Vulkan header includes:

```
vk_video/vulkan_video_codec_av1std_encode.h: No such file or directory
```

This is the next concrete build-system/dependency-layout blocker for REA-3 to resolve from the pinned Mesa/Vulkan source manifests. The compiler build did not complete; shader compilation and GNM binary generation were skipped.

## Acceptance gates

- **Host-build gate:** pinned PSBC builds successfully from a clean CI runner with all dependencies and generators accounted for.
- **Shader gate:** all five SPIR-V inputs compile to GFX8 GNM compute binaries and pass container validation.
- **Semantic gate:** source/SPIR-V/lowered ISA behavior is cross-checked against CPU references for each supported kernel.
- **Metadata gate:** actual emitted register/resource metadata is decoded and reviewed; container validity alone is insufficient.
- **Runtime gate:** a minimal, reversible GNM dispatch completes on PS4 Pro and returns a verified result.
- **Performance gate:** only then collect GPU latency, occupancy/register and throughput measurements.

No source/host test, compiler build, SPIR-V validation, or container check by itself proves execution on PS4 Pro. No PKG installation, plugin activation, or persistent console modification is authorized by these workstreams.
