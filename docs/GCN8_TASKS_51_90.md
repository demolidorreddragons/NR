# Neural Rendering execution ledger — tasks 51–90

Updated: 2026-11-20  
Branch: `feature/gcn8-numeric-kernels`

This ledger records the next 40 work items. A task is **DONE** only when its stated evidence exists. A source edit, host compile, or CI green result is not evidence of PS4 Pro GPU execution.

## Phase 11 — Unblock the pinned PSBC host build

| # | Work item | State | Acceptance evidence |
|---:|---|---|---|
| 51 | Correct Linux `sysconf` feature detection in host build | **PATCHED — REVALIDATION PENDING** | `HAVE_SYSCONF=1` and a compiled POSIX probe are present; latest workflow has not reached PSBC compilation. |
| 52 | Audit host OS feature macros against pinned Mesa configuration | **PARTIAL — SYSCONF SOURCE VERIFIED** | Pinned source and Orbis config use `HAVE_SYSCONF`; CI capability probe is implemented but latest run has not executed it. Full `HAVE_*` audit remains open. |
| 53 | Separate host-platform and PS4-target configuration | **DOCUMENTED** | PSBC CLI is built for Linux host; `-n` selects PS4 NEO/GFX8 output. No cross-compile of the CLI is implied. |
| 54 | Resolve subsequent missing headers or generated files | **PENDING** | Only fix errors reproduced in CI logs; preserve upstream declarations. |
| 55 | Reproduce clean PSBC build | **BLOCKED BY 51** | Clean runner build completes from pinned revisions. |
| 56 | Resolve compiler/linker errors exposed after C compilation | **PENDING** | Successful final link with no suppressed errors. |
| 57 | Verify PSBC executable and CLI target selection | **PENDING** | Executable runs; help and `-n` select PS4 NEO/GFX8. |
| 58 | Record compiler/dependency provenance | **PENDING** | SHA-256, commit IDs, compiler versions, build flags and artifact manifest. |
| 59 | Exercise compiler positive/negative input tests | **PARTIAL — HOST NUMERIC REGRESSIONS PASS** | Host CI run `44677189726` passed E4M3FN edge cases, beta-zero/NaN isolation and tiled GEMM reference tests; PSBC CLI malformed-input tests remain pending. |
| 60 | Accept reproducible GFX8 toolchain | **PENDING** | All prior gates pass; host build alone is not shader correctness. |

## Phase 12 — Compile and inspect GFX8 shaders

| # | Work item | State | Acceptance evidence |
|---:|---|---|---|
| 61 | Compile scalar FP32 GEMM to SPIR-V | **BLOCKED BY 60** | Validated SPIR-V artifact and hash. |
| 62 | Compile tiled FP32 GEMM to SPIR-V | **BLOCKED BY 60** | Validated SPIR-V artifact and hash. |
| 63 | Compile FP16-emulation shader to SPIR-V | **BLOCKED BY 60** | Validated artifact; no native FP16 claim. |
| 64 | Compile E4M3 encode/decode shaders to SPIR-V | **BLOCKED BY 60** | Both artifacts validated; software conversion semantics documented. |
| 65 | Validate all SPIR-V modules | **IMPLEMENTED — CI REVALIDATION PENDING** | Workflow installs `spirv-tools`; corrected shell expansion now validates each of the five module paths using `spirv-val --target-env vulkan1.1`. |
| 66 | Compile all five shaders for PS4 NEO/GFX8 | **PENDING** | Five non-empty GNM shader binaries from pinned PSBC with `-n`. |
| 67 | Validate GNM containers and CRCs | **IMPLEMENTED — CI REVALIDATION PENDING** | Added code-range/slot bounds checks plus negative controls for truncated header and corrupted CRC; must run on real compiler output. |
| 68 | Generate immutable shader artifact manifest | **IMPLEMENTED — CI REVALIDATION PENDING** | Container report now records source/SPIR-V/GNM hashes, sizes and pinned compiler/dependency revisions; report is included in uploaded artifact. |
| 69 | Extract raw OrbShdr metadata | **PENDING** | Offsets/lengths and raw fields preserved with bounds checks. |
| 70 | Decode ISA and resource/register metadata | **PENDING** | Disassembly and independently checked VGPR/SGPR/LDS/scratch/wave/resource interpretation. |

