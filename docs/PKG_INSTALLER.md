# PS4 PKG installer

## Current release state — PKG 012 (2026-11-22)

Release 012 is a **Full/Base** application package (`CATEGORY=gde`), not a patch. It is a read-only preflight application, not a PRX installer:

- It validates `/app0/pne-release.ini` against the bundled PRX SHA-256, displays a notification, and exits.
- It does **not** copy or activate the PRX, edit GoldHEN plugin configuration, restart GoldHEN, or change persistent console state.
- Manifest deliberately remains `abi_status=unverified` and `hardware_status=not_tested`.
- Build and independent package QA: run `45062759996`, SUCCESS. QA is PC-only, not hardware validation.
- PS4 staging/read-back: run `45063363596`, SUCCESS. Remote path `/data/pkg/PS4-Neural-Engine-012.pkg`; 6,619,136 bytes; SHA-256 `240914f39c1dbfec39b0f21fdd022dd30f10356341860e47fffaacef3aa3825f`.
- The read-only staging audit found TITLE_ID `PNEU00001` already present under `/user/app/`. This is a clean-install Full/Base PKG with the same TITLE_ID, so do not try to install it over the existing app. If the user chooses to perform this controlled preflight test, the existing app must first be removed manually by the user, after considering any app data. The assistant must not uninstall it.
- This is approved only for a **controlled PKG installation and app-startup/preflight test**. It is not a Neural Rendering runtime release and does not authorize PRX deployment or activation.

### Historical release 011

### Release 011 state (2026-11-20)

Release 011 is a full/base application package, not a Package Manager patch. It has `CATEGORY=gde`, `TITLE_ID=PNEU00001`, `CONTENT_ID=IV0000-PNEU00001_00-PS4NEURALENG0000`, `APP_VER=01.07`, and `VERSION=01.07`. The user attempted to install it over the already-installed application and the PS4 Package Manager returned `Package already installed`. The package was not installed and its eboot was not executed.

Do not ask the user to retry release 011. At that time the instruction was to preserve the existing installed application; the current PKG 012 clean-install test requires any removal to be decided and performed manually by the user. Increasing `APP_VER` does not convert a base PKG into a patch.

The release 011 build/QA evidence is PC-only:
- Build commit: `34c9c6ae1412ea71f58f03eedf8e8f0572114654`.
- Host tests: run `44852720353` (PASS).
- Release build: run `44852720357` (PASS).
- Independent package QA: PC-only PASS.
- PS4 staging: run `44853027094`, attempt 2 (SUCCESS; remote read-back hash matched).
- PKG size: 6,619,136 bytes.
- PKG SHA-256: `f7eb91f19356f92991536d36e2483a853907ddcd467df9579842383f8b87b7ef`.
- PRX SHA-256: `54e8595144a3a7e1d36602a1100bae6189f0955175c355bd3ac67d9cb7f85277`. This is byte-identical to release 010; release 011 changes the installer/eboot recovery path, not the PRX runtime. It therefore does not establish a fix for CE-34878-0.

## Two independent update mechanisms

1. **GoldHEN plugin installation/update:** the same full/base installer PKG contains the PRX and an idempotent eboot. This is the project's agreed hybrid behavior.
2. **Updating the installer application itself through PS4 Package Manager:** this requires a genuine patch package associated with the installed base application. This is a separate package-generation path. It must not be confused with the hybrid PRX installer.

The patch investigation is tracked in [PKG_PATCH_UPDATE_INVESTIGATION.md](PKG_PATCH_UPDATE_INVESTIGATION.md). A patch candidate must not be called installable until its GP4 metadata, PARAM.SFO category, base association, version relationship, package validation, and independent QA have all passed. A PC-only package validation still does not prove console installation.

## Installer behavior

The installer validates the release manifest and PRX SHA-256 before modifying persistent paths. Managed-path inspection uses `open(O_NOFOLLOW)` and `fstat()` for file-type bits only. Staging files are reserved exclusively with `O_CREAT | O_EXCL`, avoiding stale temporary-name collisions without deleting old temporary files or backups.

The installer supports initial installation and plugin update through the same full/base PKG. This is distinct from updating the application itself through Package Manager.

After a successful plugin installation, restarting GoldHEN or rebooting allows the loader to attempt to load the PRX. The GoldHEN Plugin Loader option must be enabled in GoldHEN settings. Package installation, eboot execution, PRX activation and reboot remain separate operations.

The package currently installs the diagnostics/capture scaffold only. It does not provide a complete neural inference runtime or a generic game framebuffer adapter. A successful package build or file copy is not proof that the PRX loads or that neural rendering works on hardware.

Troubleshooting must capture `/data/GoldHEN/ps4_neural_engine_install.log` after the latest launch attempt; older log contents do not describe a newer attempt. Preserve existing plugin registrations, backups, stale temporaries, crash dumps and unrelated files.
