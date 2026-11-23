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

The read-only inventory subsequently showed that this file joined several existing NR PKGs in `/data/pkg/`; it did not archive or delete those files. This was an unintended staging side effect of the legacy workflow. Independent QA for the exact build was performed by separate run `45164276177` and completed at 11:20:53Z, after the bridge had promoted the PKG at 11:20:30Z. QA artifact `14814061232` confirms the same hash and `QA_STATUS=PASS_PC_ONLY_NOT_HARDWARE_TESTED`. The package ultimately passed PC QA, but staging preceded the QA result; this is a confirmed sequencing defect, not a failed package QA result. No cleanup is being performed automatically.

The legacy workflow was retired from the working branch and merged into `main` by PR #30, merge commit `3600efb8f5261f1701c57635a842e1ddb7474989`. The static safety contract now asserts that the legacy workflow remains absent from the default branch.

### Automation disposition

- **Build and host/PC QA:** automated.
- **Independent PKG QA:** automated when its version-specific workflow is correctly bound to the build.
- **FTP staging/readback:** historically demonstrated, but the legacy auto-stage path is retired in the working branch; the approved version-pinned staging workflow is separate.
- **Read-only PS4 inventory:** raw-byte/manifest collector v3 completed successfully on the Windows bridge in preflight run `45167844316`; the downstream offline inspector then failed because its workflow did not check out the NR inspector script.
- **Remote installation:** blocked because the RPI endpoint refused TCP 12800 and the exact alternative receiver protocol/foreground precondition is not established.
- **PRX replacement/activation:** blocked by the installed PRX identity mismatch and the current title-adapter fail-closed gate. The existing `[default]` registration must not be treated as permission to replace or activate a different build.
- **GoldHEN restart:** blocked; no version-matched documented restart mechanism and recovery handshake are qualified.
- **App launch/runtime observation:** not implemented/verified.

The automation is therefore **not complete end-to-end**. The legacy staging workflow has been retired in merged PR #30, and the read-only collector has now completed successfully. The offline inspector workflow still needs its source-checkout fix from the evidence manifest. Installation, PRX activation, restart, and runtime test remain blocked by the unavailable RPI receiver, unqualified BGFT ABI, and unresolved PRX identity conflict.

## Live console evidence — 2026-11-23

- Read-only preflight run: `45162859287`; repeat snapshot: `45164276080`.
- RPI TCP 12800 refused connection; no install POST was sent.
- Both `/user/app/PNEU00001/` and `/user/app/PNEU00004/` exist. The PNEU00001 app metadata references `/user/data/pkg/PS4-Neural-Engine-015.pkg`; this is evidence of an existing app, not a verified app.pkg/SFO version.
- `plugins.ini` already has a `[default]` registration for `/data/GoldHEN/plugins/ps4_neural_engine.prx`.
- PRX FTP GET readback is 317,216 bytes / SHA-256 `1481214d21aa5c826f9d51ae8cd55759e625c2b9ca7159dd9578ea632661d8e7`; FTP LIST reports 224,128 bytes. This discrepancy is stable across two snapshots. Preserve the file and do not replace/activate it.
- PKG006 is FULL BASE, PNEU00001 APP_VER 01.02, QA PC-only; PKG015 is FULL BASE, PNEU00001 APP_VER 01.10, QA PC-only. Do not treat either as a patch or reinstall over existing app data.
- The legacy PKG006 stage workflow now binds the exact build run through the QA report and skips if a newer NR package (015) is already in `/data/pkg/`; run `45164398673` succeeded with `STAGING_SKIPPED_NEWER_RELEASE_PRESENT=true`, preserving PKG015.


### Second legacy auto-stage observed — PKG 015 build

