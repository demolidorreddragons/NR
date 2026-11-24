# Autonomous engineering task — Neural Rendering

## Mission
Advance the Neural Rendering implementation from the exact checked-out repository state. Work as the engineering agent, not as a status reporter. Inspect the current code, open PR #63 context, existing Gate A evidence, and the GNM compute/runtime implementation. Make the strongest technically justified, testable progress possible in this run.

## Mandatory operating constraints
- Read all of AGENTS.md before acting, then the applicable docs and source files.
- Work only in this checkout and this feature branch.
- Do not access the PS4, LAN, FTP, KLog, RPI, BinLoader, or any console endpoint.
- Do not install packages, stage or deploy PKGs, alter PRX/plugin state, restart GoldHEN, or change user data.
- Do not fetch remote dependencies or browse the network. Use only checked-out source and already-present local dependencies.
- Never invent firmware 14.02 ABI facts, GNM exports, descriptor layouts, GPU addresses, synchronization semantics, or hardware results.
- Never claim Gate B/C/D or hardware validation from host tests, emulator evidence, shader compilation, or static analysis.
- Preserve existing user data, existing PS4 artifacts and all unrelated files.
- Do not make broad refactors. Prefer one minimal, evidence-backed engineering slice with regression tests.
- Do not commit; the workflow handles the commit after validation.

## Required work sequence
1. Read AGENTS.md completely. The exact checkout commit is supplied as NR_BASE_SHA. Inspect the current branch snapshot, the injected GitHub API metadata/changed-file patches for PR #63, Gate A workflow/evidence, all relevant GCN8 host/runtime source, tests, and existing docs. Do not claim the base SHA or PR metadata are unavailable when they are supplied.
2. Establish the actual highest-priority blocker for Gate B from repository evidence. Do not assume a file or API exists; search the checkout.
3. Implement one concrete improvement only if its behavior is supported by checked-in source/specification and can be tested locally. Suitable work includes ABI-neutral host-side validation, fail-closed checks, deterministic fixtures, regression tests, or removing a demonstrably incorrect assumption. Do not create fake GPU dispatch or fake firmware bindings.
4. If no safe runtime implementation can be justified, do not guess. Instead produce a precise source-audit report identifying exact files/symbols, proven vs unproven contracts, missing authoritative evidence, and the smallest next implementation/test slice. This is useful engineering output, not a claim of Gate B completion.
5. Run the most relevant existing tests and add regression tests for any code change. Record exact commands and exit codes.
6. Re-read your diff. Ensure no console/deployment paths or unrelated changes were touched.
7. Write/update docs/engineering/CODEX_AUTONOMOUS_CYCLE_REPORT.md with: exact commit base SHA from NR_BASE_SHA, PR #63 head/base and changed-file context, files inspected, files changed, technical finding, changes, tests/exit codes, gate status (A/B/C/D separately), unresolved blockers, and exact next step.

## Completion criteria
- Leave a coherent, reviewable working tree.
- Do not commit.
- Report concrete file paths and test evidence in the report.
- If blocked, document why with source evidence rather than fabricate a fix.


## Immediate implementation priority — upload confirmation integrity
The latest source inspection found a concrete TOCTOU weakness in `platform/ps4/goldhen/plugin/github_client.cpp`: the two-press upload confirmation is bound only to `count` and aggregate `bytes` (`g_pending_count`, `g_pending_bytes`). A different set of files with the same count and total size can therefore inherit the prior confirmation. Investigate this exact path and its existing tests/build constraints. If the source confirms the issue, implement a minimal fail-closed confirmation snapshot bound to the exact eligible file set (stable relative names and file identity/content digest or an equally strong deterministic fingerprint), revalidate that snapshot immediately before upload, invalidate it on disconnect/authorization changes, and add host-testable regression coverage without exposing credentials or touching the PS4. Do not add dependencies or guess unavailable PS4 APIs. If the existing architecture prevents a safe testable fix, document the precise reason and implement the smallest safe alternative. Then run the relevant tests and the PRX compile workflow only if its existing workflow is isolated and does not access the console. Do not claim hardware functionality.


## User-directed continuation — close every gate that can be evidenced

