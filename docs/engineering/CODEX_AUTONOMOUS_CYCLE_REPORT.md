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
- C++ command: `g++ -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer /tmp/nr-host-check/upload_confirmation.cpp /tmp/nr-host-check/test_upload_confirmation.cpp -o /tmp/nr-host-check/test_upload_confirmation_ps4flags && /tmp/nr-host-check/test_upload_confirmation_ps4flags` — exit 0.
- C notification stub command: `gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -I/tmp/nr-host-check /tmp/nr-host-check/plugin_common.c /tmp/nr-host-check/test_notify_checked.c -o /tmp/nr-host-check/test_notify_checked && /tmp/nr-host-check/test_notify_checked` — exit 0.
- C++17 standalone upload helper/test harness with `-fno-exceptions -fno-rtti`, warnings-as-errors, AddressSanitizer, and UndefinedBehaviorSanitizer — exit 0.
- SHA-256 vectors for empty input, short input, the 55/56-byte padding transition, 63/64/65-byte block transition, 119/120/127/128-byte transitions, and the published multi-block vector — PASS.
- Preview ordering, exact per-file byte sizes, exact-capacity success, one-byte-too-small rejection, duplicate filename rejection, and control-character filename rejection — PASS.
- Confirmation re-arming for same-count/same-size filename changes, content changes, file additions, and file removals — PASS.
- C11 stub harness for checked notification delivery — exit 0; accepted a complete message, rejected a message at/over notification-field capacity, and did not call the platform send stub for rejected input.

The standalone host harness does not replace the full CMake/CTest CI run or the PS4 SDK PRX build. No PS4 runtime, network upload, or hardware gate was exercised.

## Autonomous cycle on supplied checkout e68a6d9b2a7a2096ab3ad6e46b332a098569acf8

Date: 2026-11-23

### Checkout and PR context

- Exact checkout SHA supplied as `NR_BASE_SHA`: `e68a6d9b2a7a2096ab3ad6e46b332a098569acf8`.
- PR #63 is open, base `main` at `d0d261bd92f88e1e4e9c1d509db4d1d37093d84f`, head branch `engineering/gcn8-compute-readiness-audit` at `3a007ff856f1819308f20e06eaad70bfe68c7b6e`.
- The supplied changed-file context covers the Gate A workflow, host-test workflow changes, isolated PS4 PRX/read-only workflow change, CMake host contracts, GCN8 audit, compiler patch, parser/verifier, shader and fixture. No workflow file was edited in this cycle.
- Git and `.git` metadata are absent in this checkout, so the supplied SHA/context are treated as task input. No commit or Git diff is claimed.

### Files inspected

- `AGENTS.md`, `docs/GCN8_GNM_COMPUTE_READINESS.md`, `docs/PS4_WINDOWS_BRIDGE.md`, and this report.
- `.github/workflows/gcn8-compute-artifact-gate.yml`, `.github/workflows/host-tests.yml`, and `.github/workflows/ps4-prx.yml` (read only).
- `platform/ps4/goldhen/plugin/main.c`, `README.md`, `telemetry_bridge.{h,cpp}`, `github_client.cpp`, and upload-confirmation implementation/header.
- All checked-in `platform/ps4/gcn8/host` contracts and headers; `CMakeLists.txt`; the GCN8 parser/verifier, parser and ABI-patch tests, C++ GCN8 contract tests, smoke shader and binary fixture.
- Repository-wide search for GNM bind/dispatch, command submission, allocation, synchronization, cache maintenance, fences, and readback symbols in `platform/`, `src/`, and `include/`.

### Technical finding and changes

The upload-confirmation fix already present in this checkout addresses the identified count/total-size TOCTOU issue. `github_client.cpp` uses `pne::upload::Confirmation` snapshots and per-file digests, revalidates the captured set before upload, and checks the file digest again before sending its bytes. Authorization/disconnect paths invalidate the pending confirmation. `tests/test_upload_confirmation.cpp` provides host regression coverage. I found no regression requiring another change.

