# Neural Rendering execution ledger — tasks 51–90

Updated: 2026-11-20  
Branch: `feature/gcn8-numeric-kernels`

This ledger records the next 40 work items. A task is **DONE** only when its stated evidence exists. A source edit, host compile, or CI green result is not evidence of PS4 Pro GPU execution.

## Phase 11 — Unblock the pinned PSBC host build

| # | Work item | State | Acceptance evidence |
|---:|---|---|---|
| 51 | Correct Linux `sysconf` feature detection in host build | **PASS — POSIX PROBE + HOST BUILD** | Run `44679024542` passed the sysconf probe and linked the pinned Linux PSBC compiler. |
| 52 | Audit host OS feature macros against pinned Mesa configuration | **PARTIAL — SYSCONF PROBE PASS** | Pinned source and Orbis config use `HAVE_SYSCONF`; CI compile/run probe passed. Full `HAVE_*` audit remains open. |
| 53 | Separate host-platform and PS4-target configuration | **DOCUMENTED** | PSBC CLI is built for Linux host; `-n` selects PS4 NEO/GFX8 output. No cross-compile of the CLI is implied. |
| 54 | Resolve subsequent missing headers or generated files | **PASS — GENERATED SOURCES PRESENT** | Run `44679024542` passed all generated Mesa/NIR/ACO source steps and the compiler link. |
| 55 | Reproduce clean PSBC build | **PASS — HOST COMPILER BUILT** | Pinned PSBC host compiler linked successfully in run `44679024542`; valid shader conversion remains a separate gate. |
| 56 | Resolve compiler/linker errors exposed after C compilation | **PASS — LINKER CLEAN** | Run `44679024542` built the PSBC executable after generated NIR/ACO and format-table sources were added. |
| 57 | Verify PSBC executable and CLI target selection | **PASS — `-n` TARGET CHECK** | Run `44679024542` passed CLI help and PS4 Pro (NEO/GFX8) target selection; malformed-input handling is tracked separately in task 59. |
| 58 | Record compiler/dependency provenance | **IMPLEMENTED — CI REVALIDATION PENDING** | GNM report now records PSBC executable SHA-256, host/tool versions, pinned source revisions, shader source/SPIR-V/GNM hashes. |
| 59 | Exercise compiler positive/negative input tests | **PASS — CLI NEGATIVE INPUT REJECTED CLEANLY** | Run `44680087030` built PSBC and passed the truncated-SPIR-V CLI test: normal error exit, expected diagnostic, no output `.sb`; no segmentation fault. |
| 60 | Accept reproducible GFX8 toolchain | **PARTIAL — HOST/SPIR-V PASS; GNM METADATA GUARD BLOCKS EMISSION** | Run `44682060810` built patched PSBC, passed CLI negative test and all five SPIR-V validations, then refused tiled GEMM with `590 dwords exceeds 8-bit field` rather than emitting invalid `.sb`. SPIR-V artifact uploaded; GNM layout fix remains open. |

## Phase 12 — Compile and inspect GFX8 shaders

