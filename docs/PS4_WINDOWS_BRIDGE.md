# PS4 Windows LAN bridge — mandatory deployment route

## Purpose
The repository has a self-hosted GitHub Actions runner on the user's Windows PC with access to the PS4 Pro LAN. This runner is the approved route for all live PS4 network operations from this project. Use it instead of attempting to reach the console from the hosted ChatGPT execution environment.

## Runner and connection

- GitHub runner labels: `self-hosted`, `Windows`, `X64`, `ps4-lan`.
- Runner identity observed in project operations: `demolidorreddragons-NR.PS4-WINDOWS-BRIDGE`.
- PS4 address must come from the repository Actions variable `PS4_IP`; do not hardcode a guessed address in workflows.
- GoldHEN FTP: TCP port `2121`, anonymous credentials, passive mode, binary mode for file transfers.
- The user's previously verified PS4 endpoint is `192.168.0.17:2121`; treat the Actions variable as the operational source of truth.

## Mandatory procedure

1. Before every live operation, read `AGENTS.md`, this document, and the exact workflow being used.
2. Inspect the current GitHub Actions workflow definitions and recent runs. Do not assume an old workflow is still enabled or that a connectivity-only workflow performs deployment.
3. For builds, use the actual successful build run and its artifact. Match the artifact to the intended branch/commit and inspect package metadata and SHA-256 before transfer.
4. Trigger the specific workflow that explicitly targets `runs-on: [self-hosted, Windows, X64, ps4-lan]`. A hosted Ubuntu runner cannot access the user's LAN. The control-plane preflight is read-only and runs after eligible successful builds. Package staging is a separate QA-bound workflow; do not assume that a build or preflight stages a PKG.
5. Use the runner to perform FTP operations. Use passive mode; the PS4 GoldHEN FTP server previously rejected EPSV, so use PASV/passive FTP.
6. For upload, write to the agreed staging path, then read the file back from the PS4 (or retrieve its remote size and SHA-256 through a trusted runner-side method). Compare the remote read-back SHA-256 with the build artifact SHA-256.
7. Report transfer as complete only after the workflow's upload and integrity-verification steps succeed and the logs identify the exact remote path, byte count, and matching digest.
8. Keep staging, activation, and installation separate. Uploading a PKG does not install it; uploading a PRX does not activate it.
9. For destructive cleanup, remove only explicitly identified Neural Rendering files. Preserve crash dumps, logs, unrelated packages, unrelated plugins, and unrelated GoldHEN configuration. Preserve existing plugin registrations unless a separately approved change is necessary.
10. Never run uninspected third-party code on this privileged self-hosted runner. Review workflow scripts and action references before triggering them.

## Workflow capability distinction

The legacy `.github/workflows/ps4-bridge-test.yml` was retired by PR #30 because it could stage stale PKG 006 before independent QA. It is no longer an active deployment path. The current `ps4-control-plane-preflight.yml` only performs read-only LIST/GETSIZE/GET and HTTP GET fingerprinting; it does not upload or install. The version-pinned staging workflow is separate, binds the exact build and QA evidence, and skips when a newer NR release is already staged.

A deployment workflow must:
- download the exact build artifact from the triggering successful workflow run;
- validate the expected PKG filename, size, and SHA-256 before network transfer;
- upload to the explicitly selected PS4 staging directory;
- read back and hash the remote file (or use an equivalently strong remote integrity check);
- fail closed on missing variables, unreachable FTP, size mismatch, or digest mismatch;
- emit machine-readable success markers with run ID, commit SHA, artifact name, remote path, byte count, and digest.

If no workflow currently implements all these steps, create or repair one before claiming the bridge can transfer packages. Do not ask the user to repeat LAN setup or collect the same diagnostics when the self-hosted runner is available.

## Read-only control-plane inventory

The preflight workflow runs `tools/ps4-control-plane-preflight.ps1` on the existing Windows LAN runner after an eligible successful build. It uses FTP LIST/GETSIZE/GET for narrowly allowlisted NR metadata and plugin paths, and HTTP GET-only fingerprinting on the RPI endpoint at TCP 12800. It never probes BinLoader 9090, sends payloads, issues POST, modifies remote files, installs packages, activates PRX, or restarts GoldHEN. It stores a timestamped JSON and raw evidence artifact for 30 days.

The workflow definition is on the default branch so GitHub's `workflow_run` trigger can observe eligible build runs. It checks out the reviewed default-branch collector, not code from the triggering build, and records the build's source SHA as metadata. The preflight result is only a read-only snapshot.