## Phase 13 — GNM compute backend

| # | Work item | State | Acceptance evidence |
|---:|---|---|---|
| 71 | Determine actual shader resource ABI | **BLOCKED BY 70** | Emitted metadata and ISA identify resource classes and user-data mapping. |
| 72 | Validate read/write descriptor representation | **BLOCKED BY 71** | Descriptor bytes independently checked against shader ABI. |
| 73 | Establish supported GPU memory type/protection | **PENDING** | Firmware/API evidence and safe allocation constraints. |
| 74 | Establish CPU/GPU address mapping | **BLOCKED BY 73** | Valid GPU VA, alignment and ownership semantics documented. |
| 75 | Implement bounded GPU buffer lifecycle | **BLOCKED BY 73–74** | Allocation/map/unmap/release with failure-path tests. |
| 76 | Implement cache-coherency protocol | **BLOCKED BY 74** | Verified flush/invalidate and ordering semantics. |
| 77 | Configure compute stage from real shader metadata | **BLOCKED BY 70** | Register setup matches compiled binary, not guessed constants. |
| 78 | Build command-buffer recording path | **BLOCKED BY 71–77** | Correct resource binding and dispatch packet construction. |
| 79 | Implement submission, completion and recovery | **BLOCKED BY 78** | Verified queue lifecycle, timeout/error path and resource lifetime. |
| 80 | Build isolated GNM compute harness | **BLOCKED BY 79** | Minimal reproducible harness with logs and cleanup. |

## Phase 14 — Hardware validation, model and renderer

| # | Work item | State | Acceptance evidence |
|---:|---|---|---|
| 81 | Approve reversible PS4 Pro test procedure | **NOT STARTED** | Explicitly approved procedure, preconditions, recovery plan; no install or persistent changes implied. |
| 82 | Dispatch minimal shader on PS4 Pro | **BLOCKED BY 81** | Hardware log and verified output buffer; no claim from host tests. |
| 83 | Execute FP32 GEMM on hardware | **BLOCKED BY 82** | GPU output compared against independent CPU reference. |
| 84 | Execute FP16 path and measure accuracy | **BLOCKED BY 82** | ISA classification, numerical error and timing; no assumed native FP16. |
| 85 | Recover and authenticate model archives/resources | **BLOCKED — INPUTS ABSENT** | All expected source archive/resource SHA-256 checks pass. |
| 86 | Verify 310.8.0/310.8.3 semantic compatibility | **BLOCKED BY 85** | Tensor names/shapes/layouts/operators and graph mapping independently reconciled. |
| 87 | Build executable representation of all 71 graph blocks | **BLOCKED BY 86** | Complete operator/tensor graph with validated weights and shapes. |
| 88 | Implement and differential-test partial inference | **BLOCKED BY 87** | Per-operator reference comparisons including edge cases and tolerances. |
| 89 | Integrate neural backend with renderer and fallback | **BLOCKED BY 80, 88** | Defined image input/output ownership, fallback path and end-to-end frame tests. |
| 90 | Independent audit and experimental release gate | **NOT APPROVED** | REA-5 clean-room review, hardware evidence, package/ABI/rollback audit and explicit release approval. |

## Current concrete progress

- Task 51 patch committed as `cb66861b05fbe9601300b3bce39f99cc212447d4`.
- The observed failure in run `44676657053` was missing `HAVE_SYSCONF` in the hand-written Linux host build configuration. The pinned Mesa source uses that feature macro to select POSIX `sysconf` for physical-memory and page-size queries. The workflow now checks that capability with a compiled/running probe. Host numeric regression tests on commit `92a3bc3f` passed run `44677189726`; the GCN8 compiler workflow remains queued/cancelled during overlapping CI runs, so task 51 is not yet revalidated.
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
