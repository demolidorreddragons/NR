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

The user's approved application artwork is the exact square image supplied in the project conversation on 2026-11-20 (original upload: 1536×1536 JPEG, SHA-256 `3559eba63add88b4145982d99a6d696719e4a76bb4f3d08c26a02c89fc4fcbf5`). It depicts a monochrome silver/white smoke jellyfish/UFO on a black background, the PlayStation logo and `PS4 Pro` on the dome, and `NEURAL RENDERING` across the bottom. The user explicitly confirmed the 512×512 Lanczos preview as the exact approved app icon. That approved PNG preview has SHA-256 `7fe6a11ad7b4d37e5741c77822730a82487237dbed4a3eca529e995a29421ae4`.

This is the permanent, canonical Neural Rendering identity. Always reuse the exact user-supplied artwork and the confirmed 512×512 PNG icon; never substitute a generated image, simplified SVG, recreated logo, alternate composition, color treatment, or merely similar image. Do not apply any further transformations to the confirmed 512×512 icon. Canonical repository asset path: `assets/neural-rendering-icon-source.png`. A PKG must not be approved while this exact approved PNG is absent or its SHA-256 differs from `7fe6a11ad7b4d37e5741c77822730a82487237dbed4a3eca529e995a29421ae4`.

## Mandatory PKG installation handoff

After an approved PKG has been staged in the correct PS4 folder and its final readback size/SHA-256 has been verified, explicitly notify the user that it is ready for installation and provide the manual PS4 installation procedure in that same message. Include the exact filename/path, size, SHA-256, QA result, whether it is a clean install or update, and any user-performed prerequisite (such as manually removing an older version). Do not install automatically: installation remains a separate operation requiring explicit, specific user authorization. Never give a 'ready to install' notice before verified staging. Follow docs/REGRAS_OPERACIONAIS_DO_PROJETO.md §2.1 for the complete handoff and procedure. If technical gates are not cleared, state that the diagnostic PKG must not be installed.

## Mandatory post-install feedback response

When the user reports the result after manually installing/running a staged PKG, immediately begin evidence collection and diagnosis using the authorized Windows PS4 bridge and existing workflows. Correlate logs/crash dumps/timestamps and PKG/build hashes with the exact commit and workflow; preserve originals and use read-only collection. Then analyze, identify root cause, implement a minimal safe fix, and run applicable tests. Ask only for a specific missing datum if the available evidence is insufficient. Do not stop at acknowledgement/status. Any new PS4 test or installation requires its own explicit authorization; follow docs/REGRAS_OPERACIONAIS_DO_PROJETO.md §2.2.


## Chief Diagnostic Architect (CDA) — mandatory diagnostic authority

- The project designates the **Chief Diagnostic Architect (CDA)** as the highest functional authority for diagnostic methodology, evidence integrity, causal analysis, and diagnostic release gates. This is a project role, not a claim that an external human or autonomous agent has been hired or independently reviewed the work.
- Before any diagnostic conclusion, corrective PKG, or new PS4 diagnostic workflow, follow `docs/DIAGNOSTIC_METHODOLOGY_CDA.md` and the mandatory operational rules in section 10.
- The CDA may block speculative fixes and releases when evidence integrity, build identity, or runtime observability is insufficient.
- Separate E0 package/build identity, E1 transfer integrity, E2 console snapshot, E3 process execution, and E4 functional result. A pass at one layer never implies a pass at another.
- Preserve raw evidence and manifests; analyze copies only. Never describe printable-string scanning as decoding a binary log. Unknown formats must be marked `OPAQUE_UNDECODED`.
- Never reuse a version-specific evidence workflow for another PKG without updating and reviewing the expected version, hashes, paths, and scope.
- Root cause must be classified as observed/correlated/inferred/hypothesis/unknown; use CONFIRMED ROOT CAUSE only when the evidence and a reproducible mechanism support it.
- Engineering, QA, and independent audit remain separate. Automated tests are not an independent human review.
- The CDA role does not grant permission to install/uninstall PKGs, activate PRX, modify `plugins.ini`, restart GoldHEN, or perform destructive console actions.


