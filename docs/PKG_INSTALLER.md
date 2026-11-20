# PS4 PKG installer

PKG 005 is a full base-application installer (not a patch) intended for a jailbroken PS4. It carries APP_VER/VERSION 01.01 and a freshly built eboot containing the corrected byte-stream PRX copier. This is deliberate: the audited console installation was an older `CATEGORY=gd`, `APP_VER=1.00` base, not the `gde` PKG 002 that patch 003 targeted. Install it through GoldHEN/Debug Package Installer, then launch **PS4 Neural Engine Installer** once.

The installer:
1. Copies `/app0/ps4_neural_engine.prx` to `/data/GoldHEN/plugins/ps4_neural_engine.prx` using a temporary file.
2. Verifies the copied PRX by reading both files back and comparing every byte; it does not trust PS4 `stat`/`fstat` size metadata. Interrupted reads (`EINTR`) are retried, and short reads are accumulated independently on each stream before byte comparison.
3. Creates or updates `/data/GoldHEN/plugins.ini`, preserving existing content and adding the PRX path under the exact `[default]` section.
4. Writes `/data/GoldHEN/ps4_neural_engine_install.log` with the result for FTP diagnostics. This file can distinguish a successful installer exit from a failure before the PRX copy or loader registration.

After a successful install, restart GoldHEN or reboot so the loader can attempt to load the PRX. The GoldHEN Plugin Loader option must be enabled in GoldHEN settings.

This package currently installs the diagnostics/capture scaffold only. It does not provide a neural inference runtime or a generic game framebuffer adapter. A successful package build or file copy is not proof that the PRX loads or that neural rendering works on hardware.


For troubleshooting, the installer log records the failing operation and the platform `errno` value (for example, source lookup/open, temporary destination creation, write, verification, or final rename). Capture the log after the latest launch attempt; older log contents do not describe a newer attempt.