A later build event triggered the same legacy bridge workflow again:
- Bridge run `45164889360`, job `137659150208`, successful.
- Triggering build run `45163958301`, branch `feature/gcn8-numeric-kernels`, source commit `54ff897a07d59c318c73e23be1dd2286df9e9dce`.
- Staged path: `/data/pkg/PS4-Neural-Engine-015-089684400316.pkg`.
- Size: 6,619,136 bytes.
- SHA-256: `089684400316f26d4a7d8d04cb51f768ea14e4ea6351fdaab7007e5e073dc391`.
- The build run itself contained independent QA job `137658439809`, which completed successfully before the workflow_run event. Artifact `14814412124` includes a QA report matching this exact hash and `QA_STATUS=PASS_PC_ONLY_NOT_HARDWARE_TESTED`. This package therefore had a PC-only QA PASS before staging. The legacy bridge still failed to explicitly bind/verify that QA artifact, so its stage gate is structurally weak; it is not hardware-approved.

This is distinct from the earlier independently QA'd PKG 015 hash `2fcaf679766fefbed4bc60cc31beb0227b8bfefd4eb8070160f65a402be747e9`. Do not conflate the two files because they share a release number. No file has been deleted or archived by this audit.

### Collector v3 scope extension

The reviewed collector now also snapshots, with GETSIZE/GET/GETSIZE and local SHA-256:
- installed app package bytes for `/user/app/PNEU00001/app.pkg` and `/user/app/PNEU00004/app.pkg`;
- the previously approved PKG 015 staging path;
- the exact legacy-staged PKG 006 file and the later unqualified PKG 015 file.

Each file remains subject to the 16 MiB upper bound. The new reads are read-only and are intended to establish E0/E2 identity on the next eligible preflight. They do not install, modify, or launch anything.

### Historical installer log signals — observed, not causal proof

The legacy bridge's read-only pre-stage diagnostic in run '45164889360' also retrieved '/data/GoldHEN/ps4_neural_engine_install.log' and printed its existing contents. It contains historical entries including:
- 'ERROR: cannot create temporary PRX in /data/GoldHEN/plugins (errno=17)';
- 'ABORTED: CONFLICT; managed path is inaccessible or not a regular file; all data preserved.';
- 'ERROR: cannot inspect plugins.ini (errno=78)';
- 'LEGACY_MIGRATION: unverified old PRX preserved as backup; managed manifest installed.';
- 'ABORTED: CONFLICT; cannot safely open installed PRX without following symlinks (errno=78); all data preserved.'

These are **OBSERVED strings in a historical application log**. They are not tied to a single current process/session, do not establish which exact installer build emitted each line, and do not prove that the currently observed PRX path is a symlink. The existing FTP directory listing reports the current PRX as a regular-file entry, but FTP LIST metadata is not a definitive lstat/readlink result. Keep the cause as UNKNOWN / INSUFFICIENT EVIDENCE until the log is preserved with the new collector and the exact filesystem object type/path is established through a safe, documented interface.

Collector v3 additionally downloads the NR installer log and the adjacent 'ps4_neural_engine.prx.pne-release.ini' manifest so the next evidence artifact can bind these historical messages to current PRX bytes and registration state.


## Installed application byte identity — 2026-11-23

