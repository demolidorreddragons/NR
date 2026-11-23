# GCN8 GNM Compute Readiness Audit

Date: 2026-11-23  
Base commit: `d0d261bd92f88e1e4e9c1d509db4d1d37093d84f`

## Executive finding

**Real GNM compute execution and Neural Rendering on the PS4 Pro GPU are not implemented or demonstrated by the current repository.** The existing GoldHEN PRX is a diagnostics/capture scaffold. It hooks `sceGnmSubmitAndFlipCommandBuffers` to record present timing and display-buffer metadata. It does not bind a compute shader, dispatch workgroups, synchronize GPU writes, read back compute output, or perform neural inference.

This is a source-audit finding, not a hardware test.

## Evidence inspected

- `platform/ps4/goldhen/plugin/README.md` explicitly says no GNM compute backend, neural inference runtime, temporal reconstruction, or generic framebuffer interception is implemented.
- `platform/ps4/goldhen/plugin/main.c` installs only a present-submit hook and calls `pne_telemetry_on_present(...)`. No compute dispatch or shader binding appears in this entry point.
- `include/pne/telemetry.hpp`, `src/telemetry.cpp`, and `src/telemetry_bridge.cpp` implement CSV frame-time logging and an RGBA8 BMP writer. The readback submission function accepts already-readable CPU memory; it does not acquire or synchronize GPU memory.
- `.github/workflows/stage-gcn8-gnm-link-001.yml` describes a GNM-load-only diagnostic PKG. Its independent QA contract explicitly requires `STATIC_GNM_IMPORT=ABSENT` and `COMPUTE_SHADER=ABSENT`. That artifact cannot prove a GNM compute dispatch.
- The current plugin README records that PS4 firmware 14.02 loading and title-specific GPU readback are still unverified.

## External API and compiler research

The following references establish useful interfaces and candidate tooling, but **do not establish that this repository can safely use them on the user's console**:

1. OpenOrbis `GnmDriver.h` declares GNM entry points, including `sceGnmDispatchDirect`. The header contains placeholder declarations for several APIs and does not provide a working compute implementation by itself: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/include/orbis/GnmDriver.h
2. The opengnm API reference documents compute shader binding and dispatch interfaces (`sceGnmSetCsShader`, `sceGnmDispatchDirect`): https://ps4-opengnm.github.io/opengnm/reference/driver/
3. Compiler candidate **PS4-OpenGNM/opengnm-psbc**, pinned source revision `28697f007862427ebec54388ec972ce6e3545912` (2026-10-23), documents a compute stage and an explicit `-n` PS4 Pro/NEO (GFX8) target. Its repository includes `tests/test.comp`, a 64x1x1 compute shader, and a shader-binary verifier that recognizes compute stage type and NEO target metadata. The same README's hardware-validation claim is limited to VS/PS triangle rendering on PS4 FW 9.00/GFX7; it does **not** claim compute dispatch, GFX8 compute hardware validation, or FW 14.02 validation: https://github.com/PS4-OpenGNM/opengnm-psbc/tree/28697f007862427ebec54388ec972ce6e3545912
4. The opengnm API reference documents compute shader binding and dispatch interfaces (`sceGnmSetCsShader`, `sceGnmDispatchDirect`): https://ps4-opengnm.github.io/opengnm/reference/driver/
5. The opengnm documentation distinguishes a firmware-backed Orbis backend from a generic software backend. A generic/host test cannot substitute for a retail-console GPU test: https://ps4-opengnm.github.io/opengnm/backends/

The compiler candidate is research-only. The inspected checkout has a reproducibility gap: the top-level Makefile includes an untracked `config.mak` by default, and its code-generation rules reference a sibling `../SPIRV-Headers` tree; the PS4 cross-build configuration additionally expects sibling opengnm and Vulkan-Headers trees. The README's bare `make` command therefore does not, by itself, define a reproducible clean-checkout build.

### Gate A automation added

`.github/workflows/gcn8-compute-artifact-gate.yml` now defines an isolated hosted-runner build for the pinned compiler candidate. The independent verifier is stored under `tests/` (not `tools/`) so merging this gate cannot accidentally trigger the repository's separate PS4 PRX/PKG build path. It checks out:
- `opengnm-psbc`: `28697f007862427ebec54388ec972ce6e3545912`
- `opengnm` headers: `4b295ca54c82c83acf308d1c646a2dfa9ae57350`
- `SPIRV-Headers`: `fb1379425b16e58dcbdda7be0f8604a7f5cdce78`
- `Vulkan-Headers`: `d277a0a3cda9c682708c93279f0c72cd6b0ae3f3`

The workflow supplies an explicit host `config.mak`, generates the Mesa format outputs, shader statistics header, sRGB helper, and GLSL `builtin_types.h/.c` omitted from the candidate Makefile's generated-source list, compiles a deterministic compute shader with `-n`, and checks both compute stage and NEO/GFX8 target byte in addition to the upstream container/CRC verifier. It retains logs, SPIR-V, shader binary, report, and SHA-256. It runs only on `ubuntu-24.04`; no third-party source is executed on the privileged PS4-LAN runner.

