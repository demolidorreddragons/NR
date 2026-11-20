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

The pinned PSBC host compiler now builds and links on a clean CI runner. Run `44680087030` passed the host compiler build, the malformed-SPIR-V CLI rejection test, generation of all five NR SPIR-V modules, and `spirv-val --target-env vulkan1.1`. PSBC emitted five non-empty PS4 NEO/GFX8 `.sb` files.

The current blocker is no longer host compilation. It is GNM input-usage metadata: the first real container stores `chunkUsageBaseOffsetDwords=78`, which resolves into the code bytes rather than the slot table written immediately after `GnmCsShader`. OpenGNM's header comment and the independent fpPS4 parser differ on whether the offset points to the table start or end, but both expected offsets for this shader exceed the 8-bit field (590 or 589 dwords). The upstream container verifier's CRC/structure pass does not validate that pointer. Issue [#8](https://github.com/demolidorreddragons/NR/issues/8) tracks the format resolution.

The integration script now patches PSBC to use the fpPS4 end-of-table calculation and fail closed when the 8-bit offset would overflow. This guard has not yet been revalidated by CI. No emitted `.sb` is accepted for runtime or hardware dispatch.

## Acceptance gates

- **Host-build gate:** PASS in run `44680087030`; pinned PSBC builds from the clean CI runner.
- **SPIR-V gate:** PASS for all five modules in run `44680087030`, including `spirv-val --target-env vulkan1.1`.
- **GNM emission gate:** partial only; five non-empty `.sb` files were emitted, but their input-usage pointer metadata is not accepted.
- **Shader gate:** BLOCKED until all five GNM binaries pass structural and slot-pointer validation.
- **Semantic gate:** source/SPIR-V/lowered ISA behavior remains unverified against CPU references for the emitted GFX8 code.
- **Metadata gate:** BLOCKED; actual register/resource metadata cannot be accepted until the input-usage pointer format is resolved and decoded.
- **Runtime gate:** a minimal, reversible GNM dispatch completes on PS4 Pro and returns a verified result.
- **Performance gate:** only then collect GPU latency, occupancy/register and throughput measurements.

No source/host test, compiler build, SPIR-V validation, or container check by itself proves execution on PS4 Pro. No PKG installation, plugin activation, or persistent console modification is authorized by these workstreams.


### REA-5 — Independent GCN8 validation specialist (clean-room reviewer)

**Mission:** independently decide whether each shader artifact is correct and safe to advance, without inheriting the project's assumptions, prior conclusions, or implementation bias.

**Independence protocol (mandatory):**
1. Start from the test specification and acceptance criteria, not the implementation team's status report or claimed conclusions.
2. Receive artifacts by immutable commit SHA and SHA-256, plus only the minimum interface/specification needed to test them. Do not use REA-1..4 conclusions as evidence.
3. Reconstruct expected results independently from shader specifications and CPU reference equations; write or select independent test vectors before inspecting implementation output.
4. Review source, SPIR-V, GNM containers and ISA artifacts independently. Record every assumption and distinguish observed facts, derived conclusions and unknowns.
5. Run negative tests: malformed/truncated containers, invalid dimensions, boundary tiles, overflow, misalignment, unsupported capabilities and corrupted artifacts.
6. Differential-test numerical outputs against independently calculated expected values, including tolerances, NaN/Inf, signed zero, subnormals and rounding behavior where applicable.
7. Do not accept CI green, compiler success, a container CRC, or a developer-provided report as proof of semantic correctness or hardware execution.
7a. Independently resolve `GnmShaderBinaryInfo.chunkUsageBaseOffsetDwords` to the actual input-usage slot table. Compare the raw pointer target with the table bytes after `GnmCsShader`; reject a pointer that lands in GCN instruction bytes even if the outer container and CRC pass. Record the start-vs-end pointer interpretation and evidence from at least two independent sources or real PS4 samples.
8. Issue a signed-off-by report with PASS / FAIL / BLOCKED per artifact and reproducible commands, tool versions, input hashes, output hashes and logs. A blocked test is not a pass.
9. The reviewer must not modify the implementation under review. Defects go back to the owning REA workstream; retesting uses a new immutable artifact hash.
10. No console dispatch is required or authorized by this role until a separately approved, reversible test plan exists.

**Required outputs:** independent test plan; test-vector corpus; machine-readable results; defect list; final per-shader verdict. The reviewer has veto authority over advancement to GNM runtime testing when evidence is incomplete.

**Independence limitation:** this is a separately defined review workstream and protocol, not a claim that an external human contractor or autonomous agent has been hired. The review must be performed in a clean context without relying on the implementation narrative; until that independent execution occurs, REA-5 remains NOT EXECUTED.