A separate read-only identity audit completed on the Windows PS4 bridge:
- Audit run `45165759218`, job `137662173022`, branch `automation/installed-app-identity-audit-run`, commit `6f78fceed00973f4bb46c00e08f442f3c7563b24`.
- Evidence artifact `14814688337`, ZIP SHA-256 `d2952176c7f6f1a191a5746290622747e6602837e2816da7dcc16ed71345f081`.
- `/user/app/PNEU00001/app.pkg`: 6,619,136 bytes, SHA-256 `2fcaf679766fefbed4bc60cc31beb0227b8bfefd4eb8070160f65a402be747e9`.
- `/user/data/pkg/PS4-Neural-Engine-015.pkg`: same 6,619,136 bytes and same SHA-256. The installed app package is byte-for-byte identical to the source package referenced by its app.json. This establishes installed package identity for the canonical QA'd PKG 015; it does not prove the app was launched or that the PRX is active.
- `app.json` points to `/user/data/pkg/PS4-Neural-Engine-015.pkg`. Its `packageDigest` is `7A65DF53B2B5CB51F9042EE2DFE96BFAC8877BE572B54999CC9B5AB27A992AD9`, which is not the raw-file SHA-256. The digest's semantics are unverified; do not classify that difference as corruption.
- The inspected staged candidate `/data/pkg/PS4-Neural-Engine-015-7609b5d2b536.pkg` is 6,619,136 bytes, SHA-256 `7609b5d2b53628bd0a22c85c31ac72df2ae36d9047c9a4908ad24ff2f8fc5548`; it does **not** match the installed app package.
- Direct FTP paths for `/user/app/PNEU00001/eboot.bin` and `/user/app/PNEU00001/ps4_neural_engine.prx` returned FTP 78/unavailable. The installed executable and PRX are inside app.pkg, not exposed as loose files at those paths.
- The PKG 015 QA report identifies packaged PRX SHA-256 `1e4531711a0768cfeb29f1d6f66b9541cf087b141eda7a05fed755d12507866c`; the currently installed GoldHEN PRX readback hash is `1481214d21aa5c826f9d51ae8cd55759e625c2b9ca7159dd9578ea632661d8e7`. These are different binaries. No overwrite or activation is authorized by this evidence.

### First post-merge preflight failure and correction

The first post-merge preflight run `45165799711` failed before any FTP request. The Windows runner had no Git executable, so checkout used the GitHub REST archive; the collector-identity step incorrectly ran `git rev-parse`. In addition, that step used the runner's restrictive PowerShell execution policy. No console data was collected or changed by this failed run.

The follow-up branch now uses the documented `actions/checkout` output `steps.checkout-collector.outputs.commit` to bind the exact collector commit, sets `COLLECTOR_REF=main`, and invokes the script with `-ExecutionPolicy Bypass`. The bypass applies only to this reviewed workflow script and does not change the machine policy. The artifact is now named with the preflight run ID so a downstream offline inspector can download the exact run without guessing.

### Offline installed-package inspection

The new offline inspector consumes only the preserved preflight artifact. It validates and extracts the installed app.pkg and any captured staged PKGs with a pinned OrbisPkgTool build, reads embedded PARAM.SFO, records Title ID/category/version/content ID/storage type, hashes the packaged PRX, and compares installed/source/staged package identities. It makes no network calls and cannot mutate the PS4. Hardware validation remains explicitly false.

### Alternative install adapter research — BGFT / AppInstUtil

A local BGFT installation path exists in pinned ItemzFlow source, but it is not qualified for NR on firmware 14.02:
- ItemzFlow commit `4013e1ef188672db63346fef4221b1d8f4f13f02` uses AppInstUtil/BGFT task registration, start, and progress polling.
- flatz definitions commit `a8d044c12ea2dc3fa190ce1769eeb48b30d28ed4` use 64-bit package-size/progress fields.
- OpenOrbis 0.5.4 commit `0a1aaf9dd4a92695538bdeb09fb056d06dd11725` declares corresponding fields as 32-bit.
- This ABI disagreement blocks any BGFT call until exact exports, structures, task lifecycle, and firmware 14.02 behavior are independently verified. No BGFT install agent exists in the current PRX; no speculative payload will be deployed.

References:
- https://github.com/LightningMods/Itemzflow/blob/4013e1ef188672db63346fef4221b1d8f4f13f02/itemzflow/source/installpkg.cpp
- https://github.com/flatz/ps4_stub_lib_maker_v2/blob/a8d044c12ea2dc3fa190ce1769eeb48b30d28ed4/include/bgft.h
- https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/0a1aaf9dd4a92695538bdeb09fb056d06dd11725/include/orbis/_types/bgft.h


## Installed application byte identity — 2026-11-23

