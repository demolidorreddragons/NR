# PS4 Control Plane Automation

**Repository:** demolidorreddragons/NR  
**Existing bridge:** demolidorreddragons-NR.PS4-WINDOWS-BRIDGE  
**Purpose:** complete the existing PS4 LAN bridge without duplicating setup.

## Authorization recorded

The user explicitly authorized automated installation of approved NR PKGs, deployment/activation of the NR PRX, GoldHEN restart, and sensitive console operations necessary for the NR development cycle. This authorization supersedes older manual-install instructions for these named NR operations. It does not extend to unrelated software, saves, user data, arbitrary commands, or unreviewed binaries.

## Verified service facts

GoldHEN upstream documents FTP TCP 2121, BinLoader TCP 9090, Klog TCP 3232, internal package installation from `/data/pkg`, plugin support, and remote package installation. Upstream warns BinLoader is experimental and may crash the console. These feature statements do not establish the exact protocol, firmware compatibility, or availability on the user's installed GoldHEN version. See [GoldHEN upstream README](https://github.com/GoldHEN/GoldHEN) and [release history](https://github.com/GoldHEN/GoldHEN/releases).

The user reports GoldHEN v2.4b18.14 on firmware 14.02, and the GamerHack distribution advertises this pairing. Installed binary identity remains unverified; keep the version as USER_REPORTED until device evidence confirms it.

## Automation components

| Component | Current evidence | Next acceptance criterion |
|---|---|---|
| Existing Windows LAN runner | Previously verified | Reuse; never reinstall/reconfigure |
| FTP transfer/readback/staging | Historical successful run 45151203751 | Continue per-artifact SHA-256 verification |
| FTP/RPI discovery | Read-only FTP inventory and HTTP GET-only fingerprint on 12800 | Successful runner artifact and device snapshot |
| Remote PKG install | Not yet implemented/verified | Exact GoldHEN protocol and installed Title ID/version readback |
| PRX deployment | FTP file transfer only | Hash verification and reversible NR-only registration |
| PRX activation | Not yet implemented/verified | Firmware-qualified mechanism and runtime evidence |
| GoldHEN restart | Not yet implemented/verified | Documented restart operation plus independent post-restart check |
| App launch/exit and crash observation | Not yet implemented/verified | Supported process/runtime telemetry |
| Klog and diagnostic collection | FTP read-only collection exists in part | Timestamped, hash-preserved CDA manifest |
| Rollback | Not yet implemented | Transaction-scoped reversible rollback preserving unrelated state |

## Transaction order

1. Build and QA; bind all outputs to repository, commit, workflow run and artifact digest.
2. Stage to a temporary path; read back; compare size and SHA-256; promote to `/data/pkg/`.
3. Install only the approved NR Full/Base PKG; verify installation and Title ID/version. Do not auto-uninstall an existing app.
4. Deploy the approved NR PRX; preserve a byte-for-byte backup of existing NR-owned registration; change only the NR entry; verify the resulting file and PRX hashes.
5. Activate only after the adapter's firmware/API qualification and the PRX's own safety gates pass. The current plugin's title adapter is deliberately fail-closed; do not bypass it.
6. Restart GoldHEN only through a documented, firmware-qualified method; verify the service returns and the intended plugin state is present.
7. Run the controlled NR test; collect Klog/app/system evidence read-only; preserve originals and produce CDA E0-E4 manifest.
8. If a stage fails, stop the transaction. Roll back only changes made by that transaction, never unrelated plugins, packages, logs, saves or user data.

## Read-only collector evidence integrity — 2026-11-23

The control-plane collector is deliberately checked out from the reviewed `main` branch before it runs on the privileged self-hosted Windows runner. The triggering build's branch, commit, and run ID are recorded as separate provenance fields; build-branch source code is not executed merely because its build succeeded. The collector itself is therefore identified by the workflow's checkout ref and repository commit, not conflated with the build commit.

FTP directory listings are retained as raw byte streams (`.raw.bin`) without StreamReader decoding or text re-encoding. Each file GET records UTC start/end, remote GETSIZE before and after, local byte count, and SHA-256. A size change is reported as a mutability/size mismatch, not as authenticated remote content. Partial downloads are preserved separately with their own size and digest. Local files are created exclusively to prevent accidental evidence overwrite.

A static CI contract checks the exact FTP read-only method allowlist (`LIST`, `GETSIZE`, `GET`), raw byte preservation, exclusive file creation, partial preservation, pre/post size checks, digest/timestamp fields, and HTTP GET-only RPI fingerprinting. This contract is source-level validation; it does not prove Windows PowerShell execution or a live PS4 collection.

