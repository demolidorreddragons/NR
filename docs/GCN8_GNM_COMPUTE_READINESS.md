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

Automated build attempts `45200242052`, `45200602970`, `45201076372`, and `45201396827` failed before shader compilation. Their logs exposed missing generated Mesa/GLSL files in the candidate's incomplete Makefile recipe (`u_format_gen.h`, `builtin_types.h/.c`, `shader_stats.h`) an unpinned sibling `Vulkan-Headers` dependency (`vk_video/vulkan_video_codec_h264std.h`), missing Vulkan utility header (`vk_struct_type_cast.h`), missing AMD PM4 packet/format generated files (`amd_cp_packets_gfx11.h`, gfx12, packet printers, and `gfx10_format_table.c`), and the host build's XSI feature level hid `pthread_barrier_t` at `_XOPEN_SOURCE=500`. Earlier failed attempts exposed missing generated Mesa/GLSL sources, an unpinned Vulkan-Headers dependency, XSI feature-level issues, omitted ACO opcode generation, and unresolved BLAKE3 SIMD symbols. Those workflow defects were corrected in successive commits and the final corrected run is recorded below under Gate A PASS. The historical failures are retained as audit trail; they no longer represent the current Gate A state.

**Scope limitation:** Gate A proves only that the pinned compiler emits a structurally valid GFX8/NEO compute shader container. It does not prove the binary executes, that firmware 14.02 accepts it, or that a GPU dispatch/readback works. No shader binary is staged to the console by this workflow.

## Required engineering sequence

### Gate A — reproducible compute shader artifact

- [x] Pin compiler source and dependency revisions.
- [x] Add isolated hosted build and GFX8/NEO target verification workflow.
- [x] Corrected workflow run using upstream `make generated` plus the explicitly generated omitted `aco_opcodes.h`; inspect link result, output binary, report, and digest (run `45209149062`).
- [x] Inspect the emitted shader binary, independent structural verifier, report, and SHA-256 (artifact `14835836628`).
- [ ] Independent reproduction outside the producing workflow.
- Reject output that targets GFX7/base PS4 or GFX10+/PS5.

### Gate B — command submission design

**Current state: BLOCKED / research only. No PS4 compute dispatch or readback implementation exists.**

Additional pinned-source finding: in `PS4-OpenGNM/opengnm` commit `4b295ca54c82c83acf308d1c646a2dfa9ae57350`, the generic backend contains PM4 packet construction for `sceGnmSetCsShader` and `sceGnmDispatchDirect`, but its `sceGnmSubmitCommandBuffers` implementation is explicitly a no-op and its compute-queue mapping returns `GNM_ERROR_UNSUPPORTED`. The Orbis backend declares/forwards to real `libSceGnmDriver` firmware entry points. Thus generic backend tests cannot establish GPU execution; the firmware-backed path, buffer residency, exact 14.02 ABI, and completion/readback path still need independent proof. See `src/driver_generic.c`, `src/driver_orbis.c`, and `docs/backends.md` at that pinned commit.


#### ABI evidence collected during Gate B follow-up (2026-11-23)

- The OpenOrbis Toolchain `include/orbis/GnmDriver.h` header is not a usable typed ABI contract for the compute path: `sceGnmSetCsShader` and `sceGnmDispatchDirect` appear as empty `void function(void)` declarations. Calling either through those declarations would be invalid; do not copy them into the PRX.
- The pinned opengnm header `PS4-OpenGNM/opengnm/include/gnmdriver.h` at `4b295ca54c82c83acf308d1c646a2dfa9ae57350` does provide candidate typed signatures: `sceGnmSetCsShader(uint32_t*, uint32_t, const uint32_t*)` and `sceGnmDispatchDirect(uint32_t*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)`. Its submit declarations also distinguish DCB and CCB arrays, and it exposes compute-queue mapping and compute-wait interfaces. These are source-level candidates, not proof of the correct ABI on retail firmware 14.02.
- The pinned opengnm Orbis backend forwards to firmware GNM entry points, while its generic backend emits PM4 but deliberately does not submit to hardware and reports compute-queue mapping unsupported. Therefore the host backend cannot be used as Gate C evidence.
- The opengnm repository's own documented hardware evidence is limited to graphics rendering on PS4 FW 9.00/GFX7; it does not establish GFX8 compute or FW 14.02 behavior. No dispatch ABI, allocation/residency model, fence/readback sequence, or firmware 14.02 compatibility is accepted yet.
- The PRX `main.c` currently hooks only `sceGnmSubmitAndFlipCommandBuffers`; it has no typed compute calls or hardware test path. The existing resource-table contract is host-side layout validation only and does not allocate or populate GPU descriptors.