| # | Work item | State | Acceptance evidence |
|---:|---|---|---|
| 61 | Compile scalar FP32 GEMM to SPIR-V | **PASS — SPIR-V GENERATED + VALIDATED** | Run `44680087030` generated the scalar FP32 module and passed `spirv-val --target-env vulkan1.1`. |
| 62 | Compile tiled FP32 GEMM to SPIR-V | **PASS — SPIR-V GENERATED + VALIDATED** | Run `44680087030` generated the tiled FP32 module and passed `spirv-val --target-env vulkan1.1`. |
| 63 | Compile FP16-emulation shader to SPIR-V | **PASS — SPIR-V GENERATED + VALIDATED** | Run `44680087030` generated the FP16-emulation module and passed `spirv-val --target-env vulkan1.1`; no native FP16 claim. |
| 64 | Compile E4M3 encode/decode shaders to SPIR-V | **PASS — BOTH SPIR-V MODULES GENERATED + VALIDATED** | Run `44680087030` generated and validated both E4M3 modules with `spirv-val --target-env vulkan1.1`. |
| 65 | Validate all SPIR-V modules | **PASS — FIVE MODULES VALIDATED + HASHED** | Run `44680087030` passed `spirv-val --target-env vulkan1.1` for all five modules and generated the source/SPIR-V provenance manifest; SPIR-V artifact upload passed. |
| 66 | Compile all five shaders for PS4 NEO/GFX8 | **BLOCKED — FAIL-CLOSED GUARD PREVENTS INVALID `.sb`** | Run `44682060810` verified the PSBC guard refuses the tiled shader at 590 dwords; no GNM `.sb` was emitted/uploaded. Five SPIR-V modules remain available. Earlier five `.sb` files from `44681174240` are diagnostic only and invalid for runtime. |
| 67 | Validate GNM containers and CRCs | **BLOCKED — OFFSET OVERFLOW CONFIRMED; FAIL-CLOSED GUARD PASS** | Artifact `44681174240` shows four short binaries resolve their slot table at `0x58`; tiled GEMM needs 590 dwords but stores 78. Host arithmetic tests passed, and run `44682060810` confirmed patched PSBC rejects overflow without emitting `.sb`. The long-shader layout fix remains open in issue [#8](https://github.com/demolidorreddragons/NR/issues/8). |
| 68 | Generate immutable shader artifact manifest | **PARTIAL — SPIR-V PROVENANCE PASS; GNM REPORT BLOCKED** | Run `44682060810` uploaded all five source/SPIR-V modules and the provenance manifest (artifact ID `14604462717`); no GNM binaries/report are produced while the offset guard blocks conversion. |
| 69 | Extract raw OrbShdr metadata | **BLOCKED — SLOT POINTER DOES NOT RESOLVE TO HEADER TABLE** | The raw OrbShdr fields were read, but `chunkUsageBaseOffsetDwords` does not resolve to the input-usage table at the end of `GnmCsShader`; full metadata acceptance is blocked. |
| 70 | Decode ISA and resource/register metadata | **BLOCKED — INVALID GNM METADATA** | LLVM GFX8 disassembly and register decoding remain gated until the slot pointer is valid; no register/resource result accepted. REA-5 corroboration remains open. |

## Phase 13 — GNM compute backend

| # | Work item | State | Acceptance evidence |
|---:|---|---|---|
| 71 | Determine actual shader resource ABI | **BLOCKED BY 70** | Emitted metadata and ISA identify resource classes and user-data mapping. |
| 72 | Validate read/write descriptor representation | **BLOCKED BY 71** | Descriptor bytes independently checked against shader ABI. |
| 73 | Establish supported GPU memory type/protection | **PARTIAL — API CANDIDATE IDENTIFIED** | Pinned OpenGNM VideoOut helper uses WC_GARLIC + CPU/GPU RW; compute-buffer suitability is not established. |
| 74 | Establish CPU/GPU address mapping | **PARTIAL — DIRECT-MEMORY OFFSET FOUND** | Helper retains CPU mapping and direct-memory offset; generic compute GPU-VA translation remains unproven. |
| 75 | Implement bounded GPU buffer lifecycle | **PARTIAL — UPSTREAM CANDIDATE REVIEWED** | OpenGNM helper checks alignment/overflow and rolls back map failure; NR compute lifecycle not implemented. |
| 76 | Implement cache-coherency protocol | **BLOCKED** | No compute-buffer flush/invalidate sequence verified. |
| 77 | Configure compute stage from real shader metadata | **BLOCKED BY 70** | Register setup matches compiled binary, not guessed constants. |
| 78 | Build command-buffer recording path | **PARTIAL — API SURFACE DOCUMENTED** | CS bind, V# user-data and dispatch packet helpers identified; resource ABI still blocks safe binding. |
| 79 | Implement submission, completion and recovery | **PARTIAL — API SURFACE DOCUMENTED** | Submit and submit-done APIs identified; NR compute lifecycle/timeout/recovery unimplemented. |
| 80 | Build isolated GNM compute harness | **BLOCKED BY 79** | Minimal reproducible harness with logs and cleanup. |

## Phase 14 — Hardware validation, model and renderer

| # | Work item | State | Acceptance evidence |
|---:|---|---|---|
| 81 | Approve reversible PS4 Pro test procedure | **PLAN DRAFTED — EXECUTION NOT AUTHORIZED** | `docs/PS4_PRO_REVERSIBLE_TEST_PLAN.md` defines read-only preflight, separate approval gates, exact backup/recovery requirements and PASS/FAIL/BLOCKED criteria. No console action is authorized. |
| 82 | Dispatch minimal shader on PS4 Pro | **BLOCKED BY 81** | Hardware log and verified output buffer; no claim from host tests. |
| 83 | Execute FP32 GEMM on hardware | **BLOCKED BY 82** | GPU output compared against independent CPU reference. |
| 84 | Execute FP16 path and measure accuracy | **BLOCKED BY 82** | ISA classification, numerical error and timing; no assumed native FP16. |
| 85 | Recover and authenticate model archives/resources | **PASS — INPUTS RECOVERED AND REPRODUCED** | Three ZIP hashes, DLL/resource hashes, 153 records, contiguous tensor slices and `s00.bin`/manifest byte-for-byte re-export verified. See `docs/DLSSNR_310_8_3_RESOURCE.md`; proprietary payloads remain out of Git. |
| 86 | Verify 310.8.0/310.8.3 semantic compatibility | **PARTIAL — WEIGHTS + ALL 153 LAYOUTS MATCH REFERENCE; DLL GRAPH PARITY OPEN** | Exact `WEIGHTS_HT` resource hash parity and all 153 tensor names/byte lengths match the pinned 310.8.0 graph formulas; full 310.8.3 DLL graph/preprocessing/runtime code is not independently compared. See `docs/DLSSNR_310_8_3_RESOURCE.md`. |
| 87 | Build executable representation of all 71 graph blocks | **PARTIAL — 71-BLOCK LAYOUT EXPORT + HOST TESTS PASS; INFERENCE NOT INTEGRATED** | `tools/verify_dlssnr_tensor_layout.py` verifies all 153 packed records; `tools/export_dlssnr_graph_layout.py` emits 71 blocks, 11 transitions and 153 tensor layouts. Host tests passed run `44683109821`; PS4 graph execution remains unimplemented. |
| 88 | Implement and differential-test partial inference | **BLOCKED BY 87** | Per-operator reference comparisons including edge cases and tolerances. |
| 89 | Integrate neural backend with renderer and fallback | **BLOCKED BY 80, 88** | Defined image input/output ownership, fallback path and end-to-end frame tests. |
| 90 | Independent audit and experimental release gate | **PARTIAL — REPEATED PKG PC QA PASS; RELEASE NOT APPROVED** | Independent PKG QA passed on runs `44680087099` and `44680530122`, including structure/digest/signature/payload validation, 15/15 extraction and semantic SFO checks. REA-5 clean-room review, hardware/ABI/runtime/rollback evidence and explicit release approval remain open. |

## Current concrete progress

- Task 51 patch committed as `cb66861b05fbe9601300b3bce39f99cc212447d4`.
- The observed failure in run `44676657053` was missing `HAVE_SYSCONF` in the hand-written Linux host build configuration. The pinned Mesa source uses that feature macro to select POSIX `sysconf` for physical-memory and page-size queries. The workflow now checks that capability with a compiled/running probe. Host numeric regression tests on commit `92a3bc3f` passed run `44677189726`. The latest non-canceled compiler attempt `44677801306` passed dependency checkout, host configuration, the `sysconf` probe and all Mesa header generators; the PSBC build is now running.
- Revalidation is required. No task from 61 onward is marked complete; no accepted `.sb` artifacts, GNM dispatch, PS4 Pro GPU measurements, model intake, or neural renderer are claimed.
- REA-5 remains **NOT EXECUTED**. It is a documented independent-review protocol, not a separately instantiated agent.
- Console installation, plugin activation, `plugins.ini` changes, saves/app-data changes and persistent console modifications remain outside this work.

## Execution rule

Proceed in dependency order. When an external prerequisite is absent (PS4 Pro test access, model archives, or user approval for a reversible hardware procedure), mark the affected task **BLOCKED** and continue with independent repository/host work rather than inventing evidence.


## Additional source-level correctness correction

The scalar FP32 and FP16-emulation GEMM shaders now skip reading C when beta is exactly zero, preventing NaN propagation from an unused residual buffer. Host regression tests cover this contract and E4M3FN edge behavior. This does not count as shader compilation or GPU execution.


Task 65 implementation was added in commit `1809a26942db7d25f61e4620de0c3e88816d63bf`; commit `5849ef2b` fixed a shell-variable escaping error that would have prevented the validator from opening the five module paths. No SPIR-V validation is marked passed until CI executes it.


Task 68 provenance fields and report artifact upload were added in commit `5b91e7e0d4c38ccbf64c66641b8611799f3b8656`. Task 69 raw OrbShdr offsets and fields were already collected by the workflow's report code, but their correctness remains subject to a successful build and later independent interpretation.


Task 67 structural validation was hardened in commit `9dbebb41c8ef61a5b6f4797e6a25a0421edd2477`: the report stage now rejects invalid code ranges and input-usage slot ranges, and includes truncated-header and corrupted-CRC negative controls. The checks are implemented but await actual GNM shader files.


Tasks 57–59 received additional CI gates in commit `6570ece7959f36f4e15268d1a4512fc2118baa53`: PSBC CLI help must advertise PS4 Pro NEO/GFX8, malformed SPIR-V must be rejected without producing an output file, and the artifact report records host/compiler tool versions plus the PSBC executable hash. These gates await the PSBC host build completing successfully.


The pinned GNM memory/submission investigation is recorded in `docs/REA_GNM_MEMORY_SUBMIT_FINDINGS.md` (commit `e91ca768598a61a2f69ae9ec48974740c37e3a4d`). It narrows the candidate APIs but deliberately does not infer compute memory semantics from the VideoOut helper.


## Additional host-side hardening (2026-11-20)

- Model intake verifier now rejects symbolic-link inputs instead of following paths outside the supplied intake directory. Tests cover symlink rejection and size mismatch even when a digest matches the fixture. Commit: `ce319840a21ef218dd5cd8bc145a8d04ab3f58c4` (verifier), followed by `f7a2ad21db452f93af9847baaf6747784c4d611b` (tests). The real model archives remain absent, so task 85 stays blocked.
- GFX8 workflow concurrency now queues follow-up commits instead of canceling an active compiler build mid-checkout. Commit: `e9acec50f1b63a1242448b47e4ef240c92a6a27f`. A successful run is still required for tasks 51 and 55–70.

- Tiled GEMM host emulation now runs 40 deterministic randomized shape/value cases (including non-multiple-of-16 boundaries) and verifies that `beta=0` does not propagate NaNs from C. Commit: `8a605b753c20aca544754ca8db461735b4454a9b`. This remains CPU emulation, not GPU shader execution.


Task 69 extraction was expanded in commits `9fdef49f7186e07500596e74b4ab223ac45b39bb` and `e3bce82e8261bc7a37db4a7765c39cc091076755`. The report preserves raw register words only; no register bitfield semantics or resource ABI are inferred. It also records SPIR-V LocalSize for each entry point. CI must produce and validate actual compiler output before the task is accepted.

- GNM report validation now cross-checks the three compiled CS thread dimensions against the SPIR-V `LocalSize` and verifies the CS program-code offset against the declared header size (commit `5750d92768846cfea35f0d902a3a024a65cdd432`). This is a consistency check, not ISA register decoding or proof of runtime dispatch.


Task 70 now has a disassembly pipeline in commit `634bb64c7f7d96e328b69c553706db712cade08b`, with target-probe correction in `3016a8a310143bbef524ebb9c06e936fb43851f2`. CI installs LLVM 18, extracts the exact GCN code range, preserves raw code bytes, and disassembles with the AMDGCN Tonga target. The `.isa` output and hashes are included in the artifact. This is not yet a register-resource audit: VGPR/SGPR/LDS/scratch values still require interpretation of raw CS registers and corroboration against decoded ISA.

- PRX/PKG workflow concurrency was changed in commit `7e7ebe87a4209c350a6efe20c51373506a99d182` to queue later commits rather than cancel an active package build. This protects artifact reproducibility; it does not alter the console or install anything.


Task 70 resource decoding was added in commits `085c3ba230d53ac9d40f54709fa9ad513ff09d9b` and `ce12f02886dccb496ba97603c97de80a173028fc`. The report decodes COMPUTE_PGM_RSRC1/2 field boundaries from the pinned Mesa GFX8 register JSON and derives VGPR/SGPR allocation granules, LDS allocation units, scratch bytes/thread, user SGPR count and wave64 target. The report preserves raw register words alongside the decoded fields and records the basis used. This remains a compiler-side interpretation; REA-5 has not independently verified it and no GPU runtime allocation/occupancy measurement is claimed.


The first non-canceled PSBC build attempt `44677801306` passed host setup, sysconf probe and Mesa header generation, compiled most of the compiler, then failed at final link with unresolved `nir_op_infos`, `nir_intrinsic_infos` and sRGB table symbols. Inspection of the pinned Makefile showed NIR/ACO generated `.c` sources are prerequisites but `NIR_SRCS`/other source wildcards are expanded before those files exist. The workflow now runs the upstream `make generated` target before launching a fresh `make -j2`, and separately generates `src/util/format_srgb.c` per Mesa's `src/util/meson.build`. Commit: `f15abeebbcb71e94438f28ba102b158c0f8a8832`. This is a source-level correction awaiting CI revalidation, not a successful compiler build.


The next PSBC link attempt `44678629656` passed all generated-source steps and compiled the large Mesa/NIR/ACO object set, then exposed two further Makefile integration gaps: missing `gfx10_format_table`/`gfx11_format_table` generated definitions and unresolved BLAKE3 x86 SIMD functions (the standalone Makefile lists dispatch/portable sources but omits the upstream assembly objects). The workflow now generates `gfx10_format_table.c` using the pinned Mesa generator and configures BLAKE3's documented portable-only build macros for this host compiler. This does not affect PS4 GPU shader code. Commit: `c379045aa03e4ad48b8d5cb812375395e77e93aa`; awaiting revalidation.


## PSBC first valid-shader conversion attempt — run 44679024542

The pinned PSBC host compiler now builds and links on Ubuntu after generating NIR/ACO sources, sRGB and GFX10/11 format tables, and using BLAKE3 portable mode for the standalone Makefile. All five NR compute shaders compile to SPIR-V and pass `spirv-val --target-env vulkan1.1`. The first GNM conversion then aborts on the tiled GEMM shader at `assert(config->lds_size <= compiler_info->ac->lds_size_per_workgroup)`: the pinned standalone `setup_ac_info()` never initialized this field, unlike Mesa's `ac_fill_compiler_info()`. Source review of `src/amd/common/ac_gpu_info.c` confirms GFX7+ workgroups may use 64 KiB LDS. A narrow patch now sets the documented limit and the CLI rejects truncated SPIR-V headers before entering NIR/ACO. This is not yet validated by a new run; no `.sb` is accepted. The same run also showed the old malformed-input test had incorrectly treated a segmentation fault as a successful rejection; the new gate rejects signal exits.
