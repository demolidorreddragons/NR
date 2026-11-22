# CDA initial audit — diagnostic collection pipeline

**Date:** 2026-11-22  
**Branch:** `feature/gcn8-numeric-kernels`  
**Scope:** existing PKG 014 evidence workflows, legacy Windows collector, and GCN8 stage logger  
**Disposition:** methodology deficiencies confirmed; no console operation performed.

## Executive finding

The previous pipeline was not wholly corrupted: the PKG 014 workflow successfully retrieved files and calculated SHA-256 hashes for the local copies, and the installed `app.pkg` hash matched the PKG 014 build. However, the pipeline did not establish complete runtime observability, did not decode the binary system log, and did not prove that the application launched or produced a specific notification. Therefore it was insufficient for root-cause diagnosis.

No evidence reviewed here proves FTP transport corruption. The demonstrated issue is diagnostic coverage and interpretation, not a proven transport-integrity defect.

## 1. Reviewed sources

- `.github/workflows/ps4-current-state-evidence-014.yml`
- `.github/workflows/ps4-logbin-readonly-analysis-014.yml`
- `.github/workflows/ps4-readonly-diagnostics.yml` (legacy workflow definition; not executed)
- `tools/windows/PS4-Diagnostics.ps1`
- `tools/windows/README-PS4-Diagnostics.md`
- `platform/ps4/gcn8/stage_log.c`
- `platform/ps4/gcn8/stage_log.h`
- `tests/test_stage_log.c`

This was a source review only. No runner workflow, PS4 FTP operation, build, test, installation, or runtime action was executed as part of this audit.

## 2. Findings

### CDA-001 — binary system log is not decoded

**Severity:** High  
**Evidence:** `ps4-logbin-readonly-analysis-014.yml` reads `log.bin` as ASCII and UTF-16LE, then searches printable strings.

This is exploratory string extraction, not a format parser. It cannot reliably identify record boundaries, event types, timestamps, truncation, compression, or binary payloads. A zero-match result cannot support a conclusion that no relevant event exists.

**Disposition:** Any such output must be labeled exploratory. Until a format/version is identified and a bounds-checked parser is validated against known samples, report `OPAQUE_UNDECODED`.

### CDA-002 — release-specific evidence workflow is hard-coded to PKG 014

**Severity:** High  
**Evidence:** `ps4-current-state-evidence-014.yml` hard-codes PKG filename, APP_VER 01.09, digest, build run and commit; it collects PKG 014 paths.

The workflow cannot establish PKG 015 identity without an explicit reviewed update. Running it for PKG 015 would create misleading provenance.

**Disposition:** Do not reuse for PKG 015. After the user reports manual installation/execution, create or update a PKG 015-specific read-only workflow and inspect it before triggering.

### CDA-003 — acquisition does not bracket mutable remote files

**Severity:** Medium  
**Evidence:** the workflow issues GETSIZE and then GET, recording the local copy's size/hash. It does not capture a remote pre/post metadata snapshot or retry when a mutable file changes during download.

For a live/rotating file such as `log.bin`, the local hash accurately identifies the bytes received, but does not prove that the transfer represents one stable remote snapshot.

**Disposition:** For mutable sources, collect independent timestamped snapshots and compare their local hashes/sizes. Record the possibility of concurrent writes. Never call changing hashes corruption without further evidence.

### CDA-004 — flattened local filenames can lose path identity

**Severity:** Medium  
**Evidence:** remote paths are converted to local filenames by replacing path separators with underscores.

This can create collisions if two remote paths normalize to the same local name and weakens provenance when reviewing artifacts.

**Disposition:** Future collector must preserve remote path identity using a directory hierarchy or collision-resistant path encoding, and include the original remote path in every manifest entry.

### CDA-005 — legacy collector can overwrite local evidence and is not provenance-complete