A separate read-only identity audit completed on the Windows PS4 bridge:
- Audit run `45165759218`, job `137662173022`, branch `automation/installed-app-identity-audit-run`, commit `6f78fceed00973f4bb46c00e08f442f3c7563b24`.
- Evidence artifact `14814688337`, ZIP SHA-256 `d2952176c7f6f1a191a5746290622747e6602837e2816da7dcc16ed71345f081`.
- `/user/app/PNEU00001/app.pkg`: 6,619,136 bytes, SHA-256 `2fcaf679766fefbed4bc60cc31beb0227b8bfefd4eb8070160f65a402be747e9`.
- `/user/data/pkg/PS4-Neural-Engine-015.pkg`: same 6,619,136 bytes and same SHA-256. The installed app package is byte-for-byte identical to the source package referenced by its app.json. This establishes installed package identity for the canonical QA'd PKG 015; it does not prove the app was launched or that the PRX is active.
- `app.json` points to `/user/data/pkg/PS4-Neural-Engine-015.pkg`. Its `packageDigest` is `7A65DF53B2B5CB51F9042EE2DFE96BFAC8877BE572B54999CC9B5AB27A992AD9`, which is not the raw-file SHA-256. The digest's semantics are unverified; do not classify that difference as corruption.
- The inspected staged candidate `/data/pkg/PS4-Neural-Engine-015-7609b5d2b536.pkg` is 6,619,136 bytes, SHA-256 `7609b5d2b53628bd0a22c85c31ac72df2ae36d9047c9a4908ad24ff2f8fc5548`; it does **not** match the installed app package.
- Direct FTP paths for `/user/app/PNEU00001/eboot.bin` and `/user/app/PNEU00001/ps4_neural_engine.prx` returned FTP 78/unavailable. The installed executable and PRX are inside app.pkg, not exposed as loose files at those paths.
- The PKG 015 QA report identifies packaged PRX SHA-256 `1e4531711a0768cfeb29f1d6f66b9541cf087b141eda7a05fed755d12507866c`; the currently installed GoldHEN PRX readback hash is `1481214d21aa5c826f9d51ae8cd55759e625c2b9ca7159dd9578ea632661d8e7`. These are different binaries. No overwrite or activation is authorized by this evidence.

### First post-merge preflight failure and correction

The first post-merge preflight run `45165799711` failed before any FTP request. The Windows runner had no Git executable, so checkout used the GitHub REST archive; the collector-identity step incorrectly ran `git rev-parse`. It also used the runner's restrictive PowerShell execution policy. No console data was collected or changed.

A second attempt, run `45167496411`, fixed the PowerShell policy but exposed that `actions/checkout` output `commit` is empty when the action falls back to the REST archive. It also failed before FTP; the log shows `COLLECTOR_COMMIT` empty and no network request to the PS4.

The corrected follow-up pins checkout to `${{ github.sha }}` in the `workflow_run` context and passes that same exact default-branch SHA as `COLLECTOR_COMMIT`. This avoids depending on Git or checkout outputs and makes the collector source commit identical to the checked-out archive. It keeps `COLLECTOR_REF=main` and invokes the reviewed script with `-ExecutionPolicy Bypass` for that process only; the machine policy is not changed. The artifact is named with the preflight run ID so the downstream offline inspector can download the exact run without guessing.

### Offline installed-package inspection

The new offline inspector consumes only the preserved preflight artifact. It validates and extracts the installed app.pkg and captured staged PKGs with a pinned OrbisPkgTool build, reads embedded PARAM.SFO, records Title ID/category/version/content ID/storage type, hashes the packaged PRX, and compares installed/source/staged package identities. It makes no network calls and cannot mutate the PS4. Hardware validation remains explicitly false.

### Alternative install adapter research — BGFT / AppInstUtil

A local BGFT installation path exists in pinned ItemzFlow source, but it is not qualified for NR on firmware 14.02:
- ItemzFlow commit `4013e1ef188672db63346fef4221b1d8f4f13f02` uses AppInstUtil/BGFT task registration, start, and progress polling.
- flatz definitions commit `a8d044c12ea2dc3fa190ce1769eeb48b30d28ed4` use 64-bit package-size/progress fields.
- OpenOrbis 0.5.4 commit `0a1aaf9dd4a92695538bdeb09fb056d06dd11725` declares corresponding fields as 32-bit.
- This ABI disagreement blocks any BGFT call until exact exports, structures, task lifecycle, and firmware 14.02 behavior are independently verified. No BGFT install agent exists in the current PRX; no speculative payload will be deployed.