## Windows runner service troubleshooting

The runner service has previously failed to start with reported error 1068, although the registered runner connects when started manually with `run.cmd`. Preserve the working manual registration and credentials. Do not reinstall the runner, remove its service, modify registry values, change the logon identity, or stop the manual listener as a diagnostic shortcut.

Use `.github/workflows/ps4-windows-runner-service-diagnostics.yml` to collect read-only service state, service-control configuration, relevant event-log entries, runner directory ACLs, and filtered `_diag` excerpts. The workflow uses the existing `[self-hosted, Windows, X64, ps4-lan]` runner and retains a sanitized artifact for seven days. A repair may be considered only after the collected evidence identifies the specific failing dependency, account right, service SID, executable ACL, or runner service error. No automated repair is performed by the diagnostics workflow.

## Codex CLI installed and locally authenticated — 2026-11-23

The Codex CLI was installed on the existing Windows bridge runner host `DESKTOP-TG85GF4` (runner identity `PS4-WINDOWS-BRIDGE`) at the pinned version `0.171.0`. Executable:

`C:\\ProgramData\\NR\\Codex\\node_modules\\.bin\\codex.cmd`

The user completed device-code authentication. The terminal reported `Successfully logged in` / `Logged in using ChatGPT`. This confirms a local interactive-user session only; it does **not** prove that the GitHub Actions runner service identity can access the same Codex credentials. Do not copy, print, upload, or commit Codex authentication files/tokens, and do not expose device codes in Actions logs.

A local read-only execution from `C:\\Windows\\System32` initially failed because the default Windows process policy rejected the PowerShell child process. A one-off MXC sandbox probe succeeded:

`codex.cmd -c windows.sandbox=mxc sandbox --include-managed-config --permission-profile :workspace -- cmd.exe /d /c echo MXC_OK`

Observed output: `MXC_OK`, exit code `0`. A subsequent Codex read-only execution using `-c windows.sandbox=mxc` successfully ran `Get-Location`, returned `C:\\Windows\\System32`, and exited successfully. The test did not modify files. MXC was selected per invocation; no persistent Windows policy, registry, service, or system-directory patch was made by these tests.

**Operational boundary:** Codex CLI is installed and locally authenticated, and a basic sandboxed command execution has been demonstrated. Repository task execution, the runner service's access to the interactive login, and GitHub Actions integration remain unverified. Do not claim the Codex agent is operational in Actions until a separate non-mutating job proves authentication and execution under the actual runner service identity. Do not run Codex from `C:\\Windows\\System32`; set its working directory to the checked-out NR repository and use read-only sandboxing for the first repository inspection. Keep all initial inspection scoped to the repository. Any future write-capable Codex run must be explicitly scoped to NR project files, reviewed, and validated; no arbitrary system, PS4 deployment, installation, PRX activation, or restart operations are authorized by the CLI installation itself.

## Current project access baseline — user-confirmed, 2026-11-23

The following project prerequisites have been provided and must be treated as established. Do not ask the user to configure or provide them again unless new evidence shows a specific failure:

- **Windows runner:** the self-hosted Windows runner is connected to GitHub Actions.
- **Codex CLI:** Codex CLI is installed and authenticated in the user's Windows session.
- **Repository:** the Neural Rendering repository is accessible through the configured GitHub connection.
- **Console:** the PS4 Pro is available as the target for controlled project tests.

These statements record the user's supplied access/setup baseline. They do not, by themselves, assert that Codex credentials are available to the GitHub Actions service identity, that every workflow is currently healthy, or that a GPU compute test has passed. Before each operation, inspect the relevant workflow and verify its actual result. Preserve all existing PS4 safety gates and do not request repeated setup when the established access path can perform the task.


## Codex as the project engineering executor — required operating model (2026-11-23)

### Intended purpose

Codex is part of the Neural Rendering engineering pipeline, not an optional local demo or a tool whose output must be manually relayed by the user. Its assigned work is to operate on the checked-out NR repository, inspect the applicable instructions, implement isolated engineering changes, build, run tests, inspect failures, iterate, and produce reviewable commits and evidence. The project owner should not have to copy source files, run repeated diagnostic commands, or relay routine Codex output between steps.

The expected execution path is a **remote, non-interactive, repository-scoped job** with explicit credentials and bounded permissions. The interactive Windows login is not the production execution environment. The Windows PS4-LAN runner remains reserved for reviewed workflows that actually require access to the console network; ordinary Codex engineering should not inherit PS4 deployment privileges.

### Confirmed facts and failure

