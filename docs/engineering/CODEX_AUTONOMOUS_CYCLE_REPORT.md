# Codex Autonomous Engineering Cycle Report

Date: 2026-11-23

## Checkout and PR context

- Supplied exact checkout/base SHA: `c7de2359a8266f20a6ddd98a1fc81aec11cea47d` (`NR_BASE_SHA`).
- PR #63: open, base `main` at `d0d261bd92f88e1e4e9c1d509db4d1d37093d84f`, head branch `engineering/gcn8-compute-readiness-audit` at `3a007ff856f1819308f20e06eaad70bfe68c7b6e`.
- Supplied PR changed files: GCN8 artifact-gate workflow, host-test workflow, Windows Codex install workflow, read-only installed-app audit workflow, CMake host contracts, GCN8 readiness audit, PS4 bridge documentation, PSBC two-SGPR patch, GCN8 fixture/shader/parser/verifier and host tests.
- The checkout did not expose a `.git` directory and `git` was not available on PATH, so runtime branch/status/diff inspection was unavailable. The supplied SHA and PR metadata above are recorded as input facts, not independently queried.

## Files inspected

- `AGENTS.md` (read in full).
- `docs/GCN8_GNM_COMPUTE_READINESS.md`, `docs/PS4_WINDOWS_BRIDGE.md`.
- `.github/workflows/gcn8-compute-artifact-gate.yml`, `.github/workflows/host-tests.yml`.
- `platform/ps4/gcn8/host/compute_shader_contract.{h,cpp}`.
- `platform/ps4/gcn8/host/resource_table_contract.{h,cpp}`.
- `platform/ps4/gcn8/host/known_answer_contract.{h,cpp}`.
- `platform/ps4/goldhen/plugin/main.c`, `platform/ps4/goldhen/plugin/README.md`, `platform/ps4/goldhen/plugin/telemetry_bridge.{h,cpp}`.
- `src/telemetry_bridge.cpp`, `src/telemetry.cpp`, `include/pne/telemetry.hpp`.
- `tests/gcn8_compute_smoke.comp`, `tests/verify_gcn8_compute_sb.py`, `tests/test_gcn8_compute_sb_parser.py`, `tests/test_gcn8_compute_shader_contract.cpp`, `tests/test_gcn8_known_answer.cpp`, `tests/test_gcn8_resource_table.cpp`, `tests/test_psbc_descriptor_pointer_patch.py`, and `tests/fixtures/gcn8-compute-smoke.sb`.
- `patches/opengnm-psbc/0001-gcn8-descriptor-pointer-reserve-two-sgprs.patch`, `CMakeLists.txt`.

## Technical finding

The repository's highest-priority Gate B blocker is the absence of a PS4 runtime compute path. The GCN8 directory contains host-only metadata/layout/oracle contracts. `rg` found no runtime references in the checked-out project to `sceGnmSetCsShader`, `sceGnmDispatchDirect`, `sceGnmSubmitCommandBuffers`, `sceGnmSubmitDone`, or `sceGnmDrawCmdDispatchDirect`. `platform/ps4/goldhen/plugin/main.c` hooks `sceGnmSubmitAndFlipCommandBuffers` only to record present timing and display-buffer metadata. Its README explicitly says there is no compute backend and requires a title adapter to provide readback, fence completion, and cache coherency before passing pixels to telemetry.

What is established locally: the hosted Gate A workflow pins compiler/dependency revisions, applies the descriptor pointer patch, compiles a compute artifact, and runs an independent container/metadata verifier. The supplied readiness record documents Gate A as artifact-only PASS and gives artifact evidence; the verifier itself marks hardware dispatch and firmware 14.02 validation false. The checked-in smoke shader and host known-answer contract define a deterministic workload but do not submit it to a GPU.

What remains unproven: the exact retail firmware 14.02 call ABI/exports and command-buffer lifecycle; GPU allocation, address/residency, and descriptor contents; shader address patching; compute command submission and completion; cache-coherent GPU-to-CPU readback; and a PS4 Pro known-answer execution. The supplied readiness document also identifies source-level disagreement in candidate dispatch packet sizing and known synchronization/readback defects in the examined external Vulkan candidate. These are reasons to avoid adding guessed firmware calls or promoting host/emulator checks to hardware evidence.

## Engineering changes

No runtime code change was justified by the checked-in evidence. This cycle adds this source-audit report as the concrete engineering output, with the exact next implementation slice below. No workflow, PS4, deployment, plugin, package, or user-data files were changed.

## Tests and commands

- `python -m unittest tests/test_gcn8_compute_sb_parser.py` — exit 0; 7 tests passed.
- `python -m unittest tests/test_psbc_descriptor_pointer_patch.py` — exit 0; 2 tests passed.
- C++ CMake/CTest checks were not run: `cmake`, `g++`, and `clang++` were not available in this execution environment. (`CMakeLists.txt` registers the three GCN8 host contracts.)
- Git diff/status could not be inspected because `.git` was absent and `git` was unavailable on PATH. No commit was made.

## Gate status

- **Gate A:** PASS for the artifact-only result recorded in the supplied readiness document; not independently rerun in this local cycle. No execution or firmware compatibility is implied.
- **Gate B:** BLOCKED. There is no in-repository runtime dispatch, allocation, submission, synchronization, or readback implementation, and the required firmware/runtime contracts are not established.
- **Gate C:** NOT PASSED. No PS4 Pro dispatch, fence/completion evidence, coherent readback, or full known-answer comparison was performed.
- **Gate D:** NOT STARTED / BLOCKED by Gate C. No neural reconstruction integration or visual/performance validation is present.

## Exact next step

Before writing PS4 runtime code, obtain and independently verify authoritative firmware-14.02-compatible ABI and lifecycle evidence for one isolated, opt-in known-answer harness: command-buffer ownership/submission, shader and descriptor allocations/addressing/residency, dispatch setup, bounded completion/recovery, and a cache-coherent readback path. Then implement only that harness behind a separate build/runtime gate, add host tests for its ABI-neutral validation pieces, and require actual console output/hash comparison before calling Gate C passed. Keep it separate from the telemetry PRX until those contracts are demonstrated.