**Severity:** High  
**Evidence:** `tools/windows/PS4-Diagnostics.ps1` uses a fixed default host, repeats collection into the same local paths, uses `Move-Item -Force`, and does not create a per-run hash manifest. It parses FTP LIST entries by taking the last whitespace-delimited token, which is unsafe for names containing spaces.

This script is not an acceptable authoritative evidence collector. Its default infinite polling behavior also differs from the one-shot, run-scoped workflow model.

**Disposition:** Do not use it for official evidence. Keep it out of the release/diagnostic path until it is redesigned with unique run directories, no-overwrite semantics, exact remote paths, hashes, timestamps, robust listing handling, and explicit one-shot behavior. No code change was made in this audit.

### CDA-006 — application crash context layout is not proven by host tests

**Severity:** High / requires ABI evidence  
**Evidence:** `stage_log.c` reads selected words and a 48-word prefix from the signal context pointer. The source comments acknowledge that OpenOrbis and PS4 kernel context layouts differ and intentionally preserve raw words. Host tests validate behavior on the host ABI, not the retail PS4 FW 14.02 signal-frame layout or the validity/extent of that context pointer.

**Disposition:** Do not label the emitted words as RIP/RSP/registers or rely on fixed word offsets until the PS4 ABI/layout and accessible bounds are supported by authoritative evidence or a safe target-specific validation. Keep raw context words explicitly opaque. Do not trigger a crash to validate this on the user's console without a separate approved test plan.

### CDA-007 — normal logger flush policy may perturb timing

**Severity:** Medium  
**Evidence:** `emit_normal` calls `fsync` after every normal record. This may add latency and alter timing of the code being diagnosed.

**Disposition:** Do not change this blindly. Measure on a safe host harness first, define required durability points, and evaluate target-specific behavior before changing the logger. Any change must preserve error reporting and short-write/EINTR handling.

### CDA-008 — no proven application execution event in PKG 014 evidence

**Severity:** Critical diagnostic gap  
**Evidence:** the PKG 014 audit established installed package identity but did not establish app launch or the notification result; no app-specific persistent log was found.

**Disposition:** Do not infer a runtime crash, success, or failure from package presence or empty coredump directory. User feedback and a future execution-correlated diagnostic record are required.

## 3. Positive controls observed

- The PKG 014 collector used the authorized Windows self-hosted runner.
- Its remote operations were LIST/GETSIZE/GET only and the manifest declared no remote modification.
- It calculated SHA-256 for downloaded local files and preserved a workflow artifact.
- The installed `/user/app/PNEU00001/app.pkg` snapshot matched the PKG 014 raw-file SHA-256.
- The collection explicitly recorded unavailable app-log paths and an empty coredump listing rather than fabricating content.

These controls remain useful, but they do not close the diagnostic gaps above.

## 4. Required remediation order

1. Do not run the legacy polling collector as an authoritative source.
2. Wait for the user's PKG 015 install/run feedback; do not install or trigger a new console test.
3. Prepare a PKG 015-specific read-only collector with unique run-scoped output, exact path preservation, explicit acquisition status, timestamps, sizes and hashes, and no remote writes.
4. Preserve raw `log.bin` snapshots. Do not claim to decode them until a supported format/parser is established.
5. Correlate the user's exact visible result with installed PKG hash, app metadata, coredumps and app/system logs.
6. Audit and improve application observability only after source review and a safe test plan; do not use speculative signal-context offsets.
7. Require CDA gate review before any corrective PKG is authorized.

## 5. Audit conclusion

**Current diagnostic capability: PARTIAL — package identity and file acquisition are demonstrated; runtime cause attribution is not.**

**Transport corruption: NOT ESTABLISHED.**  
**PKG 015 runtime result: NOT YET OBSERVED.**  
**Root cause of recurring error: UNKNOWN / INSUFFICIENT EVIDENCE.**  
**Independent human review: NOT PERFORMED.**

This audit changes the project's diagnostic standard. It does not claim a fix to the PS4 application or a validation of PKG 015 runtime.
