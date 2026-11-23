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
4. Dispatch or trigger a workflow that explicitly targets `runs-on: [self-hosted, Windows, X64, ps4-lan]`. A hosted Ubuntu runner cannot access the user's LAN.
5. Use the runner to perform FTP operations. Use passive mode; the PS4 GoldHEN FTP server previously rejected EPSV, so use PASV/passive FTP.
6. For upload, write to the agreed staging path, then read the file back from the PS4 (or retrieve its remote size and SHA-256 through a trusted runner-side method). Compare the remote read-back SHA-256 with the build artifact SHA-256.
7. Report transfer as complete only after the workflow's upload and integrity-verification steps succeed and the logs identify the exact remote path, byte count, and matching digest.
8. Keep staging, activation, and installation separate. Uploading a PKG does not install it; uploading a PRX does not activate it.
9. For destructive cleanup, remove only explicitly identified Neural Rendering files. Preserve crash dumps, logs, unrelated packages, unrelated plugins, and unrelated GoldHEN configuration. Preserve existing plugin registrations unless a separately approved change is necessary.
10. Never run uninspected third-party code on this privileged self-hosted runner. Review workflow scripts and action references before triggering them.

## Workflow capability distinction

`.github/workflows/ps4-bridge-test.yml` is a connectivity/inspection workflow unless its current contents explicitly include upload and read-back verification. It must not be described as a deployment workflow merely because it can list the PS4 FTP directories.

A deployment workflow must:
- download the exact build artifact from the triggering successful workflow run;
- validate the expected PKG filename, size, and SHA-256 before network transfer;
- upload to the explicitly selected PS4 staging directory;
- read back and hash the remote file (or use an equivalently strong remote integrity check);
- fail closed on missing variables, unreachable FTP, size mismatch, or digest mismatch;
- emit machine-readable success markers with run ID, commit SHA, artifact name, remote path, byte count, and digest.

If no workflow currently implements all these steps, create or repair one before claiming the bridge can transfer packages. Do not ask the user to repeat LAN setup or collect the same diagnostics when the self-hosted runner is available.

## Current verified installation state — 2026-11-23

The PS4 Windows LAN bridge is **already installed and configured**. This is established project infrastructure, not a setup task to repeat with the user.

- Runner identity: `demolidorreddragons-NR.PS4-WINDOWS-BRIDGE`.
- Labels: `self-hosted`, `Windows`, `X64`, `ps4-lan`.
- The runner can reach the PS4 GoldHEN FTP endpoint using `PS4_IP` from Actions variables and TCP port `2121`.
- A successful live transfer/integrity verification is recorded in bridge workflow run `45151203751` (job `ps4-connectivity`): PKG 006 was uploaded, read back, SHA-256 checked, and promoted to `/data/pkg/PS4-Neural-Engine-006-eb68c632848b.pkg`.
- Verified transfer evidence for that historical PKG 006: 6,619,136 bytes; SHA-256 `eb68c632848b6072213459d25b74da239d1535eaaf7b1af2314c732a8002bbff`. This is evidence of bridge transfer only, not installation or app execution, and must not be reused as the hash of later builds.

### Operating rule: reuse, do not recreate

- Never ask the user to reinstall, reconfigure, re-register, or manually retest the bridge merely because a new development task begins.
- Before each use, inspect current runner/workflow status and the exact workflow implementation. If a workflow is skipped or unavailable, diagnose its trigger/branch/permissions and repair the repository workflow where possible; do not send the user back through bridge setup.
- Prefer the existing self-hosted runner for LAN transfer and read-only evidence collection. Hosted runners and the ChatGPT container are not substitutes for the user's LAN bridge.
- Treat the following capabilities separately and verify each from workflow code and run logs: (a) FTP connectivity/listing, (b) upload, (c) remote read-back and digest verification, (d) staging/promotion, (e) package installation, (f) app launch/termination, (g) screen/process/crash observation, and (h) log collection. Success at an earlier item does not prove a later item.
- Current demonstrated capability includes FTP upload, read-back size/SHA-256 verification, and promotion into `/data/pkg/`. Do not claim that remote installation, app launch/close, screenshot capture, or runtime crash detection is automated unless a specific implemented workflow and successful run prove it.
- Continue to preserve the existing authorization boundaries: read-only collection and verified staging are distinct from installation, PRX activation, GoldHEN restart, configuration changes, or destructive operations. Those sensitive actions require explicit, specific authorization.


