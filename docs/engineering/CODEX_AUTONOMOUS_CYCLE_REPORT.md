# Codex Autonomous Engineering Cycle Report

Date: 2026-11-23

## Checkout and PR context

- Exact supplied checkout SHA (`NR_BASE_SHA`): `e5eb8a59783919b0188bd12c0b8d8ee73e6b3eb2`.
- PR #63 is open, base `main` at `d0d261bd92f88e1e4e9c1d509db4d1d37093d84f`, head branch `engineering/gcn8-compute-readiness-audit` at `3a007ff856f1819308f20e06eaad70bfe68c7b6e`.
- Supplied PR changed-file context: `.github/workflows/gcn8-compute-artifact-gate.yml`, `.github/workflows/host-tests.yml`, `.github/workflows/install-codex-windows-runner.yml`, `.github/workflows/ps4-installed-app-readonly-audit.yml`, `CMakeLists.txt`, `docs/GCN8_GNM_COMPUTE_READINESS.md`, `docs/PS4_WINDOWS_BRIDGE.md`, the GCN8 PSBC patch, GCN8 host contracts and tests, the shader fixture/source, and independent verifier.
- Git is unavailable on PATH and this checkout has no `.git` directory. The supplied SHA and PR metadata are treated as task input, not independently queried. No commit was made. Accordingly, a Git diff could not be generated or verified.

## Files inspected

- `AGENTS.md` (complete).
- `.github/workflows/gcn8-compute-artifact-gate.yml`, `.github/workflows/host-tests.yml`, `.github/workflows/ps4-prx.yml`, `.github/workflows/codex-autonomous-cycle.yml`.
- `docs/GCN8_GNM_COMPUTE_READINESS.md`, `docs/UPLOAD.md`, `docs/PS4_WINDOWS_BRIDGE.md`.
- `platform/ps4/goldhen/plugin/github_client.cpp`, `main.c`, `Makefile`, `plugin_common.h`, `README.md`, `telemetry_bridge.{h,cpp}`.
- `platform/ps4/gcn8/host/compute_shader_contract.{h,cpp}`, `resource_table_contract.{h,cpp}`, `known_answer_contract.{h,cpp}`.
- `CMakeLists.txt`, `tests/test_gcn8_compute_sb_parser.py`, `tests/test_psbc_descriptor_pointer_patch.py`, `tests/test_gcn8_compute_shader_contract.cpp`, `tests/test_gcn8_known_answer.cpp`, `tests/test_gcn8_resource_table.cpp`, `tests/verify_gcn8_compute_sb.py`, `tests/gcn8_compute_smoke.comp`, and `tests/fixtures/gcn8-compute-smoke.sb`.
- Existing contract scripts for CDA evidence, PS4 control-plane collection, installed-package inspection, package archival, RPI readiness, and Windows runner diagnostics.

## Technical findings

### Upload confirmation

`github_client.cpp` previously bound its two-press upload confirmation to `g_pending_count` and `g_pending_bytes`. Two distinct file sets with equal count and total size therefore inherited the same confirmation. Upload enumeration and file reads happened later in `upload_log()` / `upload_one()`, leaving a replacement window between preview and sending.

The new confirmation identity is the sorted set of relative file names, byte counts, and SHA-256 content digests. A changed set re-arms the confirmation. The worker hashes the eligible set again before upload, and each file's exact byte buffer is digest-checked immediately before its GitHub request. Thus a same-size replacement cannot cause unconfirmed bytes to be sent. Authorization start, successful authorization, and disconnect clear pending confirmation. A new authorization attempt also clears the previous in-memory authorization; it is rejected while an upload is active.

A second review found the old preview itself was lossy: it used a 160-byte name buffer and a 256-byte message, omitted per-file sizes, and silently truncated longer batches. The preview now lists every sorted filename and exact byte count. `NotifyStaticChecked` compares the complete message length against the actual platform notification buffer and reports delivery success. If the list does not fit or notification delivery fails, the pending confirmation is invalidated and upload is blocked. The preview builder also rejects duplicate/unsafe names and overflowed totals.