**Decision:** do not add guessed firmware prototypes, PM4 packets, queue setup, or GPU-address dereferences to the live PRX. Gate B remains blocked until a firmware-14.02-qualified ABI and allocation/synchronization/readback contract can be sourced and independently verified. The next implementation must remain isolated from the existing telemetry PRX until that evidence exists.

Before adding code to the PRX, prove from pinned headers/source and ABI evidence:

- how this title/process obtains a valid GNM command buffer and GPU-visible allocations;
- the correct compute shader register setup and user-data/resource binding;
- the valid dispatch packet size and workgroup dimensions;
- resource cache policy, GPU barriers/fences, completion signaling, and timeout/recovery behavior;
- how GPU output becomes CPU-readable without dereferencing a GPU virtual address as a CPU pointer;
- that the method is valid for retail PS4 Pro firmware 14.02, not merely a devkit or emulator.

Do not invent a standalone queue or call undocumented firmware exports based only on symbol names.



#### Cross-check against shadPS4 HLE declarations (2026-11-23)

Inspected `shadps4-emu/shadPS4` commit `c4f8699581ef1f795a74669cc12fadf440cbe4a1`, files `src/core/libraries/gnmdriver/gnmdriver.h` and `gnmdriver.cpp`. This independently corroborates candidate C signatures for `sceGnmSetCsShader`, `sceGnmDispatchDirect`, `sceGnmSubmitCommandBuffers`, `sceGnmMapComputeQueue`, and `sceGnmDingDong`; it is emulator implementation evidence only, not proof of retail Sony firmware ABI or behavior.

Notable implementation detail: the HLE `sceGnmSetCsShader` validates a 7-dword register block and rejects a nonzero high shader-address word; HLE `sceGnmDispatchDirect` accepts a 9-dword command reservation. The pinned opengnm generic backend's `sceGnmDispatchDirect` currently checks `size < 12` and writes a larger PM4 sequence (including thread-count register setup, dispatch, and NOP padding). This cross-implementation difference must be reconciled against a real Sony SDK/firmware contract before using either size convention in the PRX. Emulator HLE can help test packet shape but cannot qualify the console path.

#### Additional end-to-end compute stack investigated (2026-11-23)