References:
- https://github.com/LightningMods/Itemzflow/blob/4013e1ef188672db63346fef4221b1d8f4f13f02/itemzflow/source/installpkg.cpp
- https://github.com/flatz/ps4_stub_lib_maker_v2/blob/a8d044c12ea2dc3fa190ce1769eeb48b30d28ed4/include/bgft.h
- https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/0a1aaf9dd4a92695538bdeb09fb056d06dd11725/include/orbis/_types/bgft.h

## Latest successful control-plane snapshot — 2026-11-23

- Preflight run `45167844316`, build run `45167536520`, build commit `f220efed706dc5750aee646aa8aa754389ef56a4`, collector commit `9feef27b83b4fa22255a9a578f496e78b9a6469d`.
- Evidence artifact `ps4-control-plane-preflight-45167844316`, artifact ID `14815586255`, archive SHA-256 `11e0a62efd7519fc361916f46b0febac7ec0ccc30ec5377fd39dd5e9c8a9c68f`. FTP LIST/GETSIZE/GET completed successfully; no remote files were modified.
- RPI TCP 12800 actively refused the GET-only `/api` request. No RPI backend was identified; no POST was sent. BinLoader 9090 was not probed.
- `/data/GoldHEN/plugins.ini` is 55 bytes, SHA-256 `5b186ce425fcdc4cdc25312635114ca79dc24e73545c315ad645abca39abd5d0`, and contains `[default]` registration for `/data/GoldHEN/plugins/ps4_neural_engine.prx`. This proves registration text only, not successful load or execution.
- Current PRX FTP GET: 317,216 bytes, SHA-256 `1481214d21aa5c826f9d51ae8cd55759e625c2b9ca7159dd9578ea632661d8e7`. The raw FTP LIST line reports 224,128 bytes for the same path. GETSIZE before/after is stable at 317,216, but LIST and GET disagree; do not treat the PRX as identity-verified.
- Adjacent release manifest `ps4_neural_engine.prx.pne-release.ini` is 289 bytes, SHA-256 `cadaf8665dc5bfe4f28999cee4a5bbc5153aa15220efe58de50ddb75f95ca0a3`. It declares release 010, app version 01.06, expected PRX SHA-256 `54e8595144a3a7e1d36602a1100bae6189f0955175c355bd3ac67d9cb7f85277`, `abi_status=unverified`, and `hardware_status=not_tested`. The expected hash does not match the current GET hash.
- The plugin directory listing also shows `ps4_neural_engine.prx.pne-tmp` (224,128 bytes), `ps4_neural_engine.prx.pne-backup` (206,528 bytes), and `ps4_neural_engine.prx.pne-backup.001` (224,128 bytes). These backup/temp files have not yet been downloaded; PR40 adds read-only snapshots and hash comparison. Do not restore or overwrite any of them before identity analysis.
- Installer log SHA-256 `9c37f7b5e45ed70e6b58b17150c7b213ef3a228543006792afe29b8115448e4d` contains historical `errno=17` and `errno=78` conflict entries plus older `ALREADY_CURRENT` messages. Those older messages are contradicted by the current manifest/hash mismatch and must not be treated as current success evidence.
- Installed PNEU00001 app.pkg and app.json source package remain byte-identical at SHA-256 `2fcaf679766fefbed4bc60cc31beb0227b8bfefd4eb8070160f65a402be747e9`, APP_VER 01.10, CATEGORY `gde`. Staged PKG015 candidates `7609b5...` and `089684...` are distinct FULL BASE packages at the same APP_VER 01.10; neither is a patch or safe upgrade.
- Offline inspection run `45167943764` failed before executing the script because the workflow checked out OrbisPkgTool but not the NR repository. No package inspection result was produced by that run. PR40 binds the NR script checkout to `collector_commit` from the exact preflight artifact and expands PRX integrity analysis.

