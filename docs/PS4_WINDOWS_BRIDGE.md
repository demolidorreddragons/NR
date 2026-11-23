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
4. Dispatch or trigger a workflow that explicitly targets `runs-on: [self-hosted, Windows, X64, ps4-lan]`. A hosted Ubuntu runner cannot access the user's LAN. The bridge workflow is configured to process successful builds from `main` and `feature/gcn8-numeric-kernels`, downloading the artifact by the triggering build run ID.
5. Use the runner to perform FTP operations. Use passive mode; the PS4 GoldHEN FTP server previously rejected EPSV, so use PASV/passive FTP.
6. For upload, write to the agreed staging path, then read the file back from the PS4 (or retrieve its remote size and SHA-256 through a trusted runner-side method). Compare the remote read-back SHA-256 with the build artifact SHA-256.
7. Report transfer as complete only after the workflow's upload and integrity-verification steps succeed and the logs identify the exact remote path, byte count, and matching digest.
8. Keep staging, activation, and installation separate. Uploading a PKG does not install it; uploading a PRX does not activate it.
9. For destructive cleanup, remove only explicitly identified Neural Rendering files. Preserve crash dumps, logs, unrelated packages, unrelated plugins, and unrelated GoldHEN configuration. Preserve existing plugin registrations unless a separately approved change is necessary.
10. Never run uninspected third-party code on this privileged self-hosted runner. Review workflow scripts and action references before triggering them.

## Workflow capability distinction

`.github/workflows/ps4-bridge-test.yml` performs read-only pre-stage diagnostics and then stages the exact successful build artifact under `/data/pkg/` with remote SHA-256 read-back verification. Its diagnostics are explicitly labelled `pre-stage`: the installer log on the console describes the last eboot launch, not the candidate just uploaded. A fresh post-install log can only be attributed to the new candidate after the user installs and launches it.

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
