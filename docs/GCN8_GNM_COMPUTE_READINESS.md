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


### Gate A artifact re-verification — 2026-11-24

The latest hosted run, GitHub Actions run `45299287689` on branch
`engineering/codex-autonomous-cycle-20261123`, was independently inspected
from its downloaded artifact ZIP (`gcn8-compute-artifact-gate-45299287689`,
artifact ID `14878131997`). The ZIP integrity check passed. The binary and
SPIR-V hashes were recomputed from the extracted bytes rather than copied from
the report:

- `gcn8-compute.sb`: 228 bytes,
  SHA-256 `8f52fb74771164498a0c1e90bd828d8517f6414d34a1d28c395c9aae2563b6e1`.
- `gcn8-compute.spv`: 1,020 bytes,
  SHA-256 `964af9e2b13bdce7883cb2b53437e4b75547ddfa12580177e4d0da0d7509a626`.
- Container fields independently decoded from the binary: GNM magic
  `0x72646853`, compute stage `0x04`, NEO target `0x02`, 40-byte stage
  header, one input-usage slot `[0x1c, 0, 2, 0]`, `COMPUTE_PGM_RSRC2=0x88`,
  four user SGPRs, and thread dimensions `[64, 1, 1]`.
- The independently decoded values and recomputed SHA-256 match the JSON
  artifact report. The compiler build log records the pinned revisions:
  opengnm-psbc `28697f007862427ebec54388ec972ce6e3545912`, opengnm
  `4b295ca54c82c83acf308d1c646a2dfa9ae57350`, SPIRV-Headers
  `fb1379425b16e58dcbdda7be0f8604a7f5cdce78`, and Vulkan-Headers
  `d277a0a3cda9c682708c93279f0c72cd6b0ae3f3`.

This is an independent artifact-byte and metadata re-verification, not a
second compiler build. The artifact explicitly reports
`PASS_ARTIFACT_ONLY_NOT_HARDWARE`; firmware 14.02 and hardware dispatch are
both untested. Gate A remains artifact-only PASS. Gates B/C remain blocked
until a qualified FW 14.02 command submission, allocation/residency,
completion/recovery, and cache-coherent readback path is validated on the
PS4 Pro. No console command, PRX, package, or device state was touched.

### Gate A follow-up: compiler capability warning and descriptor-patch scope review (2026-11-24)

The latest successful compiler job log for run `45299287689` was re-read
at the exact warning site. The shader compile emits:

```
Unsupported SPIR-V capability: SpvCapabilityShader (1)
```

Pinned-source inspection of
`PS4-OpenGNM/opengnm-psbc` revision
`28697f007862427ebec54388ec972ce6e3545912` establishes the following:

- `SpvCapabilityShader` is present in the pinned compiler's
  `implemented_capabilities` table (`src/compiler/spirv/spirv_to_nir.c`).
  Therefore it is not rejected by the earlier
  `Unimplemented SPIR-V capability` failure check.
- The warning is emitted by the subsequent comparison against
  `b->supported_capabilities`. In the RADV path, that mask is supplied by
  `compiler_info->spirv_caps`, rather than automatically copied from the
  compiler's implemented-capability table.
- Consequently, the warning means the supplied device/compiler capability
  mask does not advertise the core Shader capability, even though the parser
  recognizes it. The successful compiler exit and produced artifact show
  that this warning was non-fatal for this invocation; they do **not** prove
  that all shader requirements are supported or that the binary is valid for
  retail firmware. No warning suppression or source change was made.

This narrows the warning's meaning but leaves a reproducibility/quality
follow-up: inspect the exact initialization of `compiler_info->spirv_caps`
and run an independent SPIR-V validation pass on the generated module under
the intended Vulkan/SPIR-V environment. Until then, retain the warning in
the audit trail and do not label the SPIR-V capability set fully validated.

The descriptor-pointer patch was also reviewed directly against the pinned
`radv_shader_args.c` implementation. It keeps the ACO argument payload at
one SGPR, increments `ac.num_sgprs_used` to reserve the high dword, and
advances the GNM user-data metadata by two SGPRs. This is internally
consistent with the patch's intended low-address scalar lowering and the
pinned GNM input-usage width helper for the direct descriptor-set path.
The patch applies the same accounting to `add_descriptor_heap`, but the
current smoke shader and independent artifact verifier exercise only the
direct set-0 descriptor-table slot. The descriptor-heap and indirect
all-descriptor-sets paths remain untested and are not covered by the Gate A
PASS.

**Result:** no change to gate status. Gate A remains artifact-only PASS;
Gate B remains BLOCKED. The successful compiler run is evidence that this
particular shader compiled despite a non-fatal capability warning, not a
general SPIR-V capability or firmware-compatibility certification.

### Gate A root-cause finding: SPIR-V capability mask is zero-initialized (2026-11-24)

A deeper trace from the CLI entry point through `libpsbc/psbc_compile.c`,
`radv_shader_spirv_to_nir`, and the pinned Mesa SPIR-V translator identifies
the concrete source of the `SpvCapabilityShader (1)` warning.

In `psbc_compile_shader()`, the compiler context is created as:

```c
struct radv_compiler_info compiler_info = {0};
```

The standalone path then calls `radv_get_nir_options(&compiler_info)`, but
the reviewed function does not initialize `compiler_info.spirv_caps`.
Later, `radv_shader_spirv_to_nir()` passes
`&compiler_info->spirv_caps` as the SPIR-V translator's explicit
`capabilities` mask. The translator copies that mask into
`b->supported_capabilities`. Since the structure was zero-initialized and
no capability bits were set in this standalone path, the translator warns
that the module's declared `Shader` capability is unsupported, even though
`Shader` is present in its separate `implemented_capabilities` table.

This is not merely an unspecified compiler warning: the standalone compiler
is supplying an all-false capability mask to a translator that interprets it
as the supported device capability set. For the current smoke module the
warning is non-fatal and translation proceeds, but other capability-gated
SPIR-V features may be treated as unavailable or trigger different lowering
paths. The prior artifact PASS therefore establishes successful compilation
under this mask, not correctness of capability-dependent shader semantics.

