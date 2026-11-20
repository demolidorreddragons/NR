# Mandatory operating rules — Neural Engine

## Read and understand before every action

**Before taking any action in this repository or project, read this file from beginning to end and understand it.** Then read the applicable project instructions and workflow documentation before editing files, triggering workflows, deploying artifacts, or touching the PS4. Do not act first and consult the rules afterward.

If an instruction is unclear, resolve the ambiguity from the repository documentation and current project state before acting. If it cannot be resolved safely, stop only at that genuine blocker and ask one precise question.

## Execution discipline

- Do the requested work; do not replace execution with progress announcements, apologies, promises, or repeated requests for authorization already given.
- Do not send unsolicited status updates. Report only a concrete result, a genuine blocker requiring user input, or a status explicitly requested by the user.
- Never claim background work continues between messages. Never claim a build, test, deployment, validation, or hardware check succeeded unless its result was actually inspected.
- When the user says **Go** or otherwise authorizes continuation, continue the existing task from the actual repository and workflow state. Do not ask for another Go.
- Keep work moving through the next safe, deterministic steps. Do not stop merely to narrate that work is continuing.

## Engineering and QA integrity

- Separate engineering changes from QA review. QA audits and reports; Engineering addresses the findings; network/deployment work handles transfer and integrity.
- Do not turn an unverified hypothesis into a code change presented as a fix. Inspect the relevant source, logs, metadata, and authoritative documentation first.
- After a change, run the relevant tests and inspect their actual results. For package work, independently inspect package metadata, GP4 association, contents, hashes, and validation output.
- A base PKG is not a patch. Do not describe a candidate as installable or ready until it is a genuine associated patch and has passed package validation and QA. Hardware readiness requires actual console validation.
- Never invent workflow runs, artifacts, hashes, test results, or successful outcomes.

## Mandatory PS4 Windows bridge usage

- Before attempting any PS4 FTP/KLog/network task, consult [`docs/PS4_WINDOWS_BRIDGE.md`](docs/PS4_WINDOWS_BRIDGE.md) and use the configured self-hosted Windows runner labeled `[self-hosted, Windows, X64, ps4-lan]`.
- The bridge runner is the default route to the PS4 LAN. Do not ask the user to recreate the bridge, rerun PowerShell commands, or re-upload diagnostics when the runner and existing artifacts can perform the task.
- Do not attempt to substitute hosted GitHub runners or the hosted ChatGPT container for the Windows LAN runner.
- Inspect the workflow's actual capabilities before use. A read-only connectivity test is not a file-transfer workflow. Transfer requires explicit upload plus remote read-back/integrity verification.
- Use the successful build run's exact artifact, branch, and commit. Verify its SHA-256 before and after transfer; report success only when the remote digest matches.
- Keep package staging, package installation, PRX deployment, and PRX activation distinct. Do not install packages or activate PRX without the user's explicit authorization.
- Review scripts before running them on the privileged self-hosted runner; never execute untrusted third-party code there.

## PS4 safety boundaries

- Do not uninstall the app, alter saves or app data, activate a PRX, or edit `plugins.ini` without a clear technical reason and the user's approval.
- Do not deploy or execute uninspected third-party binaries on the privileged Windows runner that has access to the PS4 network.
- Preserve existing verified artifacts and deployment state unless a change is necessary, understood, and authorized.
- Keep PRX staging, activation, and package installation as separate operations. A file being present on the console does not mean it is activated or safe to load.

## Final reporting

Be concise and factual. State exactly what changed and cite the commit or workflow evidence when available. Clearly distinguish completed work, failed attempts, unverified candidates, and remaining hardware checks. Do not pad the response with status narration.

## Official Neural Rendering artwork — immutable project identity

The user's approved application artwork is the exact 1536×1536 RGB PNG supplied in the project conversation on 2026-11-20. It depicts a luminous monochrome silver/white smoke jellyfish/UFO on a black background, the PlayStation logo and `PS4 Pro` on the dome, and `NEURAL RENDERING` across the bottom. Its source SHA-256 is `3559eba63add88b4145982d99a6d696719e4a76bb4f3d08c26a02c89fc4fcbf5`.

This is the permanent, canonical Neural Rendering identity. Always reuse the exact original artwork; never substitute a generated image, simplified SVG, recreated logo, alternate composition, or merely similar image. The only permitted icon processing is proportional Lanczos resizing to 512×512 for `icon0.png`, preserving the full square composition. Canonical repository asset path: `assets/neural-rendering-icon-source.png`. A PKG must not be approved while this exact source asset is absent or its SHA-256 differs from the approved source.