This is a host-testable source change. No PS4 API was added or changed, and no console upload was attempted.

### GCN8 gates

The repository contains host-side GNM metadata, resource-table, and known-answer contracts, plus the Gate A compiler workflow and a 228-byte compute artifact fixture. `main.c` hooks `sceGnmSubmitAndFlipCommandBuffers` only for present telemetry. No checked-in runtime dispatch, compute allocation/residency, submission, completion, or coherent GPU readback path was found. The readiness document records candidate-source disagreements and does not establish the exact retail firmware 14.02 ABI or lifecycle.

The local fixture was independently read and parsed with the checked-in Python layout parser: 228 bytes, SHA-256 `8f52fb74771164498a0c1e90bd828d8517f6414d34a1d28c395c9aae2563b6e1`, compute stage, 64x1x1 local size, descriptor slot 0 at register 2, and four user SGPRs. This reproduces fixture metadata only. The pinned compiler/dependency trees needed to rerun the full Gate A verifier/build are not present in this checkout.

## Files changed

- `platform/ps4/goldhen/plugin/github_client.cpp` — content-bound batch snapshot, revalidation, complete preview, and pre-request digest verification; authorization lifecycle invalidation.
- `platform/ps4/goldhen/plugin/upload_confirmation.h` — host-testable file identity/confirmation API.
- `platform/ps4/goldhen/plugin/upload_confirmation.cpp` — deterministic SHA-256, order-independent snapshot comparison, and bounded complete-preview formatting.
- `platform/ps4/goldhen/plugin/Makefile` — compile and link the confirmation helper into the PRX; track its header dependency.
- `platform/ps4/goldhen/plugin/plugin_common.h` and `plugin_common.c` — checked notification delivery that rejects truncation.
- `tests/test_upload_confirmation.cpp` — SHA-256 padding-boundary vectors, exact-file-set/content/addition/removal regressions, complete sorted preview, truncation, duplicate-name, and unsafe-name checks.
- `docs/UPLOAD.md` — implementation scope, complete per-file preview, and fail-closed notification behavior.
- `docs/engineering/CODEX_AUTONOMOUS_CYCLE_REPORT.md` — this report.

No `.github/workflows` file was edited. No PRX compile workflow was run: the inspected `ps4-prx.yml` installs packages and downloads the OpenOrbis toolchain and other dependencies, conflicting with this cycle's no-network/no-dependency-fetch constraint. It is hosted on Ubuntu and contains no PS4 endpoint calls, but an Actions run was not available from this local-only session. No complete diff or changed-file archive artifact could be generated locally because Git is absent; this report names every file changed by the cycle.

## Tests and commands

- `python -m unittest tests/test_gcn8_compute_sb_parser.py` — exit 0; 7 tests passed.
- `python -m unittest tests/test_psbc_descriptor_pointer_patch.py` — exit 0; 2 tests passed.
- `python -c "import pathlib,hashlib,sys;sys.path.insert(0,'tests');from verify_gcn8_compute_sb import parse_compute_shader_layout;p=pathlib.Path('tests/fixtures/gcn8-compute-smoke.sb');b=p.read_bytes();print('fixture_bytes=',len(b));print('fixture_sha256=',hashlib.sha256(b).hexdigest());print(parse_compute_shader_layout(b))"` — exit 0; fixture fields and SHA-256 listed above.
- `python tests/test_cda_pkg015_log_extractor_contract.py` — exit 0; contract PASS.
- `python tests/test_ps4_control_plane_collector_contract.py` — exit 0; contract PASS.
- `python tests/test_ps4_installed_package_inspector_contract.py` — exit 0; contract PASS.
- `python tests/test_ps4_pkg_archive_contract.py` — exit 0; contract PASS.
- `python tests/test_ps4_rpi_readiness_probe_contract.py` — exit 0; contract PASS.
- `python tests/test_windows_runner_service_diagnostics_contract.py` — exit 0; contract PASS.
- The Windows Codex runner has no `cmake`, `clang++`, `g++`, or `c++`, so it could not run CMake/CTest or the PRX build. Separately, a Linux host harness built the upload-confirmation implementation and regression cases with C++17, `-fno-exceptions -fno-rtti`, warnings-as-errors, AddressSanitizer, and UndefinedBehaviorSanitizer. SHA-256 vectors at lengths 0, 3, 55, 56, 63, 64, 65, 119, 120, 127, and 128; exact batch identity; add/remove/content changes; complete preview formatting; and truncation/unsafe-name rejection all passed. This was a standalone host harness, not a full repository CMake/CTest build or PS4 SDK compile. A C11 stub harness for `NotifyStaticChecked` verified exact delivery, rejection of over-capacity messages, and no notification call on rejection. GitHub host-tests PR run `45292520548` is `action_required`; no C++ CI result is claimed.
- An initial mistaken attempt to pass the C++ source to Python (`python tests/test_upload_confirmation.cpp`) exited 1 with Python `SyntaxError`; it did not execute the C++ test.
- Full Gate A compiler reproduction and PS4 PRX compilation were not run. The required pinned dependency trees/toolchain are not local, and fetching dependencies or invoking remote workflows was outside this task's permitted actions.
- No PS4, LAN, FTP, RPI, KLog, BinLoader, deployment, credential, or secret access occurred.

