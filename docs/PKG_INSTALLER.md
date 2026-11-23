# PS4 PKG installer

PKG 006 is a full base-application installer (not a patch) intended for a jailbroken PS4. It carries APP_VER/VERSION 01.02 and a freshly built eboot containing the corrected byte-stream PRX copier. This is deliberate: the audited console installation was an older `CATEGORY=gd`, `APP_VER=1.00` base, not the `gde` PKG 002 that patch 003 targeted. Install it through GoldHEN/Debug Package Installer, then launch **PS4 Neural Engine Installer** once.

The installer:
1. Copies `/app0/ps4_neural_engine.prx` to `/data/GoldHEN/plugins/ps4_neural_engine.prx` using a temporary file.
2. Verifies the copied PRX by reading both files back and comparing every byte; it does not trust PS4 `stat`/`fstat` size metadata. Interrupted reads (`EINTR`) are retried, and short reads are accumulated independently on each stream before byte comparison.
3. Creates or updates `/data/GoldHEN/plugins.ini`, preserving existing content and adding the PRX path under the exact `[default]` section.
4. Writes `/data/GoldHEN/ps4_neural_engine_install.log` with the result for FTP diagnostics. This file can distinguish a successful installer exit from a failure before the PRX copy or loader registration.

After a successful install, restart GoldHEN or reboot so the loader can attempt to load the PRX. The GoldHEN Plugin Loader option must be enabled in GoldHEN settings.

This package currently installs the diagnostics/capture scaffold only. It does not provide a neural inference runtime or a generic game framebuffer adapter. A successful package build or file copy is not proof that the PRX loads or that neural rendering works on hardware.


For troubleshooting, the installer log records the failing operation and the platform `errno` value (for example, source lookup/open, temporary destination creation, write, verification, or final rename). Capture the log after the latest launch attempt; older log contents do not describe a newer attempt.

## Current console identity correction — 2026-11-23

The opening paragraph above is historical and no longer describes the installed console state. A read-only FTP audit and pinned OrbisPkgTool inspection now prove:
- Installed PNEU00001 app.pkg is 6,619,136 bytes, SHA-256 2fcaf679766fefbed4bc60cc31beb0227b8bfefd4eb8070160f65a402be747e9.
- The app.json-referenced /user/data/pkg/PS4-Neural-Engine-015.pkg is byte-identical to the installed app.pkg.
- Independent SFO: TITLE_ID PNEU00001, APP_VER/VERSION 01.10, CATEGORY gde, CONTENT_ID IV0000-PNEU00001_00-PS4NEURALENG0000; package validation PASS.
- The staged candidate /data/pkg/PS4-Neural-Engine-015-7609b5d2b536.pkg is a different FULL BASE package with the same APP_VER 01.10, SHA-256 7609b5d2b53628bd0a22c85c31ac72df2ae36d9047c9a4908ad24ff2f8fc5548. It is not a patch or version upgrade.
- PKG006 is FULL BASE APP_VER 01.02 and is older than the installed 01.10 app. Do not install PKG006 or the same-version PKG015 candidate over the existing app; either operation risks replacing existing app data.

The packaged PRX hash 1e4531711a0768cfeb29f1d6f66b9541cf087b141eda7a05fed755d12507866c differs from the current GoldHEN plugin PRX readback hash 1481214d21aa5c826f9d51ae8cd55759e625c2b9ca7159dd9578ea632661d8e7. The PRX FTP LIST/GET size mismatch and installer errno=78 identity conflict remain unresolved. Do not replace or activate the plugin until the file identity and loader lifecycle are qualified.

RPI TCP 12800 refused the read-only probe. BGFT/AppInstUtil remains unqualified because pinned source headers disagree on 32-bit versus 64-bit size/progress fields. No remote GoldHEN restart API is qualified. The instruction above to restart GoldHEN or reboot is not a remote protocol or proof of automation; do not invent a command.

Until a genuine associated patch with a higher APP_VER is built and independently validated, preserve the installed 01.10 app and do not install another full-base package.