Current disposition: preserve the installed app, `plugins.ini`, active PRX, manifest, and all backup/temp files. Do not install a same-version FULL BASE PKG, replace/activate PRX, restart GoldHEN, or claim runtime validation.
## Latest automated preflight and offline PKG inspection — 2026-11-23

- Build run `45168834153` succeeded on commit `dd08ba375c5e618c6472e465a67c45896b033b67`.
- Read-only preflight run `45169044022` succeeded; artifact ID `14815193437`, SHA-256 `f4cedd493d93eaa0220c815e44c70b070d60054f21d13261bfffce051bf56496`. It downloaded the active PRX plus `.pne-tmp`, `.pne-backup`, and `.pne-backup.001` without changing any remote file.
- Offline inspection run `45169142004` succeeded; artifact ID `14816675444`. Independent OrbisPkgTool validated/extracted installed PNEU00001, installed PNEU00004, source PKG015, canonical staged PKG015, alternate staged PKG015 candidates, and all captured PKG006 candidates. All package SFO identities passed PC-only validation; hardware validation remains not performed.
- Installed PNEU00001 app.pkg and app.json source and canonical staged PKG015 are byte-identical: 6,619,136 bytes, SHA-256 `2fcaf679766fefbed4bc60cc31beb0227b8bfefd4eb8070160f65a402be747e9`, APP_VER 01.10. Alternate PKG015 candidates `7609b5...` and `089684...` are different FULL BASE packages, also APP_VER 01.10, and both contain packaged PRX SHA-256 `1e4531711a0768cfeb29f1d6f66b9541cf087b141eda7a05fed755d12507866c`. They are not patches or version upgrades.
- All captured PKG006 candidates are FULL BASE APP_VER 01.02 and contain packaged PRX SHA-256 `7561da3d3b83766becddcb37c1257d83c61009b5e814825ee60694d19f9cf704`.
- Active GoldHEN PRX GET remains 317,216 bytes / SHA-256 `1481214d21aa5c826f9d51ae8cd55759e625c2b9ca7159dd9578ea632661d8e7`, while raw FTP LIST reports 224,128 bytes. Release manifest expected SHA-256 is `54e8595144a3a7e1d36602a1100bae6189f0955175c355bd3ac67d9cb7f85277`.
- `.pne-tmp` and `.pne-backup.001` are byte-identical at 224,128 bytes / SHA-256 `54e8595144a3a7e1d36602a1100bae6189f0955175c355bd3ac67d9cb7f85277`, matching the release manifest. `.pne-backup` is 206,528 bytes / SHA-256 `7561da3d3b83766becddcb37c1257d83c61009b5e814825ee60694d19f9cf704`, matching the packaged PRX in the older PKG006 builds.
- Thus there are at least three distinct PRX identities: PKG015 embedded PRX `1e453...`, PKG006 embedded PRX / `.pne-backup` `7561...`, and release-manifest / `.pne-tmp` / `.pne-backup.001` `54e859...`; the active-path FTP GET returns a fourth hash `148121...`. Do not restore or overwrite based only on filenames.
- The first successful offline inspector run had a parser bug in the PRX metadata comparisons (double-escaped PowerShell regexes), so its PKG validation/SFO/embedded-PRX hashes are valid but its LIST-size, release-manifest and plugins.ini comparison fields are not yet trusted. PR42 corrected those regexes and added regression assertions; corrected offline inspection run 45170231258 now reports the PRX manifest/LIST/plugin registration comparisons.
- RPI TCP 12800 still actively refuses the GET-only probe. No mutating request, install, PRX activation, GoldHEN restart, or runtime test occurred.

## Reversible NR package-root normalization workflow — 2026-11-23