## Explicit NR console automation authorization — 2026-11-23

The user explicitly authorized, within the Neural Rendering project scope, automated installation of approved NR PKGs, NR PRX deployment/activation, GoldHEN restart, and the sensitive console operations required by that controlled cycle. This supersedes the older manual-install handoff and per-operation authorization language for those named NR actions. Do not ask for the same authorization again.

This authorization is not permission to uninstall unrelated software, modify saves/user data, delete evidence, execute arbitrary shell commands, or send unreviewed payloads. All operations remain gated by verified artifact identity, QA, firmware/GoldHEN compatibility, exact protocol qualification, reversible NR-only changes, and preserved CDA evidence.

Follow `docs/PS4_CONTROL_PLANE_AUTOMATION.md` and `docs/PS4_WINDOWS_BRIDGE.md`. The `.github/workflows/ps4-control-plane-preflight.yml` workflow is read-only: FTP LIST/GETSIZE/GET on allowlisted NR paths and HTTP GET-only RPI fingerprinting on TCP 12800. It never probes BinLoader 9090 or Klog 3232, sends payloads, or mutates the console. It does not install a PKG, send a BinLoader payload, activate a PRX, or restart GoldHEN. A reachable port is not proof of a usable or safe protocol.

Do not bypass the PRX title-adapter fail-closed gate. GoldHEN v2.4b18.14 / PS4 FW 14.02 compatibility is documented by the release author and reported by the user; do not keep treating this firmware pairing as unknown. The exact installed version remains USER_REPORTED until device evidence confirms it. Do not implement install/activation/restart messages until their protocol framing, success response, and recovery behavior are verified from pinned version-matched source/client and/or direct read-only device evidence.

## Control-plane discovery correction — 2026-11-23

The control-plane preflight is now a read-only inventory, not a generic port scan. It uses the existing Windows runner for FTP LIST/GETSIZE/GET against narrowly allowlisted NR paths and HTTP GET-only fingerprinting of the Remote Package Installer API on TCP 12800. It must never probe GoldHEN BinLoader TCP 9090, send a payload, issue POST requests, modify console files, install a PKG, activate a PRX, or restart GoldHEN. The runner script is `tools/ps4-control-plane-preflight.ps1`; inspect it before every change or execution. A successful preflight only proves the observed read-only paths and endpoint fingerprint.

The RPI client protocol has been source-reviewed against `AldySan/Remote-Package-Installer-NG` commit `f99e62089afa61fb418446bdeb12340afd951a17`. The protocol uses HTTP JSON APIs on the RPI endpoint (normally TCP 12800), including `GET /api`, `GET /api/status`, `POST /api/is_exists`, `POST /api/install`, and progress polling. Only GET endpoints are used by preflight. Actual installation additionally requires a compatible RPI receiver / GoldHEN Package Installer active in the foreground, a bounded HTTP Range-capable file server on the Windows runner, artifact/metadata/QA allowlisting, and verified task completion. Do not run POST install requests until all these conditions and the exact package manifest are validated.

The GamerHack web host is an exploit/loader distribution, not the GoldHEN source project. The patnawa mirror repository documents its bundled v2.4b18.14 archive SHA-256 `fe67d34f2a67b1f4f018629b51318bab40ba7ad13f2293ae802e6cb57ea202cf` and extracted payload SHA-256 `c5480587d99efc2e3cc132352d9b7cf052dd3068280f1686e7fbb37d57e9ba8b`. These identify the published distribution artifact, not the bytes installed in the user's console. The host repository also marks FW 14.02 offset data as not yet proven on hardware. Never infer successful jailbreak/GoldHEN loading on FW 14.02 from routing or host-page support alone.