The user has explicitly directed us to continue through the gates. Work autonomously and do not stop for another authorization.

- Re-read AGENTS.md and all applicable gate/build documentation before acting.
- Gate A: independently reproduce the pinned GCN8 artifact verification if the checked-out dependencies/artifact permit it. Keep artifact-only scope explicit.
- Complete host-side C++ validation for the upload-confirmation change and existing GCN8 contracts using available local tools; record exact commands/results. Do not claim a host test proves PS4 behavior.
- Implement the upload-confirmation integrity fix and regression test if it is not already present in this checkout. Revalidate exact file-set identity immediately before bytes are sent; fail closed on changes.
- Audit Gates B, C, and D against exact checked-in code and pinned-source evidence. Do not invent firmware 14.02 ABI, allocation/residency, submission, synchronization, or cache-coherency contracts. If any gate cannot be closed safely, document the exact missing evidence and a precise acceptance test. Do not mark it passed.
- Do not edit any file under .github/workflows in this cycle. The workflow's GitHub Contents writeback token does not have sufficient permission for workflow-file writes. Do not modify task/workflow automation.
- Do not access PS4/LAN/FTP/RPI/KLog/BinLoader, deploy, stage, install, activate, or restart anything. This cycle is host-only.
- Keep changes limited to source, CMake, tests, and engineering docs. Do not commit; the workflow performs writeback.
- The runner has no Git executable. Ensure the report names every changed file and the workflow artifact captures a complete copy of changed files if possible; never claim a diff exists if only a report is captured.

## Next autonomous engineering cycle — GNM runtime path and independent Gate A reproduction

The upload-confirmation slice is implemented and host-tests CI now passes on the current branch. Do not spend another cycle repeating that work unless a regression is found.

### Required work
1. Read AGENTS.md, docs/GCN8_GNM_COMPUTE_READINESS.md, docs/PS4_WINDOWS_BRIDGE.md, the current engineering report, and the complete relevant GNM/PRX build workflows before acting.
2. Independently reproduce Gate A from the pinned source/dependency revisions and inspect the resulting shader binary, report, and SHA-256. The PR change in docs/GCN8_GNM_COMPUTE_READINESS.md intentionally triggers the isolated GCN8 artifact workflow. Record its exact run/artifact/hash only after verifying the completed outputs. Gate A remains artifact-only.
3. Audit the current branch's complete GNM compute path and all checked-in evidence. Inspect the exact source and symbols for shader binding, descriptor-table pointer setup, GPU allocation/residency, command buffer construction, submission, completion, cache maintenance, and CPU readback. Compare the pinned OpenGNM and shadPS4 source evidence already referenced in the audit; do not promote emulator or generic-backend behavior into retail firmware facts.
4. Identify the smallest real implementation slice that is supported by authoritative checked-in ABI/source evidence. If a safe, isolated component can be implemented without guessing retail firmware 14.02 ABI, implement it and add executable tests. Do not add speculative firmware prototypes, queue setup, GPU addresses, PM4 packets, or live-PRX dispatch calls.
5. Build the complete host CMake/CTest suite and release-mode GCN8 tests on an available host. Build the PS4 PRX only through the existing isolated build workflow if its trigger and exact checkout can be used without touching the console. Inspect all results.
6. Inspect the current PS4 bridge/read-only evidence only if a documented read-only workflow can resolve a concrete GNM/runtime prerequisite. Do not stage, install, activate, restart, or mutate the console in this cycle; preserve the existing app, saves, plugins.ini, PRX, and backups.
7. Update the report with exact files/symbols inspected, concrete code changes, commands/exit codes, Gate A-D status, and the single next acceptance test. Do not write a report-only change if a testable code improvement is available.

### Non-negotiable
- Do not stop after a progress summary. Complete all independent host-verifiable work in the cycle.
- Do not mark Gate B/C/D passed without retail PS4 Pro FW 14.02 evidence.
- Never fabricate a Sony ABI contract. If the checked-in evidence cannot prove it, keep runtime code isolated and fail closed.
- No changes under .github/workflows. The GCN8 workflow is triggered by the docs change already committed; do not modify workflow automation.
- Do not commit from Codex; the workflow owns writeback.
