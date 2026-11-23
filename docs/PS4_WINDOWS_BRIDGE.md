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