The GNM runtime audit confirms the highest-priority Gate B blocker: there is no checked-in compute runtime path to harden or extend safely. `platform/ps4/goldhen/plugin/main.c` hooks only `sceGnmSubmitAndFlipCommandBuffers` for present telemetry. The only compute parser/resource/known-answer code is under `platform/ps4/gcn8/host`; it validates metadata/contracts and does not submit GPU work. `telemetry_bridge.cpp` accepts an already CPU-readable RGBA pointer, while its header explicitly requires a completed readback/fence/cache-invalidation path supplied by a title adapter. No such adapter or allocation/residency, compute binding, command-buffer construction/submission, completion/recovery, or cache-coherent output path exists in the checkout. The pinned OpenGNM and shadPS4 discussion in the audit remains candidate/emulator evidence and does not prove the retail FW 14.02 ABI or behavior.

No runtime code change is justified by the checked-in authoritative evidence. The upload-integrity work was already present, and no C++ compiler is available in this host to validate a fresh host-side code slice. This cycle therefore changes only this report. This avoids adding guessed firmware bindings or treating host metadata validation as GPU behavior.

### Gate A reproduction and local tests

- The checked-in fixture independently parses with `tests/verify_gcn8_compute_sb.py`: 228 bytes; SHA-256 `8f52fb74771164498a0c1e90bd828d8517f6414d34a1d28c395c9aae2563b6e1`; compute stage; NEO target from the fixture contract; local size `[64, 1, 1]`; descriptor API slot 0 at register 2; `USER_SGPR` count 4. This is fixture metadata reproduction only, not the upstream compiler/CRC verification.
- Full independent Gate A reproduction was unavailable: the pinned compiler and sibling dependency trees are absent locally, as are `cmake`, `ctest`, `clang++`, `g++`, and `git`. No dependencies were fetched, and no remote workflow was run. Existing Gate A PASS evidence in the audit remains artifact-only; this cycle did not independently reproduce it.
- `python -m unittest tests/test_gcn8_compute_sb_parser.py` — exit 0; 7 tests passed.
- `python -m unittest tests/test_psbc_descriptor_pointer_patch.py` — exit 0; 2 tests passed.
- Python fixture parse/hash command — exit 0; values above.
- Full CMake/CTest suite and release-mode GCN8 targets — not run; CMake/CTest and a C++ compiler are unavailable.
- PS4 PRX build workflow — not run. Its inspected workflow installs host packages and downloads the OpenOrbis toolchain and other sources, which are outside this cycle's no-network/no-dependency-fetch constraints.

### Gate status

- **Gate A:** documented artifact-only PASS remains the repository's recorded status. No independent compiler reproduction in this cycle; fixture parser verification is not a replacement.
- **Gate B:** BLOCKED. Exact retail FW 14.02 exports/ABI, command-buffer ownership and submission, shader/resource allocation and residency, bounded completion/recovery, and cache-coherent CPU readback remain unproven; no runtime implementation exists.
- **Gate C:** NOT PASSED. No hardware dispatch, fence/completion, readback, or full known-answer comparison occurred.
- **Gate D:** NOT STARTED. It remains gated on Gate C and title-specific render-target/readback validation.

No PS4/LAN/FTP/RPI/KLog/BinLoader, deployment, secrets, package, PRX, plugin state, or user data were accessed or changed.

### Single next acceptance test

Independently reproduce Gate A from the exact pinned compiler/dependency revisions in an isolated host environment, run the upstream structural/CRC verifier plus `tests/verify_gcn8_compute_sb.py`, and compare the resulting shader binary SHA-256 and JSON report with the recorded artifact. This is the next host-verifiable acceptance test; it must remain artifact-only and cannot close Gates B, C, or D.

### Files changed in this cycle

- `docs/engineering/CODEX_AUTONOMOUS_CYCLE_REPORT.md` — appended this cycle's checkout, source audit, exact local test results, gate status, and next acceptance test. No source, test, CMake, workflow, or console-related file was changed.
