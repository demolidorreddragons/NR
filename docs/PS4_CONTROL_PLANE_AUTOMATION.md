# PS4 Control Plane Automation

**Repository:** demolidorreddragons/NR  
**Existing bridge:** demolidorreddragons-NR.PS4-WINDOWS-BRIDGE  
**Purpose:** complete the existing PS4 LAN bridge without duplicating setup.

## Authorization recorded

The user explicitly authorized automated installation of approved NR PKGs, deployment/activation of the NR PRX, GoldHEN restart, and sensitive console operations necessary for the NR development cycle. This authorization supersedes older manual-install instructions for these named NR operations. It does not extend to unrelated software, saves, user data, arbitrary commands, or unreviewed binaries.

## Verified service facts

GoldHEN upstream documents FTP TCP 2121, BinLoader TCP 9090, Klog TCP 3232, internal package installation from `/data/pkg`, plugin support, and remote package installation. Upstream warns BinLoader is experimental and may crash the console. These feature statements do not establish the exact protocol, firmware compatibility, or availability on the user's installed GoldHEN version. See [GoldHEN upstream README](https://github.com/GoldHEN/GoldHEN) and [release history](https://github.com/GoldHEN/GoldHEN/releases).

The recorded console firmware is 14.02. The release page's listed firmware matrix must be checked against the exact installed GoldHEN build; do not infer compatibility from a port being open.

## Automation components

| Component | Current evidence | Next acceptance criterion |
|---|---|---|
| Existing Windows LAN runner | Previously verified | Reuse; never reinstall/reconfigure |
| FTP transfer/readback/staging | Historical successful run 45151203751 | Continue per-artifact SHA-256 verification |
| 2121/9090/3232 discovery | Added read-only preflight workflow | Successful run with JSON report |
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

- Treat 2121 FTP, 9090 BinLoader, 3232 Klog, remote package installation, `/data/pkg` internal package installation, and plugin support as features documented for the GoldHEN v2.4b18 family; verify the actual running endpoints with the read-only preflight.
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