The project requires one canonical Full/Base NR PKG in `/data/pkg/`, with older/superseded NR packages preserved under `/data/pkg/old/`. The pre-archive read-only inventory contained eight NR packages in the root: canonical PKG 015 (identical to the installed app), two alternate PKG 015 builds, and five older PKG 006 builds. Two unrelated third-party game PKGs were also present and remained out of scope.

The new `PS4 Archive Superseded NR PKGs` workflow is triggered only after the offline installed-package inspection succeeds. It:
1. Requires installed app.pkg, app.json source and canonical staged PKG 015 to remain byte-identical and PASS_PC_ONLY.
2. Requires every other NR root PKG to be present in the exact inspection report and pass PKG/SFO validation as TITLE_ID PNEU00001, CATEGORY gde, storage digital50, and APP_VER no newer than installed.
3. Re-reads installed/source/canonical packages and each archive candidate with GETSIZE-before/after + GET + SHA-256.
4. Uses FTP RENAME only to move the seven superseded NR packages into the existing `/data/pkg/old/`, with collision-safe names containing the offline inspection run ID.
5. Reads each archive destination back and verifies exact size/SHA-256.
6. Verifies the root retains exactly `PS4-Neural-Engine-015.pkg` and that all non-NR root names (including third-party game PKGs) are unchanged.

The workflow never deletes or uploads files, never creates/removes directories, and never touches installed apps, PNEU00004, plugins.ini, PRX files, GoldHEN state, or RPI. It is reversible archival only, not installation.

**Completed:** archive run `45172997929` succeeded with `ARCHIVE_COMPLETE_READBACK_VERIFIED`. It consumed offline inspection run `45172819393`, preflight run `45172681859`, build run `45172444644`, build commit `c7f6f550ec2a685c3062fd979abbc443fbe04be9`. Artifact `14817842746` (110,304,230 bytes; SHA-256 `98cbf6efe152117185b7062e1c89100dd9c8d6c677b75bc4b11ff85098fcf647`) contains the manifest, raw before/after listings, source snapshots and archive readbacks.

Seven superseded NR packages were moved into `/data/pkg/old/` using FTP RENAME only. Every source snapshot and archive readback matched its offline SHA-256 and 6,619,136-byte size. The final root contains exactly `PS4-Neural-Engine-015.pkg` (6,619,136 bytes; SHA-256 `2fcaf679766fefbed4bc60cc31beb0227b8bfefd4eb8070160f65a402be747e9`), still byte-identical to installed app.pkg and app.json source. The two third-party game package names were unchanged. Manifest flags confirm `installation_performed=false`, `prx_modified=false`, `plugins_ini_modified=false`, `goldhen_restarted=false`.

## GoldHEN config discovery follow-up — 2026-11-23

The `/data/GoldHEN/config.ini` file is present (the directory listing reports 1,127 bytes). PR47 adds a safe in-memory fingerprint: size/SHA-256 plus only allowlisted relevant boolean states for RPI/remote/package/FTP/Klog/BinLoader/plugin/install keys. Non-boolean values are redacted, the raw file is never uploaded, and the temporary runner-local copy is deleted. It also captures `plugins.ini.pne-tmp` and `plugins.ini.pne-backup` so the active registration can be compared with the installer's backups.

The `/user/app/` listing contains no identifiable Remote Package Installer title, and TCP 12800 refused the read-only GET probe. No setting is changed by this follow-up.

### Archive workflow first attempts and collision-probe correction — 2026-11-23

The first automatic archive run `45171209915` failed before any FTP mutation because active-mode FTP LIST timed out. No package was renamed. The follow-up switched to the passive FTP mode already proven by the read-only collector and increased the request timeout.

The next run `45171798451` completed the raw root/old directory listings and live GET snapshots of installed app.pkg, app.json source and canonical staged PKG 015, and downloaded the first alternate PKG 015 candidate. It then failed before the first rename because the PS4 FTP server returns `550 File unavailable` for a deliberately nonexistent archive destination. The script had incorrectly used GETSIZE as a destination-collision probe and treated the expected 550 as fatal. Artifact `14816757713` preserves the pre-move snapshots and initial listings; no archive destination was created and no source PKG was moved.