## Required machine-readable evidence

Every operation must emit JSON with schema version, UTC timestamp, runner identity, build run ID, commit SHA, artifact name, expected and observed hashes, target path, operation, result, and error. Runtime records must add firmware, GoldHEN version, Title ID, process/test identity, and E0-E4 evidence references. Secrets and credentials must never be logged.

## Security gates

- Never execute a payload merely to test whether BinLoader is open.
- TCP reachability is not protocol validation.
- Pin and review exact GoldHEN source/release and protocol before implementing control messages.
- Do not accept arbitrary payload URLs, shell strings, paths, or user-supplied commands from workflow inputs.
- Only artifacts produced by the triggering successful build on allowlisted branches are eligible.
- No operation may touch saves, user data, unrelated packages/plugins, or unknown files.
- Fail closed on firmware mismatch, missing version evidence, hash mismatch, unexpected plugin config, ambiguous install state, or failed rollback.
- Keep engineering, QA and CDA evidence roles separate.


## Confirmed target version supplied by the user — 2026-11-23

The user reports that the installed console version is **GoldHEN v2.4b18.14**, paired with **PS4 firmware 14.02**. This exact GoldHEN build is the firmware-specific release that adds support for 14.02. The author SiSTRo's release listing states that v2.4b18.14 adds firmware 14.02 support and remains a beta; see [SiSTRo's v2.4b18.14 release listing](https://ko-fi.com/s/66eac96180). A separate host repository identifies the payload as v2.4b18.14 and publishes SHA-256 `c5480587d99efc2e3cc132352d9b7cf052dd3068280f1686e7fbb37d57e9ba8b`; that hash identifies that host's distributed payload, **not** a verified hash of the binary installed on this console ([host source](https://github.com/X-F1REBALL-X/ps4-jb)).

This resolves the version/firmware compatibility uncertainty at the release-documentation level. The version is user-reported and has not yet been independently read from the console.

### Consequences for implementation

- FTP 2121 is reachable. The read-only preflight's HTTP GET fingerprint to RPI TCP 12800 was refused. Do not probe BinLoader 9090 or Klog 3232.
- GoldHEN's BinLoader remains explicitly experimental. Do not use it as a generic shell or as a probe; any payload protocol/ABI must be separately pinned and reviewed before sending a payload.
- The release advertises Remote Package Install and internal `/data/pkg` installation, but the public release note alone does not specify a machine protocol, request framing, response semantics, or remote success verification. Those must be derived from the exact v2.4b18.14 implementation or a trusted, version-matched client before automating installation.
- Plugin support is documented, but that alone does not establish a safe remote PRX activation or GoldHEN restart command. Continue to require a version-matched documented mechanism and independent post-operation verification.
- The 14.02 compatibility gate can now be marked `RELEASE_COMPATIBILITY_DOCUMENTED`; live console identity, service reachability, protocol qualification, and operation success remain separate evidence states.


## Provenance correction — GamerHack build/host (2026-11-23)

The user clarifies that the GoldHEN v2.4b18.14 setup in use is the **GamerHack** version. Do not describe the user's installed distribution as a binary built or released by SiSTR0 unless its binary provenance is independently established.

Public host metadata identifies the PS4 host as “Original host by GamerHack - hosted by patnawa” and lists GoldHEN v2.4b18.14 with firmware support through 14.02: https://patnawa.github.io/GoldhenPS4/g2all/. This proves host attribution and the host's advertised firmware routing, not by itself who compiled or modified the embedded GoldHEN payload. The upstream GoldHEN GitHub release page lists v2.4b18 with an older supported-firmware matrix; do not conflate that upstream release with the GamerHack v2.4b18.14 distribution.

Operational classification:
- Distribution/host: `GAMERHACK_ORIGINAL_HOST` (as advertised by the host page).
- Host mirror: `PATNAWA_HOSTING`.
- Embedded payload build provenance: `UNVERIFIED` until binary hash/source/build metadata is inspected.
- Firmware 14.02 support: advertised by the GamerHack host page; independently verify live operation and the embedded payload before enabling privileged automation.

For automation, pin the exact host repository/commit and the actual GoldHEN payload bytes/hash. Do not fetch a replacement payload from an unrelated upstream release or assume same-version strings imply identical binaries.

## Source-qualified remote package installer path — 2026-11-23

A source-reviewed candidate exists for remote PKG transfer: the RPI HTTP JSON dialect implemented by `AldySan/Remote-Package-Installer-NG`, pinned for this review to commit `f99e62089afa61fb418446bdeb12340afd951a17` (GPL-3.0 project; protocol reimplemented from the flatz installer). Relevant source files: `crates/rpi-ng-core/src/rpi/client.rs`, `types.rs`, and `docs/ARCHITECTURE.md`.

Observed protocol contract in that pinned client:
- read-only identification: `GET /api` (legacy RPI fingerprint), `GET /api/status`, optional `GET /api/v2/system`;
- read-only install-state query: `POST /api/is_exists` with a Title ID;
- install submission: `POST /api/install` with `{"type":"direct","packages":["http://<runner-address>:<port>/<allowlisted-pkg>"]}`;
- task progress: `POST /api/get_task_progress` with `{"task_id":N}`; a task ID greater than zero tracks an install, and `-1` means already installed;
- NextGen v2 uses `POST /api/v2/install` and `GET /api/v2/jobs`.

Important qualification boundaries:
- The source client describes the RPI receiver dialect, not proof that this console currently exposes it. The actual receiver identity must be confirmed by the new read-only preflight at TCP 12800.
- The RPI-NG user guide requires GoldHEN Package Installer to be open in the foreground while transferring. The existing GitHub runner cannot currently open the PS4 system UI or guarantee foreground state.
- A real install adapter must provide an allowlist-only HTTP file server with Range support, verify package metadata and SHA-256, call the exact detected API, poll until console-side terminal evidence, and correlate installed Title ID/version. A successful HTTP submission is not install completion.
- No install POST is implemented in the current preflight. It sends only GET requests and records read-only FTP evidence.
- Do not fall back to BinLoader 9090. That is GoldHEN's experimental payload service and must not be probed or treated as an HTTP RPI endpoint.

## PRX activation and GoldHEN restart — remaining protocol gates

GoldHEN's plugin documentation establishes the plugin directory `/data/GoldHEN/plugins/` and registration file `/data/GoldHEN/plugins.ini`, with per-title sections recommended. This documents registration format, not a remote activation/reload API. The current NR PRX still has `has_validated_title_adapter() == false` and exits before installing hooks or threads. It must remain fail-closed; adding it to `[default]` would not make Neural Rendering functional and is not an approved activation step.

No version-matched, documented remote GoldHEN restart endpoint has been identified. A dropped FTP/RPI connection is not evidence of restart. Do not invent a restart command or send a BinLoader payload. Restart automation remains blocked until a documented, firmware-qualified mechanism and post-restart recovery check are identified.

## GamerHack distribution identity and hashes

The public host source is `GamerHack/GamerHack.github.io`; the preserved mirror is `patnawa/GoldhenPS4`. The mirror's README/CHANGELOG identifies GoldHEN v2.4b18.14 (298,776 bytes), archive SHA-256 `fe67d34f2a67b1f4f018629b51318bab40ba7ad13f2293ae802e6cb57ea202cf`, extracted payload SHA-256 `c5480587d99efc2e3cc132352d9b7cf052dd3068280f1686e7fbb37d57e9ba8b`. The latter is a published distribution hash, not a console readback hash. The same mirror explicitly says 14.02 offset routing is upstream data and remains unproven on physical hardware. Keep exploit-chain compatibility, GoldHEN payload identity, and live console execution as three separate evidence items.


## Live read-only preflight and legacy staging incident — 2026-11-23

The read-only control-plane preflight completed on the existing Windows bridge:
- Build context: run `45164006053`, commit `941caa50b08e2f3f65dee00901dd8e09aedb65a1`, branch `main`.
- Preflight run: `45164276080`; job `137657095936`; result `success`.
- Evidence artifact: `ps4-control-plane-preflight-45164006053`, artifact ID `14814021379`, ZIP digest `7bfa3fb5832d852e20ffdeba2110f55e5043545cb7779cce3b7ccdbc702477f2`.
- FTP LIST/GETSIZE/GET completed for the enumerated NR paths. The snapshot contains `/data/GoldHEN/plugins.ini` with a `[default]` registration for `ps4_neural_engine.prx`; this proves registration text only, not that a particular PRX build was loaded or executed.
- The observed PRX at `/data/GoldHEN/plugins/ps4_neural_engine.prx` was 317,216 bytes, SHA-256 `1481214d21aa5c826f9d51ae8cd55759e625c2b9ca7159dd9578ea632661d8e7`. This does not match the PKG 015 PRX hash recorded in its build manifest; do not overwrite or activate it until the deployment target and rollback transaction are qualified.
- `/user/app/PNEU00001/app.json` and `app.xml` were readable. The metadata references `/user/data/pkg/PS4-Neural-Engine-015.pkg`; this is an installed-app metadata observation, not proof that the app was launched or that the current PKG 015 bytes match the staged PKG.
- RPI TCP 12800 actively refused the connection. The preflight therefore did **not** identify an RPI receiver. No POST, BinLoader probe, installation, PRX activation, or GoldHEN restart was attempted.
- Firmware 14.02 and GoldHEN v2.4b18.14 remain user-reported; the installed payload hash was not read from the device.

### Unintended legacy PKG 006 staging

The same successful build event also triggered the legacy `PS4 Windows Bridge Test` workflow. Despite its name, it was not read-only: it downloaded the build artifact and uploaded/promoted a legacy PKG 006 to the console. The observed run was `45164276134`, job `137657095682`, result `success`. It staged:

- `/data/pkg/PS4-Neural-Engine-006-8d95e10d45e9.pkg`
- 6,619,136 bytes
- SHA-256 `8d95e10d45e97ced884b0ab5b787cbc095047db8b7d24ec314a3052ecac92460`

The read-only inventory subsequently showed that this file joined several existing NR PKGs in `/data/pkg/`; it did not archive or delete those files. This was an unintended side effect of the legacy workflow, not a requested release. No cleanup is being performed automatically: the archive/move operation is separate and the current root contains multiple historical NR packages that require exact per-file identity review first.

The legacy workflow was deleted from the working branch in commit `3a34afa73b004b4c9e8492b73ea9dcc72925191b`, and the static safety contract now asserts that it remains absent. This deletion must reach the repository's default branch before it prevents future `workflow_run` executions; the currently running default-branch workflow definition is not retroactively changed by a feature-branch commit.

### Automation disposition

- **Build and host/PC QA:** automated.
- **Independent PKG QA:** automated when its version-specific workflow is correctly bound to the build.
- **FTP staging/readback:** historically demonstrated, but the legacy auto-stage path is retired in the working branch; the approved version-pinned staging workflow is separate.
- **Read-only PS4 inventory:** completed once; the newer raw-byte/manifest collector still needs execution on Windows PowerShell 5.1.
- **Remote installation:** blocked because the RPI endpoint refused TCP 12800 and the exact alternative receiver protocol/foreground precondition is not established.
- **PRX replacement/activation:** blocked by the installed PRX identity mismatch and the current title-adapter fail-closed gate. The existing `[default]` registration must not be treated as permission to replace or activate a different build.
- **GoldHEN restart:** blocked; no version-matched documented restart mechanism and recovery handshake are qualified.
- **App launch/runtime observation:** not implemented/verified.

The automation is therefore **not complete end-to-end**. It is not safe to mark installation, PRX activation, restart, or runtime test as automated until their protocol and rollback gates are actually satisfied. The immediate safe action is to merge the legacy-workflow retirement, qualify the new read-only collector on the Windows runner, and then source-qualify an available install adapter without sending a mutating request until the receiver is positively identified.

## Live console evidence — 2026-11-23

- Read-only preflight run: `45162859287`; repeat snapshot: `45164276080`.
- RPI TCP 12800 refused connection; no install POST was sent.
- Both `/user/app/PNEU00001/` and `/user/app/PNEU00004/` exist. The PNEU00001 app metadata references `/user/data/pkg/PS4-Neural-Engine-015.pkg`; this is evidence of an existing app, not a verified app.pkg/SFO version.
- `plugins.ini` already has a `[default]` registration for `/data/GoldHEN/plugins/ps4_neural_engine.prx`.
- PRX FTP GET readback is 317,216 bytes / SHA-256 `1481214d21aa5c826f9d51ae8cd55759e625c2b9ca7159dd9578ea632661d8e7`; FTP LIST reports 224,128 bytes. This discrepancy is stable across two snapshots. Preserve the file and do not replace/activate it.
- PKG006 is FULL BASE, PNEU00001 APP_VER 01.02, QA PC-only; PKG015 is FULL BASE, PNEU00001 APP_VER 01.10, QA PC-only. Do not treat either as a patch or reinstall over existing app data.
- The legacy PKG006 stage workflow now binds the exact build run through the QA report and skips if a newer NR package (015) is already in `/data/pkg/`; run `45164398673` succeeded with `STAGING_SKIPPED_NEWER_RELEASE_PRESENT=true`, preserving PKG015.
