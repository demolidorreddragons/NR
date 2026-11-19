# PS4 PKG installer

The PKG is a homebrew installer app intended for a jailbroken PS4. Install it through GoldHEN/Debug Package Installer, then launch **PS4 Neural Engine Installer** once.

The installer:
1. Copies `/app0/ps4_neural_engine.prx` to `/data/GoldHEN/plugins/ps4_neural_engine.prx` using a temporary file.
2. Verifies that the copied file size matches the packaged PRX.
3. Creates or updates `/data/GoldHEN/plugins.ini`, preserving existing content and adding the PRX path under the exact `[default]` section.
4. Writes `/data/GoldHEN/ps4_neural_engine_install.log` with the result for FTP diagnostics. This file can distinguish a successful installer exit from a failure before the PRX copy or loader registration.

After a successful install, restart GoldHEN or reboot so the loader can attempt to load the PRX. The GoldHEN Plugin Loader option must be enabled in GoldHEN settings.

This package currently installs the diagnostics/capture scaffold only. It does not provide a neural inference runtime or a generic game framebuffer adapter. A successful package build or file copy is not proof that the PRX loads or that neural rendering works on hardware.
