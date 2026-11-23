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
