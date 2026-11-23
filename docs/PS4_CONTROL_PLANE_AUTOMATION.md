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