The regular Vulkan runtime obtains this mask from
`vk_physical_device_get_spirv_capabilities()`, generated from Vulkan
feature/extension/property support. The standalone compiler has no
`VkPhysicalDevice` and cannot safely copy that runtime result blindly.
It needs an explicit, reviewed GFX8 capability policy: set only capabilities
the PS4 Pro target and this compiler path actually implement/support, and
reject or clearly diagnose unsupported module requirements. A blanket
copy of every entry in `implemented_capabilities` would be incorrect because
that table describes translator implementation, not target hardware support.

**Required follow-up before promoting Gate A beyond artifact-only:**
1. Add a target-specific GFX8 SPIR-V capability mask to the standalone
   compiler, with source comments/evidence for each enabled capability.
2. Add a regression test proving that the basic compute module's required
   `Shader` capability is accepted without a false unsupported warning.
3. Add negative tests for capabilities deliberately not supported by the
   GFX8 target; ensure unsupported requirements fail rather than silently
   compiling under an all-false mask.
4. Rebuild and re-run the independent artifact verifier, then independently
   validate the SPIR-V module. This still does not replace firmware or
   hardware validation.

No compiler source, generated artifact, or gate status was changed in this
follow-up. The finding is recorded for a separately reviewable implementation
change. Gate A remains artifact-only PASS; Gate B remains BLOCKED.

### Gate A capability-mask correction and rerun — 2026-11-24

The standalone compiler's zero-initialized SPIR-V capability mask was corrected
with a narrowly scoped patch to the pinned `opengnm-psbc` source. The patch
sets only the core `Shader` capability for this Vulkan 1.0 compute smoke
module; it does not advertise every translator-implemented capability and
does not constitute a complete GFX8 capability policy.

The corrected workflow completed successfully on commit
`482f757296996bedf2ff6ec3337ebb2d89d600db`:

- Gate A run: [45372837775](https://github.com/demolidorreddragons/NR/actions/runs/45372837775)
  — all ten job steps passed, including both compiler patches applying cleanly,
  pinned compiler build, SPIR-V-to-GNM compilation, independent binary
  verification, SHA-256 generation, and artifact upload.
- Host regression run:
  [45372849694](https://github.com/demolidorreddragons/NR/actions/runs/45372849694)
  — all host checks passed, including GCN8 metadata parser and descriptor
  pointer ABI patch checks.
- The compiler log reports a 228-byte GNM compute shader and no
  `Unsupported SPIR-V capability: SpvCapabilityShader` warning.
- Independent report: GNM magic `0x72646853`, compute stage `0x04`,
  NEO target `0x02`, 40-byte stage header, descriptor slot
  `[0x1c, 0, 2, 0]`, `COMPUTE_PGM_RSRC2=0x00000088`,
  four user SGPRs, and thread dimensions `[64, 1, 1]`.
- Shader binary SHA-256:
  `8f52fb74771164498a0c1e90bd828d8517f6414d34a1d28c395c9aae2563b6e1`.
  SPIR-V SHA-256:
  `964af9e2b13bdce7883cb2b53437e4b75547ddfa12580177e4d0da0d7509a626`.
- Artifact ID `14909902455`; uploaded ZIP digest
  `ea07fa8748327134dbd7a21aed7aec72b03f4dfb7a84ea1c5ba2bb81eaa08a43`.

**Gate A status: PASS — artifact-only.** The corrected run proves that this
pinned compiler build accepts the smoke module's core Shader capability and
emits a structurally verified GFX8/NEO compute container. It does not prove
that all SPIR-V capabilities required by future shaders are supported, that
the binary executes on the console, or that firmware 14.02 accepts the
dispatch/descriptor ABI. Independent compiler reproduction and a fuller
positive/negative target-capability policy remain follow-up work.

**Gate B remains BLOCKED; Gate C is NOT PASSED; Gate D is NOT STARTED.**
No PS4 dispatch, PRX activation, package installation, or console-state
change was performed by these runs.


### Gate B source-contract matrix — dispatch and user-data builders (2026-11-24)

A focused re-read of the pinned OpenGNM command-builder implementation
(`PS4-OpenGNM/opengnm`, commit
`4b295ca54c82c83acf308d1c646a2dfa9ae57350`) was completed after the
latest Gate A rerun. This narrows several source-level contracts but does not
change Gate B status.

| Contract | Pinned-source observation | Evidence grade | Remaining qualification |
|---|---|---|---|
| CS shader register builder | `sceGnmDrawCmdSetCsShaderWithModifier` rejects a nonzero high shader-address word; emits COMPUTE_PGM_LO/HI, RSRC1/2 and NUM_THREAD_X/Y/Z writes, followed by a trailing NOP | SOURCE-OBSERVED | Address placement, register contents and retail FW 14.02 acceptance |
| Descriptor pointer user data | `sceGnmDrawCmdSetPointerUserData` resolves the stage/user-data slot and writes `sizeof(void*) / 4` dwords through the persistent-register builder | SOURCE-OBSERVED | Retail pointer width, GPU virtual address semantics, descriptor allocation/residency and exact slot mapping |
| Local direct dispatch builder | `sceGnmDrawCmdDispatchDirect` reserves exactly 9 dwords; emits a compute-engine DISPATCH_DIRECT packet with X/Y/Z and ordered-append flags, then a 3-data-dword NOP | SOURCE-OBSERVED | This is a local command-builder helper, not proof of the firmware export's required reservation size or execution |
| Firmware-facing dispatch candidate | Pinned `gnmdriver.h` declares `sceGnmDispatchDirect(cmdbuf, size, threads_x, threads_y, threads_z, flags)`; the generic implementation has a distinct 12-dword path while shadPS4 HLE accepts size 9 | CROSS-REFERENCE / CONFLICT | Exact Sony SDK/FW 14.02 contract is still unknown; neither implementation wins by inference |
| Generic submit path | `sceGnmSubmitCommandBuffers` and `sceGnmSubmitDone` return success without submitting; generic compute-queue mapping returns unsupported | SOURCE-OBSERVED | Generic success is not hardware evidence; must validate Orbis firmware path |
| Orbis submit path | Optional wrappers resolve and forward submit calls to `libSceGnmDriver.so`; wrappers perform argument checks/logging before forwarding | SOURCE-OBSERVED | Exact export ABI, caller context, GPU-address requirements, submit completion semantics and FW 14.02 behavior |
| Default compute state | The pinned shadPS4 HLE models a default compute-state initializer, but this remains emulator behavior | CROSS-REFERENCE | Need Sony contract or retail hardware evidence for initialization sequence and NEO-specific state |

#### Consequence for the first hardware harness

The only currently defensible packet-level candidate remains the local 9-dword
`sceGnmDrawCmdDispatchDirect` builder paired with the compiled shader's
`[64,1,1]` local size and a `[4,1,1]` workgroup grid for 256 outputs.
This is not permission to call the firmware `sceGnmDispatchDirect` export:
the latter has an unresolved size/behavior conflict across implementations.
The local builder also does not solve default-state setup, valid shader-code
GPU placement, descriptor/output allocation and residency, submission
lifecycle, completion, timeout/recovery, or cache-coherent CPU readback.

No live PRX code was changed. No firmware export was called and no command
buffer was submitted to the console. Do not use the generic backend's
successful no-op submit result as a pass condition.

#### Evidence required to unblock each remaining Gate B item

- **Firmware ABI:** a typed SDK/header or symbol/ABI record tied to retail
  firmware 14.02, including calling convention, argument widths, return
  semantics and command-buffer reservation rules.
- **GPU memory:** allocation and mapping evidence for command buffer, shader
  code, descriptor table, output buffer and completion label, including GPU
  address translation, alignment, residency and lifetime.
- **Submission:** proof of the exact submit/submit-done sequence in the target
  process context, including error handling and whether submission completion
  means execution completion.
- **Synchronization:** a GPU completion primitive with bounded wait, timeout,
  cancellation/recovery and no unbounded polling.
- **Readback:** a documented cache invalidate/flush or staging-transfer path
  that makes GPU writes CPU-visible, followed by full-buffer comparison and
  output hash on the PS4 Pro.
- **Safety:** a reversible opt-in test isolated from the telemetry PRX, with
  no writes to saves, installed title files, or unrelated GoldHEN state.

**Gate B decision remains BLOCKED.** Source inspection has improved the
candidate map but has not supplied the missing retail firmware or hardware
evidence. Gate C remains NOT PASSED and Gate D remains NOT STARTED.


### Gate B follow-up: direct-memory consumers, dispatch sizing, and Vulkan candidate rejection (2026-11-24)

A second source pass was completed after Gate A run `45383567856` finished successfully. Its downloaded artifact was inspected locally: the ZIP contains the 228-byte GNM shader, 1,020-byte SPIR-V module, compiler/build logs, verifier log, report JSON, patch files and SHA-256 manifest. The report states `PASS_ARTIFACT_ONLY_NOT_HARDWARE`, `hardware_dispatch_tested=false`, and `firmware_14_02_tested=false`. The binary and SPIR-V hashes match the previous successful runs. This confirms the hosted build's retained outputs; it does not add hardware evidence.

#### Direct-memory allocation: what the pinned OpenGNM helper actually proves

In pinned OpenGNM `src/helpers.c` at revision `4b295ca54c82c83acf308d1c646a2dfa9ae57350`:

- `sceGnmDirectMemoryAllocate` validates size/alignment, rounds size up, calls `sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), ...)`, then maps the returned direct-memory offset with `sceKernelMapDirectMemory`.
- The helper stores the mapped CPU pointer in `GnmDirectMemory.mapped` and the direct-memory allocation offset in `GnmDirectMemory.directmemory`; release unmaps and releases the direct-memory range.
- The only in-repository consumer of this helper in that OpenGNM revision is the VideoOut buffer path in `src/helpers.c`. The hardware smoke sample separately allocates Garlic direct memory for its display buffers and graphics DCB/EOP label. Neither use is a compute-resource allocation/residency demonstration.
- The helper itself does not create a distinct GPU virtual address, establish a GPU VA mapping contract, register residency, or define cache behavior for shader code, descriptor tables, compute outputs, or completion labels. A CPU mapped pointer and a direct-memory offset must not be silently treated as a proven GPU address for every resource class.
- The documented Garlic memory type value `3` and CPU/GPU read-write protection constants are source/documentation declarations in this project. They are not firmware-14.02 qualification.

Source: [pinned OpenGNM helpers.c](https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/src/helpers.c), [gnm_helpers.h](https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/include/gnm_helpers.h), and [hardware_smoke.c](https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/tests/hardware_smoke.c).

#### Corrected generic dispatch packet-size finding

The pinned generic implementation in `src/driver_generic.c` was re-counted from the exact function body:

- It checks `size < 12`.
- It writes a 5-dword SET_SH_REG packet for COMPUTE_NUM_THREAD_X/Y/Z.
- It then writes a 5-dword DISPATCH_DIRECT packet.
- It then writes a 3-dword NOP packet.
- Total body writes: **13 dwords**, despite the 12-dword minimum guard.
- The pinned `tests/test_api.c` calls this function with `size=12`.

This is a source-level size-guard/body-length inconsistency. If `size` is the dword capacity—as the surrounding command-builder APIs and test usage indicate—the 12-dword accepted case can write one dword beyond the declared capacity. The function's exact intended reservation contract remains unqualified; do not use it as a firmware ABI template. This correction supersedes earlier wording in this audit that described the generic path simply as a 12-dword sequence. The separate local `sceGnmDrawCmdDispatchDirect` builder remains a 9-dword packet candidate and is not the firmware-facing function.

Source: [pinned driver_generic.c](https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/src/driver_generic.c), [test_api.c](https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/tests/test_api.c).

#### Vulkan-PS4 candidate: useful architecture, not an accepted compute backend

The pinned `PS4-OpenGNM/vulkan-ps4` revision `f4d940b723771b3fe637e8ac0704bb5d6ab19622` exposes candidate implementations for direct-memory allocation, compute pipeline binding, dispatch recording, EOP labels, and queue submission. A closer audit finds reasons it cannot unblock Gate B:

- `vk_ps4_AllocateMemory` maps Vulkan Onion/Garlic memory types to project-defined direct-memory types and maps the direct memory for CPU access. The implementation does not independently prove GPU VA translation, residency, or retail firmware memory semantics.
- `vk_ps4_InvalidateMappedMemoryRanges` is explicitly a no-op. The source comment equates direct GPU visibility with cache invalidation; that is not evidence of CPU-visible GPU writes or cache coherency.
- Descriptor binding passes host-side pointers such as `b->buffers`, `b->textures`, or `b->samplers` directly to `sceGnmDrawCmdSetPointerUserData`. No independently verified GPU address translation/residency contract is established by that source.
- Queue fence/semaphore signaling uses EOP event writes to label addresses derived by casting CPU pointers to integers. This is a plausible design sketch, not a validated GPU-address contract.
- The fence wait has a timeout-capable polling path, but the queue-idle path contains an unbounded spin loop. The wait-semaphore error fallback can also continue when the semaphore is not signaled. These behaviors do not meet this project's bounded-wait and safe-recovery requirements.
- The candidate's host fallback deliberately marks synchronization objects signaled because generic submission is a no-op. Such host success cannot validate the hardware path.

Sources: [vulkan-ps4 memory](https://github.com/PS4-OpenGNM/vulkan-ps4/blob/f4d940b723771b3fe637e8ac0704bb5d6ab19622/src/vk_ps4_memory.c), [descriptor](https://github.com/PS4-OpenGNM/vulkan-ps4/blob/f4d940b723771b3fe637e8ac0704bb5d6ab19622/src/vk_ps4_descriptor.c), [command](https://github.com/PS4-OpenGNM/vulkan-ps4/blob/f4d940b723771b3fe637e8ac0704bb5d6ab19622/src/vk_ps4_command.c), [queue](https://github.com/PS4-OpenGNM/vulkan-ps4/blob/f4d940b723771b3fe637e8ac0704bb5d6ab19622/src/vk_ps4_queue.c), and [sync](https://github.com/PS4-OpenGNM/vulkan-ps4/blob/f4d940b723771b3fe637e8ac0704bb5d6ab19622/src/vk_ps4_sync.c).

#### Gate B disposition

The new source evidence narrows the implementation choices but does not supply a complete safe compute contract. In particular, the allocation helper is presently proven only for its VideoOut use; the Vulkan candidate's no-op invalidation and unbounded queue-idle wait are unacceptable as-is; and the generic dispatch function has a 12-versus-13-dword inconsistency.

**Gate B remains BLOCKED. Gate C remains NOT PASSED. Gate D remains NOT STARTED.** No compute command was submitted, no PRX was activated or modified, no PKG was installed, and no console state was changed during this audit.


#### Additional Vulkan-PS4 compute-pipeline finding: shader binary placement is not GPU-qualified

The compute-pipeline path in pinned `vulkan-ps4/src/vk_ps4_pipeline.c` parses the shader container, derives the CS code pointer as an offset inside the compiled binary allocation, and passes that pointer to `sceGnmCsRegsSetAddress`. The code keeps the allocation alive for the pipeline lifetime, but the inspected path does not place the binary through the audited direct-memory helper or prove that the allocator returns GPU-visible/resident memory. Lifetime correctness alone is not GPU addressability.

The same candidate's descriptor path passes pointers to descriptor backing arrays into `sceGnmDrawCmdSetPointerUserData`; no verified GPU-VA conversion or residency registration accompanies those pointer writes in the inspected path. This strengthens the rejection of the Vulkan candidate as a ready-made compute backend: shader code, descriptor storage, and label addresses all depend on unproven CPU-pointer-to-GPU-address assumptions.

Source: [pinned compute pipeline](https://github.com/PS4-OpenGNM/vulkan-ps4/blob/f4d940b723771b3fe637e8ac0704bb5d6ab19622/src/vk_ps4_pipeline.c) and [descriptor implementation](https://github.com/PS4-OpenGNM/vulkan-ps4/blob/f4d940b723771b3fe637e8ac0704bb5d6ab19622/src/vk_ps4_descriptor.c).

**Implementation consequence:** do not vendor this Vulkan ICD or reuse its compute pipeline, descriptor, or synchronization code in the live PRX. Treat it only as a reference for packet ordering and API decomposition until a separately verified GPU-visible allocation/address model and cache-coherent readback path exist.

### Gate B follow-up: ACE queue reality check and workflow reruns (2026-11-24)

The two GCN8 artifact workflows previously running against commit
`aec9cd1e813112f92744c0a3c687745a8eebea89` have now both completed
successfully:

- Gate A run
  [45384490914](https://github.com/demolidorreddragons/NR/actions/runs/45384490914)
  — success; artifact `14915228471`.
- Duplicate Gate A run
  [45384478416](https://github.com/demolidorreddragons/NR/actions/runs/45384478416)
  — success; artifact `14916215310`.
- Host tests
  [45384491038](https://github.com/demolidorreddragons/NR/actions/runs/45384491038)
  — success.

Both artifact ZIPs were downloaded and inspected. The retained report identifies
the result as `PASS_ARTIFACT_ONLY_NOT_HARDWARE`, with
`hardware_dispatch_tested=false` and `firmware_14_02_tested=false`.
The 228-byte GNM output retains the previously verified structural values:
magic `0x72646853`, compute stage `0x04`, NEO target `0x02`,
40-byte stage header, four user SGPRs, descriptor slot
`[0x1c, 0, 2, 0]`, `COMPUTE_PGM_RSRC2=0x00000088`, and
thread dimensions `[64,1,1]`. GNM and SPIR-V SHA-256 values remain
`8f52fb74771164498a0c1e90bd828d8517f6414d34a1d28c395c9aae2563b6e1`
and
`964af9e2b13bdce7883cb2b53437e4b75547ddfa12580177e4d0da0d7509a626`,
respectively. These are repeated artifact-only successes, not hardware
dispatch evidence.

#### ACE queue path is advertised but not wired in the inspected Vulkan candidate

At pinned `PS4-OpenGNM/vulkan-ps4` revision
`f4d940b723771b3fe637e8ac0704bb5d6ab19622`:

- `vk_ps4_GetPhysicalDeviceQueueFamilyProperties` advertises queue family 1
  as compute + transfer and describes it as an asynchronous ACE queue.
- `vk_ps4_CreateDevice` allocates queue objects and records
  `queueFamilyIndex`; `vk_ps4_GetDeviceQueue` returns those cached objects.
- The inspected queue submit implementation sends command buffers through
  `sceGnmSubmitCommandBuffers`. A repository-wide search of this pinned
  source did not find calls to `sceGnmMapComputeQueue`,
  `sceGnmDingDong`, or `sceGnmUnmapComputeQueue`.

Therefore the advertised second queue family is not evidence that an ACE
ring is mapped or rung by this candidate. Do not reuse its queue-family
advertisement as a working async-compute implementation.

Sources: [queue-family properties](https://github.com/PS4-OpenGNM/vulkan-ps4/blob/f4d940b723771b3fe637e8ac0704bb5d6ab19622/src/vk_ps4_instance.c),
[device queue creation](https://github.com/PS4-OpenGNM/vulkan-ps4/blob/f4d940b723771b3fe637e8ac0704bb5d6ab19622/src/vk_ps4_device.c),
[queue submission](https://github.com/PS4-OpenGNM/vulkan-ps4/blob/f4d940b723771b3fe637e8ac0704bb5d6ab19622/src/vk_ps4_queue.c).

#### Independent queue reverse-engineering leads are conflicting and not FW 14.02 qualification

Two additional public source investigations provide useful leads but cannot
be promoted to the target firmware contract:

- `Kravickas/PS4-apps` documents static analysis of one game's PRX and call
  sites. It reports pipe IDs up to 6, queue IDs up to 7, a 4-byte ring-address
  alignment check, and two mapped queues at pipe 0/queue 4 and pipe 1/queue 4
  with a 0x1000-dword ring. It also notes that the observed doorbell action
  was a null kick. This is title-specific reverse engineering, not a
  general Sony ABI guarantee.
- `boykopovar/AnyPS5` documents a separate implementation derived partly
  from PS4 firmware 10.71 disassembly and shadPS4. It reports a 256-byte ring
  base alignment and power-of-two ring size for its compute-queue validation,
  and explicitly labels queue IDs, refused-input results, and read-offset
  timing as not measured on console hardware.

The 4-byte versus 256-byte alignment observations conflict. They may reflect
different validation layers, call-site assumptions, or firmware revisions;
the current evidence does not resolve which constraint applies to a retail
PS4 Pro on 14.02. Do not choose either alignment, queue ID, ring size, or
doorbell behavior for a harness by guesswork.

Sources: [PS4-apps reverse-engineering notes](https://github.com/Kravickas/PS4-apps/blob/3a79e374b821d5dfc4077c75df712f942d068478/src/nid_resolve.h),
[AnyPS5 GNM driver research notes](https://github.com/boykopovar/AnyPS5/blob/e43d0cf296603edaab247cc77cbbec96b64be5e6/docs/dev/debt/libSceGnmDriver.md).

#### Updated Gate B decision

The async queue candidate is now more clearly scoped: the Vulkan-PS4
implementation advertises an ACE family without wiring the mapping/doorbell
calls in the inspected revision, while independent queue notes disagree on
ring alignment and are not specific to firmware 14.02. No firmware-qualified
queue contract has been established.

Gate A remains **PASS — artifact-only**. Gate B remains **BLOCKED**.
Gate C remains **NOT PASSED** and Gate D remains **NOT STARTED**. No compute
dispatch was submitted, no PRX was changed or activated, no PKG was installed,
and no console state was changed.


### Gate B follow-up: retail-game compute queue and readback trace (2026-11-24)

A static reverse-engineering report for **The Last Guardian v1.03 (CUSA03627)** was inspected at pinned source revision `HayatoG/tlg_pc@ee3327d7c1a1c107a16327d7c7a33c6c148b6dce`, file `analysis/02_rendering_gnm.md`. The report analyzes that title's `eboot.bin` with Ghidra and labels findings as confirmed [C], probable [P], or guesses [?]. This is materially stronger evidence of a real game's GNM usage than an emulator declaration, but it remains title-specific static analysis: it is not Sony SDK documentation, not a retail firmware 14.02 ABI record, and not a test performed on this project's PS4 Pro.

#### Queue mapping and dispatch call sites observed

The report identifies three distinct queue/context arrangements:

- A device-level `JsComputeContext` maps pipe 1 / queue 4 with priority 0 and a ring of `0x400` dwords. Its described workload includes asynchronous copy, map/readback, and resource updates.
- Three per-frame `JsComputeContext` instances map pipe 2 / queues 0, 1, and 2 with priority 1. Their ACBs are submitted before the frame's DCB during Present.
- A separate Gnmx `JsComputeQueue` maps pipe 0 / queue 3 with a 1 MiB ring for dispatch-draw work.

The report finds direct call sites for `sceGnmMapComputeQueueWithPriority`, `sceGnmMapComputeQueue`, `sceGnmDingDong`, and `sceGnmUnmapComputeQueue`. The compute-queue submit path calls `sceGnmDingDong`; unmap paths call `sceGnmUnmapComputeQueue`. The title also calls `sceGnmDispatchDirect` with a reservation size of 9 dwords at identified call sites.

These observations prove that the analyzed title contains code paths using those imports and queue configurations. They do **not** establish that the same pipe/queue IDs, ring sizes, priorities, alignment, or call arguments are valid for an unrelated process or for firmware 14.02. In particular, the 9-dword call-site observation does not resolve the 9-versus-12/13-dword source discrepancy already recorded above; the exact callee ABI and reservation semantics still need independent qualification.

#### Submission and completion ordering observed

The report describes the following title-specific flow:

1. Compute work is recorded in an ACB and submitted through the Gnmx compute queue; its submit path issues `sceGnmDingDong`.
2. During Present, the frame's compute ACB is submitted before the graphics DCB.
3. The graphics path writes an EOP event and calls `sceGnmSubmitAndFlipCommandBuffers` for the frame.
4. A dedicated `ps4render_submitDone` thread waits for the EOP event through an equeue and then calls `sceGnmSubmitDone`.

This gives a concrete candidate ordering to investigate, rather than assuming that a successful generic submit call means GPU execution has completed. It is still a game's rendering architecture, not a reusable standalone PRX contract. The report's EOP/equeue mechanism must not be copied until its label allocation, address space, event registration, memory visibility, timeout, and error semantics are independently established.

#### Resource placement and CPU readback observed

The report identifies shader creation paths that copy shader code into Garlic memory, patch its GPU address, and construct input-resource offset tables through CUE/LCUE. It also identifies asynchronous compute workflows using `AsyncCopyResource`, `AsyncMap` (GPU result readback to CPU), and `StallUntilSignaled`.

This is important evidence that a shipping title implements a complete compute-to-CPU data path. However, the report does not establish the underlying allocator's exact firmware ABI, GPU VA mapping/residency rules, cache maintenance operations, or whether the same path is accessible and safe from this project's GoldHEN PRX context. It must not be reduced to the assumption that any CPU pointer can be cast to a GPU address.

#### Gate B implications and next evidence targets

The Last Guardian trace narrows the research targets:

- Recover the exact argument and return contracts for `sceGnmMapComputeQueue`, `sceGnmMapComputeQueueWithPriority`, `sceGnmDingDong`, and `sceGnmUnmapComputeQueue`, including ring-size units, alignment, queue ownership, and failure behavior.
- Establish whether the title's queue arrangements depend on privileged title/runtime setup, owner registration, command-buffer context, or other initialization omitted by the high-level summary.
- Trace the Garlic allocation and shader-address patching path down to allocator calls and address conversion; separately trace ACB/DCB and EOP-label allocation.
- Trace `AsyncMap` and `StallUntilSignaled` down to the exact synchronization and cache-coherency operations, including bounded wait and recovery behavior.
- Reconcile the 9-dword `sceGnmDispatchDirect` call sites with the 12/13-dword OpenGNM generic implementation and the 9-dword shadPS4 HLE reservation, without selecting a convention by majority vote.
- Find firmware-14.02-qualified ABI evidence or a controlled hardware experiment before writing any firmware-facing call into NR.

Source: [The Last Guardian v1.03 GNM rendering analysis](https://github.com/HayatoG/tlg_pc/blob/ee3327d7c1a1c107a16327d7c7a33c6c148b6dce/analysis/02_rendering_gnm.md).

**Disposition:** this is accepted as a high-value reverse-engineering lead, not as a Gate B pass. Gate B remains **BLOCKED**; Gate C remains **NOT PASSED**; Gate D remains **NOT STARTED**. No queue was mapped, no compute command was submitted, no PRX was modified or activated, no PKG was installed, and no console state was changed.


### Gate B follow-up: compute-queue validation and dispatch-size cross-check (2026-11-24)

A further cross-check was made between the shadPS4 HLE source embedded in the pinned `HayatoG/tlg_pc` revision and the public PS4 error-code reference. This adds useful validation semantics, but does not qualify retail firmware 14.02.

#### Queue-map validation model

The HLE implementation inspected at `HayatoG/tlg_pc@ee3327d7c1a1c107a16327d7c7a33c6c148b6dce` exposes the candidate signatures:

- `sceGnmMapComputeQueue(pipe_id, queue_id, ring_base_addr, ring_size_dw, read_ptr_addr)`
- `sceGnmMapComputeQueueWithPriority(..., pipePriority)`
- `sceGnmDingDong(gnm_vqid, next_offs_dw)`

Its map path checks pipe and queue bounds, 256-byte ring-base alignment, power-of-two ring size (argument named in dwords), and 4-byte read-pointer alignment. It registers the queue in the emulator's ASC queue registry, returns a virtual queue ID, and initializes the read pointer. Its DingDong path accepts the virtual queue ID and next offset in dwords, handles ring wrap, and submits the selected ring segments to the emulator.

The PS4-Dev-Wiki GNM error-code table independently documents corresponding validation categories and error names: invalid pipe ID, invalid queue ID, invalid ring size, invalid ring base address (256-byte alignment), and invalid read-pointer address (4-byte alignment). This corroborates that these are distinct validation dimensions rather than a single contradictory alignment rule. It remains a community-maintained error reference, not an ABI contract or proof of the exact checks on firmware 14.02.

Important HLE limitation: the inspected `sceGnmDingDong` implementation takes a submit mutex and calls `WaitGpuIdle()` before processing the submission. This is an emulator serialization strategy; it must not be interpreted as evidence of real hardware ACE asynchrony, queue scheduling, or completion semantics.

Sources:
- [shadPS4 HLE source in pinned TLG port](https://github.com/HayatoG/tlg_pc/tree/ee3327d7c1a1c107a16327d7c7a33c6c148b6dce/port/gpu/shadps4/core/libraries/gnmdriver)
- [PS4-Dev-Wiki GNM compute-queue error codes](https://www.psdevwiki.com/ps4/Template%3ACE)

#### DispatchDirect reservation: HLE contract is exact-size 9, not generic-backend minimum 12

The shadPS4 HLE implementation inspected in the same pinned source accepts `sceGnmDispatchDirect` only when `size == 9`, writes a four-dword DISPATCH_DIRECT packet body plus its header and three-dword trailing NOP, and returns an error for other reservation sizes. This is consistent with the 9-dword call sites identified in The Last Guardian's static trace.

It does not resolve the retail firmware contract. The pinned OpenGNM generic implementation remains materially different: it checks `size < 12` and emits a 5-dword SET_SH_REG packet, a 5-dword DISPATCH_DIRECT packet, and a 3-dword NOP (13 dwords total). These are different layers/implementations and must not be blended into one assumed ABI. The generic path's size guard/body-length discrepancy remains a separate source-level concern.

**Decision:** the 9-dword HLE reservation is now a stronger candidate for the dispatch packet builder's intended reservation in the game/emulator path, but it is not accepted for firmware-facing NR code. Before any use, obtain a firmware-specific disassembly/ABI record or perform a controlled, reversible hardware test with bounded failure handling. No dispatch should be attempted based only on this cross-check.

#### Gate B disposition

This pass improves the candidate validation matrix and clarifies the ring/read-pointer alignment distinction. It does not prove ring allocation/residency, GPU virtual addressing, command-buffer ownership, queue mapping on retail firmware, cache coherency, completion, or readback. It also does not qualify `sceGnmDispatchDirect` reservation semantics for firmware 14.02.

Gate A remains **PASS — artifact-only**. Gate B remains **BLOCKED**. Gate C remains **NOT PASSED** and Gate D remains **NOT STARTED**. No console calls, queue mapping, compute dispatch, PRX modification/activation, PKG installation, or console-state changes were made in this source audit.


### Gate B follow-up: firmware-evidence provenance and FW 14.02 gap (2026-11-24)

A targeted search for firmware-14.02-specific `libSceGnmDriver` exports, NIDs, or disassembly did not locate a public driver image or an ABI record for the required compute calls. The newly located FW 14.02 material is useful for planning evidence acquisition, but must not be overstated.

#### What the public sources establish

- The OpenGNM project documentation describes its Orbis backend as forwarding to the official `libSceGnmDriver`, with 74 real firmware externs and a reverse-engineering workflow based on decrypted firmware modules. Its rewrite plan explicitly identifies its firmware dump material as FW 9.00 (including `libSceGnmDriver.sprx` and the Neo variant). This is evidence of a documented FW 9.00 research baseline, **not** FW 14.02 compatibility or ABI verification. The current public docs are project-maintained and can evolve independently of the pinned OpenGNM revision used by this repository.
- A public PS4 kernel-dumper fork advertises FW 14.02 (build `J02700521`) support and documents kernel offsets. It dumps `kernel.bin`; the public description does not provide a decrypted FW 14.02 `libSceGnmDriver.sprx`, its export table, NID mapping, or function disassembly. Kernel-dump support therefore does not close the GNM driver evidence gap.
- OpenOrbis' public `GnmDriver.h` leaves several GNM prototypes as untyped `void function()` declarations, including compute queue functions. Its submit declarations are more informative but still do not establish the exact runtime ABI or semantics for FW 14.02.
- A public fpPS4 compatibility issue records a missing `sceGnmMapComputeQueue` NID in that emulator. This confirms that the import is encountered by at least one title, but does not provide the function's implementation or hardware semantics.

#### Consequence for the NR harness

Do not derive the FW 14.02 ABI from:
1. FW 9.00 exports/disassembly;
2. OpenOrbis placeholder declarations;
3. shadPS4 HLE signatures/validation;
4. a game's static call sites; or
5. an emulator's missing-import report.

These remain separate evidence classes. A firmware-specific ABI record must identify the target firmware/build, module variant (including Neo where applicable), export/NID, calling convention, argument widths/order, return behavior, and any reservation-size contract. For memory and queue operations it must additionally establish the GPU-visible address domain, allocation/mapping requirements, ring/read-pointer lifetime, queue ownership, and completion/readback behavior.

#### Practical next evidence milestone

The next non-invasive milestone is an auditable FW 14.02 evidence package containing (if lawfully available): exact firmware build identity, decrypted `libSceGnmDriver` and `libSceGnmDriverForNeoMode` module hashes, export/NID tables, and disassembly notes for `sceGnmMapComputeQueue`, `sceGnmMapComputeQueueWithPriority`, `sceGnmDingDong`, `sceGnmUnmapComputeQueue`, `sceGnmDispatchDirect`, `sceGnmSubmitCommandBuffers`, and `sceGnmSubmitDone`. No firmware dump, console access, or runtime test was performed as part of this pass.

Sources:
- [OpenGNM backend documentation](https://ps4-opengnm.github.io/opengnm/backends/)
- [OpenGNM reverse-engineering plan and FW 9.00 dump inventory](https://github.com/PS4-OpenGNM/opengnm/blob/main/OPENGNM_REWRITE_PLAN.md)
- [FW 14.02 kernel dumper description](https://github.com/iHaiDeeZ/ps4-kernel-dumper)
- [OpenOrbis GnmDriver.h](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/include/orbis/GnmDriver.h)
- [fpPS4 missing sceGnmMapComputeQueue report](https://github.com/red-prig/fpps4-game-compatibility/issues/430)

**Gate disposition unchanged:** Gate A PASS (artifact-only); Gate B BLOCKED; Gate C NOT PASSED; Gate D NOT STARTED. This was source/documentation research only. No console interaction, compute dispatch, PRX/PKG operation, or console-state change occurred.


### Gate B follow-up: firmware 10.71 queue disassembly cross-check (2026-11-24)

A newer pinned research snapshot was inspected at
`boykopovar/AnyPS5@403fbe6fc19ba5d19cd62566728dfadca1924978`,
specifically `docs/dev/debt/libSceGnmDriver.md`,
`docs/dev/debt/libSceGnmDriverForNeoMode.md`, and the corresponding
`Export.cpp` implementation. This is a separate evidence class from the
shadPS4 HLE and The Last Guardian call-site analysis. The project documents
parts of its PS4 GNM model as derived from firmware 10.71 disassembly and
native firmware 13.42 runs with imports stubbed; its own debt notes repeatedly
state that the queue behavior is not measured on console hardware. It is not
evidence for firmware 14.02.

#### Additional queue details reported by that research

- The candidate map validation distinguishes pipe IDs above 6, queue IDs above
  7, ring bases not aligned to 256 bytes, non-power-of-two ring sizes, and
  read-pointer addresses not aligned to 4 bytes. The notes attribute the
  10.71 `sceGnmMapComputeQueueWithPriority` error range
  `0x80D17000..0x80D17004` to invalid pipe, queue, ring size, ring base, and
  read-pointer arguments respectively. This is a firmware-10.71-derived
  candidate, not a verified 14.02 contract.
- The same notes report that `sceGnmDingDong` updates the mapped read offset
  before the queued packets execute, and that unmapping does not wait for GPU
  completion. They also identify a path where the driver records a call and
  rings the queue through an ioctl, or stores the new offset in the mapped
  queue write pointer. These details materially warn against treating the
  read offset as a completion fence or treating unmap as a synchronization
  primitive.
- The research describes compute-ring command-buffer handling, including
  restrictions on nested indirect buffers and REWIND packets. Those are
  implementation-specific observations from this research project and must
  not be generalized to Sony firmware without independent confirmation.
- The PS5-oriented `libSceGnmDriverForNeoMode` notes explicitly distinguish
  their PS4 10.71 disassembly observations from PS5 exports and mark several
  signatures as unverified. Those PS5 exports are not used as PS4 ABI evidence.

**Engineering consequence:** even if the reported early read-offset update is
correct for the analyzed firmware path, it cannot be used as proof that GPU
work has completed. The NR harness must require an independently established
completion event/fence and readback visibility path; it must not infer
completion from `sceGnmDingDong` return, queue read-offset movement, or
`sceGnmUnmapComputeQueue`.

Source: [pinned AnyPS5 GNM driver research](https://github.com/boykopovar/AnyPS5/blob/403fbe6fc19ba5d19cd62566728dfadca1924978/docs/dev/debt/libSceGnmDriver.md),
[queue/Neo-mode export notes](https://github.com/boykopovar/AnyPS5/blob/403fbe6fc19ba5d19cd62566728dfadca1924978/docs/dev/debt/libSceGnmDriverForNeoMode.md),
and [pinned implementation snapshot](https://github.com/boykopovar/AnyPS5/blob/403fbe6fc19ba5d19cd62566728dfadca1924978/core/libs/prx/libSceGnmDriver/Export.cpp).

#### Updated Gate B disposition

This cross-check adds a concrete hazard to the queue contract: queue progress
indicators and unmap may not represent GPU completion. It does not resolve the
firmware-14.02 ABI, ring allocation/residency, submission ownership, cache
coherency, bounded completion, or readback requirements. No values or call
sequences from this source are promoted into the NR implementation.

Gate A remains **PASS — artifact-only**. Gate B remains **BLOCKED**. Gate C
remains **NOT PASSED** and Gate D remains **NOT STARTED**. No console queue
was mapped, no compute dispatch was submitted, and no PRX, PKG, or console
state was changed.

### Gate A rerun confirmation for the firmware-evidence audit head

The workflows triggered for head commit
`3c7635a51616ed7567c05be47d101a52c7108436` have now completed successfully:

- [GCN8 artifact gate run 45392325919](https://github.com/demolidorreddragons/NR/actions/runs/45392325919) — success.
- [Host validation run 45392316010](https://github.com/demolidorreddragons/NR/actions/runs/45392316010) — success.
- [Additional validation run 45392325906](https://github.com/demolidorreddragons/NR/actions/runs/45392325906) — success.

These are validation results for the current source/documentation head. The
GCN8 workflow remains an artifact-only compiler test and does not change the
Gate B/C/D disposition.


### Gate B follow-up: PS4 application memory-placement and EOP pattern (2026-11-24)

A separate PS4 application source was inspected at
`fourletterworkshop/moonlight-dx-ps4@69a985c08f8c4d455c693d9967c0af9974b0ea83`,
notably `src/platform/ps4/GpuUiRenderer.cpp` and
`src/platform/ps4/GfxManager.cpp`. This is application-level OpenOrbis /
OpenGNM code, distinct from emulator HLE and firmware disassembly. It provides
a concrete graphics-path example for direct-memory placement and EOP
synchronization. It is not a compute-dispatch test, Sony ABI specification,
or evidence specific to PS4 Pro firmware 14.02.

#### Memory placement pattern observed in the application

- The application allocates direct memory with
  `sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), alignedSize,
  0x200000, memoryType, &physicalAddress)`, then maps it with
  `sceKernelMapDirectMemory` using the returned physical offset and the same
  alignment. Its allocator rounds blocks to 2 MiB. This is the application's
  chosen alignment, not a proven minimum or universal GNM requirement.
- It uses `ORBIS_KERNEL_WC_GARLIC` with CPU RW + GPU access for shader code,
  command buffers, UI staging, and framebuffers. It separately allocates
  `ORBIS_KERNEL_WB_ONION` for a CPU-pollable EOP label, also mapped with CPU
  and GPU permissions. This is useful evidence that one real PS4 application
  separates GPU working memory from CPU-observed completion storage.
- Vertex/pixel shader code is copied into the mapped Garlic block and its
  address is installed with `sceGnmVsRegsSetAddress` /
  `sceGnmPsRegsSetAddress`. Command-buffer memory is carved from the same
  Garlic allocation. The application uses the mapped CPU virtual pointer as
  the address passed to GNM resource/register helpers. This is a concrete
  application convention, but the source alone does not prove the address
  translation contract for arbitrary allocations, firmware versions, or the
  compute path.
- CPU writes to Garlic staging and shader memory are followed by
  `sceGnmFlushGarlic()` before GPU consumption. This identifies a candidate
  CPU-to-GPU visibility operation for WC Garlic. It does not establish the
  reverse GPU-to-CPU invalidation protocol for compute output in Onion or
  Garlic, and must not be treated as a substitute for that missing readback
  contract.

#### Completion pattern observed

- The graphics path resets a 64-bit label in Onion, appends
  `sceGnmDrawCmdEventWriteEop` with
  `GNM_FLUSH_AND_INV_CB_DATA_TS` and a 64-bit data payload, submits the DCB
  with `sceGnmSubmitCommandBuffers`, calls `sceGnmSubmitDone`, then waits
  for the label to become the expected value.
- This is a materially stronger candidate completion pattern than treating
  `sceGnmDingDong` or a compute queue read offset as completion. It is still
  a graphics EOP example. The exact event packet availability/semantics,
  label memory placement, ordering relative to compute dispatch, and visibility
  on firmware 14.02 must be independently established before use in the NR
  compute harness.
- The sample uses a 100,000,000-iteration spin limit. That is not an acceptable
  bounded wait policy for NR. The harness still requires a time-based deadline,
  sleep/yield strategy, timeout reporting, and safe recovery/lifetime handling.
- `sceGnmSubmitDone` is called after submission in this sample. It is not
  itself the completion fence; the code separately waits for the EOP label.
- The inspected files do not implement compute dispatch, GPU-written compute
  output readback, a cache-invalidate path for that output, or a firmware-14.02
  ABI check.

#### NR engineering consequences

1. Keep Garlic and Onion as candidate separate allocation classes for a future
   controlled harness, but do not hardcode the sample's 2 MiB alignment as a
   universal requirement.
2. Treat `sceGnmFlushGarlic` as a candidate CPU-to-GPU Garlic visibility step,
   not as proof of GPU-to-CPU output visibility.
3. Treat an EOP label as a promising completion primitive to investigate, not
   as an approved compute fence until packet ordering, compute-engine coverage,
   address domain, and firmware-14.02 behavior are verified.
4. Do not copy the sample's busy-spin loop. NR requires a bounded wall-clock
   timeout and non-spinning wait/recovery design.
5. Do not reuse its graphics DCB submission sequence as a compute ACB/ACE
   submission sequence without proving queue ownership and ordering.

Sources:
- [Pinned GpuUiRenderer.cpp](https://git.fourletterworkshop.com/moonlight-dx-ps4/blob/69a985c08f8c4d455c693d9967c0af9974b0ea83/src/platform/ps4/GpuUiRenderer.cpp)
- [Pinned GfxManager.cpp](https://git.fourletterworkshop.com/moonlight-dx-ps4/blob/69a985c08f8c4d455c693d9967c0af9974b0ea83/src/platform/ps4/GfxManager.cpp)

**Updated Gate B disposition:** this source narrows the application-level
memory/visibility investigation and supplies a concrete graphics EOP pattern,
but does not close GPU VA/residency for compute resources, firmware-14.02
queue/dispatch ABI, compute completion ordering, or GPU-to-CPU readback.
Gate A remains PASS (artifact-only); Gate B remains BLOCKED; Gate C remains
NOT PASSED; Gate D remains NOT STARTED. No console interaction, compute
dispatch, PRX/PKG operation, or console-state change occurred.