- Codex CLI 0.171.0 exists at `C:\ProgramData\NR\Codex\node_modules\.bin\codex.cmd` on `DESKTOP-TG85GF4`.
- Device-code authentication succeeded in the user's interactive Windows session.
- The GitHub Actions runner service runs as `LocalSystem`. The service job was tested and reported both `OPENAI_API_KEY_PRESENT=False` and `CODEX_API_KEY_PRESENT=False`, with Codex `Not logged in`.
- Therefore the interactive login is **not** available to the runner service. This is an observed integration failure, not a reason to ask the user to repeat Codex installation or device authentication.
- The repository checkout observed in the runner workspace was `C:\actions-runner\_work\NR\NR`, but that transient Actions checkout had no `.git` directory and `git` was not on PATH in the inspected environment. Codex must not assume that checkout is a persistent developer clone.
- The previous sandbox probe only proved that a simple MXC-isolated command could execute. It did not prove authenticated Codex task execution or engineering capability.

### Required target configuration

1. Run Codex as a **non-interactive GitHub Actions job** with a dedicated, least-privilege OpenAI API credential supplied through a protected GitHub Actions secret (for example, `OPENAI_API_KEY`). Never use or copy the interactive user's `auth.json`, device token, browser session, or any other personal credential into the service account.
2. Prefer an isolated hosted engineering runner for source analysis, code changes, builds, and host tests. Do not execute unreviewed compiler/dependency source on the privileged PS4-LAN runner. A separate reviewed Windows-LAN workflow may consume immutable, QA-approved artifacts when a hardware test is explicitly permitted.
3. Pin the Codex CLI version and install path in the job. Fail closed if the CLI version, authentication, repository root, or expected branch/commit is wrong. Do not fall back to an interactive login or silently use another account.
4. Check out the exact target branch and commit with Git metadata available. Set the working directory to the repository root; do not run from `C:\Windows\System32`. Verify `git rev-parse --show-toplevel`, branch, and HEAD before invoking Codex.
5. Invoke Codex non-interactively with an explicit repository task prompt and a bounded sandbox. Start with read-only audit. Enable workspace writes only for a reviewed engineering task. Never grant arbitrary host/system access, PS4 network/deployment access, or permission to install packages, activate PRX, modify `plugins.ini`, or restart GoldHEN.
6. The prompt must require Codex to read `AGENTS.md` fully, then the relevant project docs and workflow, preserve the existing gates, and make no unsupported ABI/GPU claims. It must not bypass tests or lower a gate to manufacture a pass.
7. Preserve complete stdout/stderr, exit code, changed-file list, diff, commit SHA, test summaries, and generated artifacts as a run-scoped Actions artifact. Fail the job on authentication failure, uncommitted unexpected changes, test failure, or missing evidence. Never print credentials or secret-bearing environment values.
8. Engineering and QA remain separate: Codex may implement and run the prescribed tests; a distinct QA pass must inspect the diff and evidence. Hardware Gate C still requires actual PS4 Pro FW 14.02 dispatch, completion/fence, coherent readback, and full known-answer comparison. Gate D and PKG release remain blocked until their documented criteria pass.

### Configuration status and explicit blocker

**Status: NOT YET CONFIGURED / NOT YET EXECUTED AS A REMOTE CODEX AGENT.** The CLI installation and interactive authentication are not sufficient. The available GitHub integration can read and write repository files and inspect workflow runs, but it does not expose an Actions workflow-dispatch operation or GitHub Actions secret-management operation. No protected API credential is currently evidenced as configured for a remote job. Consequently this session cannot truthfully create the secret, dispatch a remote Codex job, or claim that Codex has been configured and run.

Do not work around this by embedding an API key in workflow YAML, committing a token, copying the interactive `auth.json`, running as the user's interactive desktop, or granting the privileged PS4-LAN runner unrestricted Codex access. The remaining setup dependency is a protected Actions secret plus an approved workflow execution path. Once available, prove it with a read-only Codex repository audit first; only then enable scoped workspace-write engineering runs.

### Acceptance criteria

Codex integration is considered operational only when a remote Actions run demonstrates all of the following: (a) the expected runner class and repository commit, (b) authenticated non-interactive Codex execution, (c) full `AGENTS.md` compliance, (d) a read-only repository audit artifact, (e) a recorded exit code and logs, and (f) no PS4 mutation. Engineering-write mode is a separate acceptance step requiring a reviewed diff and passing relevant tests. A successful CLI version check, interactive login, sandbox echo, shader artifact build, or host-only test does not satisfy this integration gate.