A second upstream candidate was inspected: `PS4-OpenGNM/vulkan-ps4` at commit `f4d940b723771b3fe637e8ac0704bb5d6ab19622` (repository's `main` at inspection time). It is a Vulkan ICD built on opengnm/opengnm-psbc and contains code paths for compute pipelines, descriptor sets, direct-memory allocation, command dispatch, queue submission, EOP labels, and fences. Relevant files inspected: `src/vk_ps4_pipeline.c`, `src/vk_ps4_command.c`, `src/vk_ps4_descriptor.c`, `src/vk_ps4_memory.c`, `src/vk_ps4_queue.c`, and `src/vk_ps4_sync.c`.

**This is a research lead, not an accepted backend or Gate C evidence.** The inspected revision does not provide a deterministic PS4 compute hardware smoke test or a firmware-14.02 validation record. Its README advertises compute support, but the checked-in tests list triangle/descriptor/format/validation coverage and a compute shader asset without a corresponding compute known-answer hardware test.

Specific implementation concerns found during source inspection:

- `vk_ps4_CmdDispatch` records `sceGnmDrawCmdDispatchDirect`, but the source inspection alone does not establish that the CS pipeline bind, user-data/resource descriptors, and dispatch packet sequence is valid on the target retail firmware.
- `vk_ps4_InvalidateMappedMemoryRanges` is implemented as an unconditional no-op with a comment that direct memory is GPU-visible. Visibility is not equivalent to cache coherency; this cannot currently serve as proof of GPU-to-CPU readback correctness.
- Queue fence signaling uses an EOP event write and CPU polling of a mapped label. This is a plausible synchronization design, but it has not been qualified for firmware 14.02, and timeout/recovery and GPU-write visibility still need hardware evidence.
- The memory path maps Onion/Garlic types and uses `sceGnmDirectMemoryAllocate`, but its selected memory-type constants, mapping semantics, address-space assumptions, and cache policies need validation against the actual OpenOrbis/GoldHEN runtime and PS4 Pro firmware.
- The queue path submits command-buffer pointers directly through `sceGnmSubmitCommandBuffers`; source review does not establish residency, command-buffer address validity, safe lifetime, or correct submission semantics for the PRX's process context.

**Engineering decision:** do not vendor or link this ICD into the current app yet. First pin and independently build its full dependency graph, audit the CS pipeline bind and resource-slot mapping, prove allocation/address/cache semantics, and add a minimal standalone compute known-answer sample. The sample must write a fixed sequence to a GPU buffer, signal completion with a proven fence, invalidate/transfer through a verified coherent path, and compare every element on the PS4 Pro. Only then can the reusable pieces be considered for the Neural Rendering backend.

External source references used for this investigation:
- opengnm GNM compute API reference: https://ps4-opengnm.github.io/opengnm/reference/driver/
- opengnm shader stage/register reference: https://ps4-opengnm.github.io/opengnm/reference/shader/
- opengnm direct-memory helper reference: https://ps4-opengnm.github.io/opengnm/reference/helpers/
- opengnm backend distinction (Orbis firmware forwarding vs generic no-op submit): https://ps4-opengnm.github.io/opengnm/backends/
- vulkan-ps4 candidate source revision: https://github.com/PS4-OpenGNM/vulkan-ps4/tree/f4d940b723771b3fe637e8ac0704bb5d6ab19622

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


## PS4 Pro FTP access and deployment control plane

**Operational access record (user-confirmed, 2026-11-23):** the user has re-established access to the PS4 Pro over FTP and explicitly requires this access path to be treated as part of the project setup, not repeatedly described as though the console were unknown or unavailable.

Previously supplied connection profile:

- Console: PS4 Pro, GoldHEN FTP service
- FTP endpoint: `192.168.0.17:2121`
- Login: `anonymous` with an empty password
- Upload destination for install packages: `/data/pkg/`
- Passive mode: `PASV` works; `EPSV` returns FTP error `502`
- Transfer mode: binary; prior bridge transfers were reported with matching SHA-256 hashes
- Bridge/control-plane name used in the project: `PS4-WINDOWS-BRIDGE`

**Access semantics:** FTP access is an established project capability through the user's PC/bridge path. It must not be conflated with direct reachability from an arbitrary hosted CI runner or this assistant's isolated execution environment. A previous direct connection attempt from that isolated environment was refused because its network (`172.26.36.0/22`) did not route to the console LAN (`192.168.0.0/24`). That historical failure does not invalidate the user's FTP access or the configured bridge. Deployment automation must run through the reachable bridge/control-plane path and verify connectivity at execution time; never silently fall back to a different host or claim an install succeeded without transfer, package verification, installation result, and post-install evidence.

**Safety and install protocol:** FTP read access may be used for inventory and evidence collection. Before any write/install, preserve the existing PRX, `plugins.ini`, GoldHEN state, installed applications and saves. Stage only a validated release PKG under `/data/pkg/`, verify its SHA-256 after transfer, and follow the user's standing rule: notify with the exact installation procedure, document the package and procedure here, then wait for the user's installation feedback. After feedback, collect console logs and read-only state, diagnose, correct and retest. Do not describe a PKG as installed merely because it was uploaded to FTP.


### Gate A follow-up: BLAKE3 standalone-link correction (2026-11-23)

The workflow triggered by commit `9e178d7835cf634f0f800b2d8d003e7e394b17e3` ran as GitHub Actions run `45206963771` (job `137805168205`). Dependency checkout, all generated-source steps, and compilation of the static-library objects completed. Final link failed because the standalone Makefile compiled `blake3_dispatch.c` and `blake3_portable.c` but did not include the x86 assembly translation units; the dispatcher therefore referenced missing SSE2, SSE4.1, AVX2, and AVX512 symbols. Shader compilation and independent GNM verification were skipped. This is a host compiler packaging/configuration failure, not a PS4 execution result.

Inspection of the exact pinned upstream `src/util/blake3/meson.build` confirms the supported no-assembly fallback uses all four definitions: `BLAKE3_NO_SSE2`, `BLAKE3_NO_SSE41`, `BLAKE3_NO_AVX2`, and `BLAKE3_NO_AVX512`. The standalone workflow now applies these definitions to both C and C++ flags so the dispatcher selects the portable implementation, matching that upstream fallback. Commit: `8f106df1353d3d6f253203641b2417d96b2539de`. This correction is **pending CI confirmation**; Gate A remains unpassed until a new run links, compiles the smoke shader, and passes the independent stage/target/CRC verifier.


### Gate A follow-up: BLAKE3 flags expansion defect (2026-11-23)

The workflow run triggered by commit `c6a6256edeed7ec26448921e938891442cb56cfd` (run `45207953801`, job `137808866934`) completed the dependency checkout and generated-source preparation, then stopped before compilation. The exact shell error was `BLAKE3_NO_SIMD: unbound variable`: the workflow wrote an assignment into the generated `config.mak` heredoc, but attempted to expand that name in the runner shell while `set -u` was active. This was a workflow scripting defect, not a compiler or console failure.

Commit `04c36ba7ac52b9d0d59e14b0b9d065ad2dd1f1a9` fixes the defect by placing the four upstream portable BLAKE3 defines directly in both generated `CFLAGS` and `CXXFLAGS`, avoiding an unset shell-variable expansion. The corrected workflow still requires a fresh completed run; Gate A remains unpassed until the compiler links, the compute shader compiles, and the independent GNM stage/NEO/CRC verifier succeeds.


### Gate A PASS: compiler and GNM compute artifact verified (2026-11-23)

The corrected workflow completed successfully on commit `e010c91a4b4184bbc19721fb75a5f2ec5b66c4e7`:

- GitHub Actions run `45209149062` (pull request) — all steps successful.
- Matching push run `45209140192` — successful.
- Host test run `45209149046` — successful.
- Artifact bundle: `gcn8-compute-artifact-gate-45209149062` (artifact ID `14835836628`).

The pinned compiler linked successfully; the deterministic SPIR-V compute smoke shader compiled to a 232-byte GNM shader binary; the independent verifier passed the PSSL/GNM structural checks, compute stage byte (`4`), NEO target byte (`2`), and PS4 CRC32 (`0x925b2070`). Artifact SHA-256: `32999d68fd9052a4833ba7cfa16517180544c78f173999ca8d8578f21584482d`. SPIR-V SHA-256: `964af9e2b13bdce7883cb2b53437e4b75547ddfa12580177e4d0da0d7509a626`.

**Gate A is now PASS (artifact-only).** The verifier report explicitly records `hardware_dispatch_tested: false` and `firmware_14_02_tested: false`. This result proves compiler output format/target metadata/CRC only; it does not prove shader execution on the PS4 Pro.

### Gate B status: hardware dispatch/readback integration is not implemented

A repository source inventory after Gate A found no implementation references to `sceGnmSetCsShader`, `sceGnmDispatchDirect`, or a compute readback path. Current `src/` contains telemetry and telemetry bridge sources; existing telemetry hooks capture submit/present timing and CPU-visible data, not a compute dispatch/readback pipeline. Therefore Gate B remains blocked on implementing and validating the PS4 GNM command submission, compute shader binding, resource allocation, synchronization/fence handling, and GPU-to-CPU readback path against the actual target SDK/firmware ABI.

No PKG has been produced or installed as part of this Gate A pass. No console state was changed. The next work must establish the exact target SDK headers/library ABI and add a separately gated, opt-in PS4 hardware smoke harness before any production integration.


### Gate B follow-up: current opengnm runtime documentation cross-check (2026-11-23)

The current published opengnm documentation was rechecked after the earlier pinned-source review. It clarifies an important distinction: the Orbis backend lists `sceGnmSetCsShader`, `sceGnmDispatchDirect`, `sceGnmSubmitCommandBuffers`, and compute-queue functions among firmware externs, while separately identifying 172 other GNM entry points as retail stubs returning `ORBIS_GNM_ERROR_FAILURE`. The published API reference gives typed signatures for the CS bind and direct-dispatch calls and does **not** label those two calls as retail stubs. This is stronger source-level evidence that these compute entry points are intended to be callable on retail hardware than the placeholder OpenOrbis declarations alone. It still does not prove the exact export/ABI on firmware 14.02 or a successful GFX8 dispatch on this console.

The same documentation says the generic backend's submit functions are no-ops and that its host tests validate packet construction only. Its Orbis backend forwards to `libSceGnmDriver`; this remains a distinct hardware path. Do not conflate the 172 unrelated retail stubs with the CS bind/dispatch functions, and do not infer that every GNM API is usable on retail.

References checked:
- Runtime API signatures and compute/submit sections: https://ps4-opengnm.github.io/opengnm/reference/driver/
- Orbis vs generic backend, firmware extern list, and explicit retail-stub count: https://ps4-opengnm.github.io/opengnm/backends/

**Revised Gate B assessment:** compute bind/dispatch/submit have credible typed source-level candidate contracts and are not identified as retail stubs by the current opengnm docs. Gate B nevertheless remains BLOCKED because there is still no independent firmware-14.02 ABI confirmation, no proven command-buffer and GPU allocation/residency path for this PRX process, no qualified synchronization/cache-coherent readback, and no console known-answer run. The immediate next engineering step is to build a separate non-invasive PS4 test harness using only the documented Orbis path, but only after resolving those runtime contracts. No changes to the live telemetry PRX are authorized by this source review.

### CI confirmation for current audit head

For branch head `a9c270afe171e14ac21ff12dff9d5a12e8a2a87e`, the latest Gate A pull-request run `45214108704` and push run `45214094984` both completed successfully; the matching host test run `45214108677` also completed successfully. These runs validate the hosted compiler/artifact gate and host tests only. They do not change the hardware status above.


### Gate B follow-up: Vulkan PS4 candidate synchronization defects (2026-11-23)

A focused line-level review of PS4-OpenGNM/vulkan-ps4 at pinned commit `f4d940b723771b3fe637e8ac0704bb5d6ab19622` found additional correctness defects that prevent using this implementation as the Gate B/C harness without upstream fixes and hardware qualification:

- `src/vk_ps4_memory.c`: `vk_ps4_InvalidateMappedMemoryRanges` discards all arguments and returns success without performing cache invalidation or a transfer. The implementation's comment equates GPU visibility with CPU cache coherency; those are distinct properties. It cannot establish correct GPU-to-CPU readback.
- `src/vk_ps4_queue.c`: after `sceGnmSubmitCommandBuffers` reports failure, the code marks the fence signaled and advances its signal value before returning `VK_ERROR_DEVICE_LOST`. A caller that inspects the fence can therefore observe completion even though the submitted work failed. The semaphore failure path similarly falls back to CPU-signaling the label.
- `src/vk_ps4_queue.c`: `vk_ps4_QueueWaitIdle` polls an EOP label in an unbounded `while (*idle_label != 1)` loop when submission returns success. There is no timeout, cancellation, or recovery path in this loop; a missing/invalid EOP write can hang the process indefinitely.
- `src/vk_ps4_sync.c`: fence waiting polls a GPU-written label, but the candidate does not provide a qualified retail-FW cache visibility contract for CPU reads of that label. A volatile load alone is not a cache invalidation primitive.
- `src/vk_ps4_command.c` and `src/vk_ps4_pipeline.c`: the compute path binds `cs_regs` and calls `sceGnmDrawCmdDispatchDirect`, but the inspected path still depends on the unqualified shader address, descriptor/user-data, command-buffer, and residency assumptions already listed above.

These are source-level findings in the pinned candidate, not observations from the user's console. They are not repaired by host tests or the generic backend. Do not vendor this revision into NR, use it as the known-answer harness, or call its queue/sync code from the live PRX. Gate B remains blocked pending corrected fail-closed submit/fence semantics, bounded waits with recovery, and a proven cache-coherent readback path, followed by exact PS4 Pro FW 14.02 hardware validation.

Source files and reviewed regions:
- `src/vk_ps4_memory.c`: `vk_ps4_InvalidateMappedMemoryRanges`
- `src/vk_ps4_queue.c`: failed-submit fence fallback and `vk_ps4_QueueWaitIdle`
- `src/vk_ps4_sync.c`: fence label polling
- `src/vk_ps4_command.c`: compute pipeline bind and `vk_ps4_CmdDispatch`
- `src/vk_ps4_pipeline.c`: CS binary metadata/address setup

### Gate C follow-up: pinned opengnm hardware smoke is graphics-only (2026-11-23)

A direct inspection of PS4-OpenGNM/opengnm/tests/hardware_smoke.c at commit
4b295ca54c82c83acf308d1c646a2dfa9ae57350 confirms that this sample is not a
compute known-answer harness and cannot be treated as one:

- It allocates/maps Garlic direct memory and initializes a DCB, but the submitted
  command stream contains default hardware state, a zero-count index draw, and
  an EOP event write. It does not call sceGnmSetCsShader or
  sceGnmDispatchDirect.
- The EOP label proves only that the submitted graphics command stream reached
  its event-write point on the hardware/runtime where that sample is validated.
  It does not prove a compute shader executed or that arbitrary GPU-written
  buffer data is coherent for CPU readback.
- The sample polls a volatile EOP label for a fixed spin count and then enters
  an infinite status-display loop. That lifecycle is unsuitable for an opt-in
  PRX diagnostic that must return control, unload cleanly, and recover after a
  timeout.
- Its Garlic mapping uses CPU read/write and GPU read/write protection flags,
  but the sample does not demonstrate an explicit cache invalidation or
  transfer contract for reading a compute output buffer.

**Engineering decision:** do not copy this sample into NR or promote its EOP
event into a compute-completion/readback proof. It is useful only as a
source-level reference for a basic graphics DCB submission and event-write
sequence. Gate B/C still require a separately qualified compute pipeline,
buffer residency/address contract, bounded completion handling, and
cache-coherent result readback on PS4 Pro firmware 14.02.

### Host known-answer oracle regression coverage

The host-side known-answer verifier is deliberately limited to validating the
CPU reference contract. Its regression test constructs the expected 256-element
odd-number sequence independently of the verifier implementation, checks
boundary values, rejects null/wrong-sized buffers, and proves that one
corrupted element fails the complete comparison. This is not GPU dispatch,
hardware completion, or readback evidence. The test source was strengthened
after review because generating the test output with the same production
oracle would have made the comparison tautological.
### Gate B follow-up: descriptor pointer ABI aligned in offline artifact (2026-11-23)

The original pinned PSBC source allocated the descriptor-table address as one
ACO SGPR, while GNM classifies `PTR_INDIRECTRESOURCETABLE` as a two-SGPR
pointer. A first attempt to widen the ACO argument itself to two components
failed ACO validation with a `p_parallelcopy` operand/definition size
mismatch; that approach was discarded.

The reviewed compiler patch
`patches/opengnm-psbc/0001-gcn8-descriptor-pointer-reserve-two-sgprs.patch`
uses a narrower ABI correction for the direct descriptor-set and descriptor-
heap paths:

- Keep the ACO address payload scalar (one SGPR), preserving the compiler's
  `address32_hi=0` lowering.
- Advertise two SGPRs in `ud_info->num_sgprs` and add two to
  `num_user_sgprs`.
- Increment `ac.num_sgprs_used` once after the scalar pointer argument. This
  reserves the high-address dword in the SGPR layout, so following system
  arguments cannot occupy the second half of the GNM pointer.
- The indirect-all-descriptor-sets fallback is not covered by this patch. The
  current smoke shader uses the direct set-0 path only.

The patch is applied only to the exact pinned
opengnm-psbc revision during the isolated hosted compiler build; it is not
vendored into or linked by the live PRX.

**Offline artifact evidence:** Gate run `45226345561` succeeded on commit
`ddf094006edc64631edfa1f15f10d5480eaf82ab`; artifact
`14843296604` has ZIP SHA-256
`fcb722e38b40b0c947662e65426133f2e6b36a5c369deb9c5539672c612afa6c`.
The independent report records:

- GNM compute stage byte `0x04`, NEO target byte `0x02`, 228-byte shader
  binary, SHA-256
  `8f52fb74771164498a0c1e90bd828d8517f6414d34a1d28c395c9aae2563b6e1`.
- One input-usage slot: type `0x1c` (`PTR_INDIRECTRESOURCETABLE`), API slot
  0, start register 2.
- Compiler pointer usage width 2 SGPRs, made of one scalar payload SGPR plus
  one reserved high SGPR; the pinned GNM helper also reports width 2.
- `COMPUTE_PGM_RSRC2=0x00000088`, user-data SGPR count 4. The GFX8 ring-offset
  pointer occupies user SGPRs 0-1 and the descriptor-table pointer occupies
  2-3. Workgroup-ID arguments are separate ACO system arguments, not part of
  the `USER_SGPR` field.
- Compiled compute thread dimensions `[64, 1, 1]`.
- Status `PASS_ARTIFACT_ONLY_NOT_HARDWARE`; both
  `hardware_dispatch_tested` and `firmware_14_02_tested` remain false.

The SPIR-V input SHA-256 remains
`964af9e2b13bdce7883cb2b53437e4b75547ddfa12580177e4d0da0d7509a626`.
Host-tests run `45226359325` also succeeded on the same commit.

A host-only C++ metadata parser now validates the same container fields
and the `OrbShdr` trailer: 80-byte code length, 28-byte binary-info record,
chunk-usage offset 21 dwords, input-slot count, nonzero shader hash, and
stored CRC field. Its regression test parses both synthetic malformed
containers and the exact 228-byte artifact fixture
`tests/fixtures/gcn8-compute-smoke.sb`. The parser is not linked into the
live PRX and does not submit GPU work.

This resolves the compiler/GNM pointer-width disagreement for the direct
set-0 smoke-shader path at source and binary-metadata level. It does not prove
that retail firmware writes the pointer as expected, and it does not validate
the indirect-all-descriptor-sets path.

### Gate B follow-up: compute dispatch ABI remains unresolved (2026-11-23)

The compiled shader records local thread dimensions `[64, 1, 1]`; the
256-element known-answer workload therefore requires a dispatch grid of
`[4, 1, 1]`. The pinned sources contain three distinct paths:

- The generic backend's `sceGnmDispatchDirect` writes
  `COMPUTE_NUM_THREAD_X/Y/Z` and then a `DISPATCH_DIRECT` packet from the
  same three arguments. Its size check and emitted sequence are a generic
  backend implementation, not proof of the retail function's FW 14.02
  behavior. Do not use this path for the smoke shader.
- The local PM4 builder `sceGnmDrawCmdDispatchDirect` in
  `src/drawcommandbuffer.c` is different: it emits a 9-dword
  `DISPATCH_DIRECT` packet and does not overwrite the compiled
  `COMPUTE_NUM_THREAD` registers. This agrees with the pinned shadPS4 HLE
  model, which emits the dispatch packet only. It is the current packet-level
  candidate for a `[4, 1, 1]` grid, but has not been submitted on hardware.
- The local `sceGnmDrawCmdSetCsShader` builder writes the compute-stage
  registers, including the compiled local dimensions; the local
  `sceGnmDrawCmdSetPointerUserData` builder writes the two-dword descriptor
  pointer at the parsed start register. The OpenGNM `hardware_smoke.c`
  demonstrates DCB allocation/submission and EOP signaling for a graphics
  smoke test only; it is not compute-dispatch proof.

The generic `sceGnmDispatchDirect` implementation requires a 12-dword
capacity and emits thread-register writes plus dispatch dimensions. The
shadPS4 HLE model at commit
`c4f8699581ef1f795a74669cc12fadf440cbe4a1` models the firmware function as
a 9-dword dispatch packet. This emulator/source disagreement still prevents
using the firmware function without exact FW 14.02 evidence. The local
`sceGnmDrawCmdDispatchDirect` helper is not that firmware function.

**Engineering decision:** retain the local 9-dword PM4 helper as a reviewed
candidate, but do not submit it yet. Compute default-state initialization,
shader-code GPU placement/address patching, descriptor/output allocations and
residency, the `sceGnmSubmitCommandBuffers`/`sceGnmSubmitDone` lifecycle,
completion signaling, and CPU-visible cache-coherent readback still require
qualification on FW 14.02. No compute command has been submitted to the
console. Gate B remains blocked; Gate C hardware known-answer execution and
Gate D neural reconstruction remain incomplete.

Reviewed source locations:
- opengnm-psbc commit `28697f007862427ebec54388ec972ce6e3545912`:
  `src/amd/vulkan/radv_shader_args.c`,
  `src/amd/vulkan/radv_shader.c`, and
  `src/amd/vulkan/nir/radv_nir_lower_descriptors.c`
- opengnm commit `4b295ca54c82c83acf308d1c646a2dfa9ae57350`:
  `include/gnm_shaderbinary.h`, `include/gnm_shader.h`,
  `src/driver_generic.c`, and `src/drawcommandbuffer.c`
- shadPS4 commit `c4f8699581ef1f795a74669cc12fadf440cbe4a1`:
  `src/core/libraries/gnmdriver/gnmdriver.cpp`