Automated build attempts `45200242052`, `45200602970`, `45201076372`, and `45201396827` failed before shader compilation. Their logs exposed missing generated Mesa/GLSL files in the candidate's incomplete Makefile recipe (`u_format_gen.h`, `builtin_types.h/.c`, `shader_stats.h`) an unpinned sibling `Vulkan-Headers` dependency (`vk_video/vulkan_video_codec_h264std.h`), and a missing Vulkan utility generated header (`vk_struct_type_cast.h`). The workflow now reproduces the relevant pinned Mesa/GLSL/Vulkan Meson generator outputs and checks out the exact Vulkan-Headers revision before `make`. Run `45201959348` reached the generator step and exposed a workflow-script defect (`ROOT` was not initialized in that step); this is not a compiler failure. The variable is now initialized explicitly. Gate A remains unpassed until a complete build, shader compile, target check, and CRC verification succeed.

**Scope limitation:** Gate A proves only that the pinned compiler emits a structurally valid GFX8/NEO compute shader container. It does not prove the binary executes, that firmware 14.02 accepts it, or that a GPU dispatch/readback works. No shader binary is staged to the console by this workflow.

## Required engineering sequence

### Gate A — reproducible compute shader artifact

- [x] Pin compiler source and dependency revisions.
- [x] Add isolated hosted build and GFX8/NEO target verification workflow.
- [ ] Corrected workflow run after fixing the omitted Mesa header generation; inspect build log, output binary, report, and digest.
- [ ] Reproduce independently before trusting the generated shader.
- Reject output that targets GFX7/base PS4 or GFX10+/PS5.

### Gate B — command submission design

Additional pinned-source finding: in `PS4-OpenGNM/opengnm` commit `4b295ca54c82c83acf308d1c646a2dfa9ae57350`, the generic backend contains PM4 packet construction for `sceGnmSetCsShader` and `sceGnmDispatchDirect`, but its `sceGnmSubmitCommandBuffers` implementation is explicitly a no-op and its compute-queue mapping returns `GNM_ERROR_UNSUPPORTED`. The Orbis backend declares/forwards to real `libSceGnmDriver` firmware entry points. Thus generic backend tests cannot establish GPU execution; the firmware-backed path, buffer residency, exact 14.02 ABI, and completion/readback path still need independent proof. See `src/driver_generic.c`, `src/driver_orbis.c`, and `docs/backends.md` at that pinned commit.

Before adding code to the PRX, prove from pinned headers/source and ABI evidence:

- how this title/process obtains a valid GNM command buffer and GPU-visible allocations;
- the correct compute shader register setup and user-data/resource binding;
- the valid dispatch packet size and workgroup dimensions;
- resource cache policy, GPU barriers/fences, completion signaling, and timeout/recovery behavior;
- how GPU output becomes CPU-readable without dereferencing a GPU virtual address as a CPU pointer;
- that the method is valid for retail PS4 Pro firmware 14.02, not merely a devkit or emulator.

Do not invent a standalone queue or call undocumented firmware exports based only on symbol names.

### Gate C — isolated known-answer hardware proof

The first console test must be a reversible, opt-in diagnostic workload, not a neural renderer and not a game framebuffer hook. It must dispatch a deterministic kernel, wait for GPU completion using a proven synchronization path, read back through a proven transfer/coherency path, and compare every output element with a CPU reference.

Required evidence: console model/firmware, plugin load/unload result, dispatch return/status, GPU completion/fence evidence, input/output dimensions, CPU-reference comparison, output hash, kernel duration, and crash/error logs. A successful PRX load or a host-side simulation is not a compute pass.

### Gate D — Neural Rendering integration

Only after Gate C passes:

- define the title-specific render-target adapter and supported formats;
- prove GPU-to-CPU readback and cache coherency on that title;
- implement and benchmark the selected inference/reconstruction kernel;
- validate frame pacing, latency, memory pressure, thermal behavior, and visual output against a captured baseline;
- preserve a disable/recovery path and avoid changing saves or unrelated GoldHEN state.

## Explicit non-claims

This audit does **not** claim:

- that a compute shader has been executed on GFX8/NEO;
- that `sceGnmDispatchDirect` works through the current PRX on retail firmware 14.02;
- that a GPU command buffer, compute queue, shader binding, fence, or readback path has been qualified;
- that neural inference, temporal reconstruction, or a working Neural Rendering app exists;
- that the current diagnostic PKG is a compute test or an installable update.

## Console state preservation

Gate A makes no PS4 network request and changes no console state. The existing `.github/workflows/ps4-installed-app-readonly-audit.yml` is also changed to run after a successful `PS4 Control Plane Preflight`; it collects installed title PKG, SFO, EBOOT, and PRX identities over FTP and uploads evidence without writing or deleting console files. This is a read-only state audit, not a GPU compute test. The PRX identity conflict recorded in `AGENTS.md` remains a hard block on PRX replacement or activation until independently resolved.
