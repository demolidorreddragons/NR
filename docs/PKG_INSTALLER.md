# PS4 PKG installer

Release 010 is the next candidate full/base application installer (not a patch), with APP_VER/VERSION 01.06. It includes the plugins.ini opened-descriptor inspection correction and regression assertions. Release 009 remains a historical PC-only QA result; release 010 must pass its own build and independent package QA before it is considered staged or ready.

The installer validates the release manifest and PRX SHA-256 before modifying persistent paths. Managed-path inspection uses `open(O_NOFOLLOW)` and `fstat()` for file-type bits only. Staging files are reserved exclusively with `O_CREAT | O_EXCL`, avoiding stale temporary-name collisions without deleting old temporary files or backups.

The installer supports initial installation and plugin update through the same full/base PKG. This is distinct from updating the application itself through Package Manager: this PKG is not a PS4 patch and no base/patch update capability is claimed.

After a successful plugin installation, restart GoldHEN or reboot so the loader can attempt to load the PRX. The GoldHEN Plugin Loader option must be enabled in GoldHEN settings. Package installation, eboot execution, PRX activation and reboot remain separate operations.

The package currently installs the diagnostics/capture scaffold only. It does not provide a complete neural inference runtime or a generic game framebuffer adapter. A successful package build or file copy is not proof that the PRX loads or that neural rendering works on hardware.

Troubleshooting must capture `/data/GoldHEN/ps4_neural_engine_install.log` after the latest launch attempt; older log contents do not describe a newer attempt. Preserve existing plugin registrations, backups, stale temporaries, crash dumps and unrelated files.