## Gate status

- **Gate A:** artifact-only PASS remains supported by the supplied readiness/evidence record. This cycle independently checked the local fixture metadata only; it did not reproduce the compiler build or upstream CRC verifier.
- **Gate B:** BLOCKED. No qualified firmware 14.02 compute ABI, command-buffer lifecycle, allocation/residency, dispatch, completion/recovery, or coherent readback contract is established in checked-in code/evidence.
- **Gate C:** NOT PASSED. No hardware dispatch, completion/fence evidence, readback, or console known-answer comparison was performed.
- **Gate D:** NOT STARTED; remains gated on Gate C and title-specific render-target/readback validation.

## Remaining blockers and exact next steps

1. Run CMake/CTest for `upload_confirmation`, `gcn8_resource_table_contract`, `gcn8_known_answer_contract`, and `gcn8_compute_shader_contract` in an authorized local/CI environment with a C++17 toolchain. Then inspect the PS4 PRX compile result after dependency access is separately permitted; do not infer firmware behavior from compilation.
2. For Gate A independent reproduction, provide the exact pinned compiler and sibling dependency sources (or an independently reproducible artifact bundle) locally, then run the verifier and compare the resulting shader hash/report.
3. Before Gate B runtime code, obtain independently verifiable FW 14.02 evidence for exports and ABI, command-buffer ownership/submission, shader and descriptor placement/residency, dispatch packet contract, bounded completion/recovery, and cache-coherent readback. Implement the isolated known-answer harness only after those contracts are resolved.
4. Gate C requires an explicitly controlled PS4 Pro test with full GPU output comparison and retained evidence. Gate D follows only after Gate C passes.


## Follow-up implementation and host validation

The follow-up review closed a user-visible integrity gap in the preview. A confirmation is no longer armed unless the complete per-file list and byte sizes can be sent without truncation. If it cannot fit the SDK's actual notification message field, or the notification call fails, the batch is disarmed and upload does not start.

Additional host validation performed against the current branch source snapshot:
- C++17 standalone upload helper/test harness with `-fno-exceptions -fno-rtti`, warnings-as-errors, AddressSanitizer, and UndefinedBehaviorSanitizer — exit 0.
- SHA-256 vectors for empty input, short input, the 55/56-byte padding transition, 63/64/65-byte block transition, 119/120/127/128-byte transitions, and the published multi-block vector — PASS.
- Preview ordering, exact per-file byte sizes, exact-capacity success, one-byte-too-small rejection, duplicate filename rejection, and control-character filename rejection — PASS.
- Confirmation re-arming for same-count/same-size filename changes, content changes, file additions, and file removals — PASS.
- C11 stub harness for checked notification delivery — exit 0; accepted a complete message, rejected a message at/over notification-field capacity, and did not call the platform send stub for rejected input.

The standalone host harness does not replace the full CMake/CTest CI run or the PS4 SDK PRX build. No PS4 runtime, network upload, or hardware gate was exercised.
