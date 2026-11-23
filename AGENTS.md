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


## Explicit NR console automation authorization — 2026-11-23

The user explicitly authorized automated installation of approved Neural Rendering PKGs, NR PRX deployment/activation, GoldHEN restart, and the sensitive console operations necessary for that controlled NR cycle. Do not ask the user to repeat this authorization. It does not authorize unrelated software changes, saves/user-data modification, evidence deletion, or arbitrary/unreviewed payload execution. Technical gates, verified artifact identity, QA, exact protocol qualification, reversible NR-only changes, and CDA evidence remain mandatory.

The control-plane preflight is strictly read-only: FTP LIST/GETSIZE/GET on allowlisted NR paths and HTTP GET-only RPI API fingerprinting on TCP 12800. It must not probe GoldHEN BinLoader 9090, send payloads, issue HTTP POST, write/delete/rename console files, install PKGs, activate PRX, or restart GoldHEN. The script is `tools/ps4-control-plane-preflight.ps1`; inspect it before every change or execution.

The RPI protocol candidate was source-reviewed against `AldySan/Remote-Package-Installer-NG` commit `f99e62089afa61fb418446bdeb12340afd951a17`. Actual install requires a confirmed RPI receiver, GoldHEN Package Installer in the foreground, a Range-capable allowlist-only HTTP file server, an exact QA-approved artifact, and console-side terminal task evidence. The NR PRX remains fail-closed while its title adapter is unvalidated. No documented remote GoldHEN restart endpoint has been qualified; do not invent one.

The GamerHack host is the exploit/loader distribution; the patnawa mirror documents GoldHEN v2.4b18.14 archive SHA-256 `fe67d34f2a67b1f4f018629b51318bab40ba7ad13f2293ae802e6cb57ea202cf` and extracted payload SHA-256 `c5480587d99efc2e3cc132352d9b7cf052dd3068280f1686e7fbb37d57e9ba8b`. These are published distribution hashes, not a device readback. GamerHack's FW 14.02 offset route remains unproven on physical hardware.