The corrected script uses the captured `/data/pkg/old/` LIST names for destination collision detection and idempotent archive lookup. It checks source GETSIZE only when the source is present in the root LIST; if GETSIZE returns 550 while the source is still listed, it treats that as access denied and aborts. If the source is absent from root LIST, it accepts only a matching archive name from the old-directory LIST whose GET readback hash/size matches the offline report. After RENAME, it verifies the destination by GET/readback and source absence with a fresh final root LIST; it does not issue GETSIZE against an expected-missing destination or source. The workflow continues to use passive FTP, unique run/attempt evidence directories, and readback SHA-256 verification. The next archive run must still pass the full inventory, live identity and final-root checks before reporting completion.


### Idempotent archive recovery hardening

The archive plan now supports safe reruns of the same offline-inspection event after a partial sequence of successful renames. It tolerates missing reported source paths only when a corresponding run-scoped archive name is present in `/data/pkg/old/` and the archive readback matches the exact offline SHA-256 and byte count. Any source still present in the root LIST but returning FTP 550 is classified as access denied, not missing. Extra NR packages in the root that are absent from the validated report cause an immediate abort. Post-rename source absence is confirmed by the final root LIST, not by GETSIZE on a path expected to be absent.


### Post-normalization no-op and cross-run recovery

After archive run `45172997929`, subsequent preflight inventories no longer contain the seven moved files under `/data/pkg/`. The offline inspector still emits target records for known historical paths, marking absent files `not_collected`. The archive workflow now skips only such `not_collected` entries that are absent from the live root LIST; if any unvalidated path is still present in the root, it aborts. With only canonical PKG 015 remaining, the workflow performs identity/final-root checks and completes as a no-op.

For same-run partial recovery, a source absent from the root may be accepted only when a same-basename archive exists under `/data/pkg/old/` and its GET readback matches the offline report's exact size and SHA-256. The search spans run IDs, so a fresh inspection can still recognize a previously verified archive. A missing source without a matching verified archive remains a hard failure.

### End-to-end archive automation and idempotency proof — 2026-11-23

The merged automation was exercised end to end after the no-op fix, without installing or activating anything:

- Host CI: run `45175419732` — SUCCESS.
- Build: run `45175419701`, commit `1c314ada74cb6ce94ae4e5c9fd963d0aa3691b5c` — SUCCESS.
- Read-only PS4 control-plane preflight: run `45175564496` — SUCCESS.
- Independent offline installed/staged PKG inspection: run `45175650256` — SUCCESS; artifact `14818785903`.
- Legacy PKG 006 stage guard: run `45175676727` — SUCCESS with `STAGING_SKIPPED_NEWER_RELEASE_PRESENT=true`; canonical PKG 015 remained in the root and no PKG 006 upload/rename was performed.
- Evidence-gated archive rerun: run `45175810684` — `ARCHIVE_COMPLETE_READBACK_VERIFIED`; artifact `14819940505`, 25,266,216 bytes, ZIP SHA-256 `2800f53a584c478bb2bf48df8dd4f5e1b78df8ebe88008405a80f7dfcf8b81b4`.

The archive manifest binds build/preflight/offline/archive run IDs and records an empty `files` array (zero RENAME operations), final NR root exactly `PS4-Neural-Engine-015.pkg`, `third_party_root_names_unchanged=true`, and all mutation flags false: `installation_performed`, `prx_modified`, `plugins_ini_modified`, `goldhen_restarted`. The canonical package was re-read at 6,619,136 bytes with SHA-256 `2fcaf679766fefbed4bc60cc31beb0227b8bfefd4eb8070160f65a402be747e9`.

This proves the automatic build → read-only preflight → offline inspection → archive no-op chain and its post-normalization idempotency. It does **not** complete remote installation, PRX deployment/activation, GoldHEN restart, application launch, or GPU/runtime validation; those remain separately gated by unresolved firmware/API/PRX identity evidence.

