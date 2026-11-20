# GCN8 / Neural Rendering execution ledger — tasks 21–50

Updated: 2026-11-20
Branch: `feature/gcn8-numeric-kernels`

This ledger separates source/CI work from evidence that requires a PS4 Pro,
the missing model packages, or a real renderer integration. A source file or
successful host build is not evidence of GPU execution.

## Phase 5 — GFX8 toolchain and shaders

| # | Work item | State | Evidence / gate |
|---:|---|---|---|
| 21 | Validate GNM import link smoke | **Passed (compile/link only)** | PS4 workflow run 44673126332 passed API/ABI compile and GNM import link. No firmware calls executed. |
| 22 | Pin compiler/dependencies | **Pinned** | `opengnm-psbc` 28697f0; OpenGNM 4b295ca; SPIRV-Headers 09913f0; Vulkan-Headers v1.4.328. |
| 23 | Reproducible compiler workflow | **In CI validation** | `.github/workflows/gcn8-shader-build.yml`; host compiler build has not yet been accepted. |
| 24 | Compile FP32 GFX8 GEMM | **Pending compiler CI** | Pipeline now targets tiled and scalar FP32. No accepted `.sb` artifact yet. |
| 25 | Inspect shader metadata | **Pending compiler output** | Container verifier checks GNM type, OrbShdr presence, hash and CRC. Register/resource/user-SGPR interpretation still needs separate inspection. |
| 26 | Validate RW resource descriptors | **Blocked on metadata** | Do not assume GLSL `std430` maps to V#; descriptor class and slots must be read from actual compiled shader. |

## Phase 6 — GNM compute backend

| # | Work item | State | Evidence / gate |
|---:|---|---|---|
| 27 | GPU memory allocation | **API candidate identified** | OpenOrbis direct-memory allocation/map/release declarations and framebuffer sample reviewed. Compute memory type/protection and GPU-address semantics remain unverified. |
| 28 | CPU/GPU mapping | **Blocked** | Direct-memory mapping is declared, but mapping to a valid GPU virtual address and cache ownership rules are not established. |
| 29 | Buffer descriptors | **Blocked** | V# descriptor ABI is known (16 bytes); correct descriptor form for shader RW resources is not known yet. |
| 30 | Configure compute shader | **API surface identified** | CS register setup helper exists; actual registers must come from generated shader metadata. |
| 31 | Command buffer | **API surface identified** | Typed command-buffer helpers are compile/link checked; no runtime command buffer built/submitted. |
| 32 | Submit and synchronize | **API surface identified** | Submit/submit-done declarations reviewed; completion and queue lifecycle not exercised. |
| 33 | Cache coherency/error handling | **Blocked** | No verified cache maintenance path or hardware observations. |

## Phase 7 — PS4 Pro compute bring-up

| # | Work item | State | Evidence / gate |
|---:|---|---|---|
| 34 | Independent compute harness | **Host reference exists** | CPU tiled GEMM emulation and host tests exist; this is not a GNM harness. |
| 35 | Dispatch a minimal test shader | **Requires PS4 Pro** | No compute dispatch executed. |
| 36 | Execute FP32 GEMM | **Requires PS4 Pro** | No GPU result. |
| 37 | Execute FP16 GEMM | **Shader compile pending; GPU execution requires PS4 Pro** | FP16 kernel is explicitly rounded-float emulation, not proof of native packed FP16 throughput. |
| 38 | Measure latency/resources | **Requires PS4 Pro** | No VGPR/SGPR/scratch, GPU latency or throughput measurements. |

## Phase 8 — Model and graph

| # | Work item | State | Evidence / gate |
|---:|---|---|---|
| 39 | Recover/authenticate model inputs | **PASS — THREE ARCHIVES + RESOURCE + STAGE VERIFIED** | Inputs recovered from Library; all three ZIP hashes match, `WEIGHTS_HT` resource hash/size verified, 153 records inspected, and `s00.bin` reproduced byte-for-byte. See `docs/DLSSNR_310_8_3_RESOURCE.md`. |
| 40 | Check 310.8.0/310.8.3 semantic compatibility | **PARTIAL — WEIGHT RESOURCE BYTE-IDENTICAL; GRAPH/RUNTIME PARITY OPEN** | The 310.8.3 DLL's `WEIGHTS_HT` SHA-256 matches the published 310.8.0 reference exactly. This proves weight-payload identity only; preprocessing, graph code, conditioning and runtime ABI still require comparison. |
| 41 | Executable representation of 71 blocks | **Reference graph only** | 71-block reconstruction exists in prior REA evidence; no executable PS4 graph integrated. |
| 42 | Missing operators | **Blocked on model graph/operator inventory** | Cannot enumerate or implement missing operators without the actual graph and operator definitions. |
| 43 | Load/transfer weights | **Blocked on authenticated files and runtime** | No model files are present in the build environment; no GPU memory path. |
| 44 | Partial inference vs reference | **Blocked** | No executable graph/weights or PS4 compute backend. |

## Phase 9 — Renderer integration

| # | Work item | State | Evidence / gate |
|---:|---|---|---|
| 45 | Renderer/backend interface | **Not started** | Existing repository is a plugin/scaffold, not an integrated neural renderer. |
| 46 | Image input/output buffers | **Not started** | Requires a defined renderer hook and validated GPU resource ownership. |
| 47 | Full 71-block pipeline | **Blocked** | Requires tasks 26–44. |
| 48 | Conventional rendering fallback | **Design requirement** | Must remain default until neural path is proven; no renderer integration to switch yet. |

## Phase 10 — Package and release

| # | Work item | State | Evidence / gate |
|---:|---|---|---|
| 49 | Integrate backend into hybrid PKG | **Blocked on backend** | Hybrid installer/package design exists; no production compute PRX to package. |
| 50 | Audit and experimental release | **Not approved** | PKG 006 remains not approved for installation; QA, ABI/runtime, rollback and hardware evidence are incomplete. |

## Current CI and source deliverables

- `tests/gcn8_gnm_api_smoke.cpp` now compile/link-checks GNM APIs plus
  `sceKernelAllocateDirectMemory`, `sceKernelMapDirectMemory` and
  `sceKernelReleaseDirectMemory`. These are references only; no call executes.
- `tools/verify_model_inputs.py` and `tests/test_verify_model_inputs.py`
  provide hash-based model intake validation and unit tests.
- `gcn8-shader-build.yml` pins dependencies, builds the compiler, and now
  attempts FP32 tiled/scalar, FP16-emulation, and E4M3 encode/decode compute
  shader binaries for the PS4 Pro NEO/GFX8 target.
- CI host tests passing does not establish GNM dispatch or PS4 Pro performance.

## External prerequisites for completing the remaining tasks

1. A successful compiler workflow and inspection of its five generated shader
   artifacts.
2. A PS4 Pro test session with the agreed reversible procedure and compatible
   firmware/GoldHEN. No package install, plugin activation, or persistent
   console modification is implied by this ledger.
3. The exact source/model archives and extracted resources listed in
   `tools/verify_model_inputs.py`.
4. A verified compute-buffer memory type, address translation, cache protocol,
   shader descriptor layout, and supported command submission lifecycle.