## Control-plane automation — implementation gates (2026-11-23)

The user has explicitly authorized, for the Neural Rendering project scope, automated installation of approved NR PKGs, deployment/activation of the NR PRX, and GoldHEN restart. This supersedes the older manual-install handoff and the prior requirement to obtain a second per-operation authorization for these named NR operations. It does **not** authorize uninstalling unrelated software, touching saves/user data, deleting logs, or executing unreviewed payloads.

### Discovered GoldHEN services and constraints

GoldHEN's upstream project documents:
- FTP on TCP 2121;
- BinLoader on TCP 9090;
- Klog on TCP 3232;
- internal PKG installation from `/data/pkg`;
- plugin support and remote package installation.

Upstream explicitly warns that BinLoader is experimental and payloads may crash the console. Therefore, an open 9090 port is only service reachability, not proof of a compatible/validated payload protocol, safe execution, firmware support, or an install/restart API. Do not send a payload as a connectivity test. The user's recorded FW 14.02 must be matched against the exact installed GoldHEN build and supported firmware before selecting a control adapter.

### Required control-plane stages

1. **Discovery (read-only):** probe 2121/9090/3232 from the existing Windows runner; record timestamp, target, TCP result, runner identity, triggering build run/commit. Read available GoldHEN version/configuration evidence without changing it.
2. **Protocol qualification:** pin the exact GoldHEN release/source commit and firmware compatibility; document the wire protocol and response framing for remote PKG install, BinLoader payload delivery, plugin activation, and restart. A TCP connect alone never qualifies a protocol.
3. **Signed/hashed operation manifest:** accept only artifacts from the triggering successful build, matching repository, branch allowlist, commit, package metadata, QA result, SHA-256, and explicit NR target allowlist. Reject arbitrary paths, URLs, payloads, or commands.
4. **PKG installation adapter:** use a documented GoldHEN remote-package-install mechanism or a reviewed NR-owned helper; verify install result and installed Title ID/version. Do not uninstall existing content automatically; detect conflicts and stop safely.
5. **PRX adapter:** deploy to the documented GoldHEN plugin directory; back up the exact existing NR registration/configuration, make a minimal reversible change, verify bytes/hash and registration. Do not activate the current inert scaffold merely by bypassing its title-adapter gate.
6. **GoldHEN restart/recovery:** use only a firmware-qualified, documented mechanism; record pre/post state and ensure a bounded recovery path. Never infer restart from a dropped TCP connection.
7. **Runtime observation:** collect Klog and existing app/system evidence read-only; correlate timestamps and build/PKG/PRX hashes; preserve originals and create a CDA manifest. Separate E0-E4.
8. **Rollback:** only reverse changes made by the same NR transaction; preserve unrelated plugins/configuration/data. If state is ambiguous, stop without cleanup.
9. **Gates:** QA and package integrity must pass before install; install verification before PRX activation; plugin registration verification before restart; restart verification before runtime test; E0-E3 evidence before functional conclusions. Each stage emits machine-readable JSON and GitHub step summary.

### Current status

The existing bridge proves FTP upload/readback/staging only. A new read-only control-plane preflight workflow probes 9090 and 3232 as well as 2121. Until a successful run and exact GoldHEN version/firmware protocol qualification exist, install/activation/restart remain **authorized but technically unimplemented**. Never equate port reachability with execution capability.
