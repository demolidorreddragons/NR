# PS4 PKG installer

## Current release state — PKG 013 candidate (2026-11-22)

Release 013 is being prepared as a **Full/Base** application package (`CATEGORY=gde`), not a patch. It upgrades the PKG 012 preflight with a process-local GNM runtime export audit:

- It validates `/app0/pne-release.ini` against the bundled PRX SHA-256.
- It locates or loads `libSceGnmDriver.sprx` into the app process, then resolves five required GNM exports with `sceKernelDlsym`: `sceGnmSubmitDone`, `sceGnmDispatchInitDefaultHardwareState`, `sceGnmFlushGarlic`, `sceGnmSubmitCommandBuffers`, and `sceGnmSubmitAndFlipCommandBuffers`.
- It never calls a GNM export, submits command buffers, dispatches a shader, activates the Neural Rendering PRX, edits GoldHEN plugin configuration, restarts GoldHEN, or changes persistent system configuration.
- A successful notification means only that the five symbol addresses were resolved in this app process. It does not validate ABI invocation, GPU memory, command submission, shader execution, or Neural Rendering.
- Manifest remains `abi_status=unverified` and `hardware_status=not_tested`.
- Build/independent QA/staging for 013 are pending in the current branch workflow. Do not install until the build and independent QA gates pass and staging/read-back is verified.
- The read-only audit confirms TITLE_ID `PNEU00001` is already installed. This is a clean-install Full/Base PKG with the same TITLE_ID; if 013 is eventually approved, the user must manually remove the previous app before installing it. The assistant must not uninstall it.

### PKG 012 evidence and CE-34878-0 distinction

The user's PKG 012 installation is confirmed by the installed `/user/app/PNEU00001/app.pkg` read-back: 6,619,136 bytes and SHA-256 `240914f39c1dbfec39b0f21fdd022dd30f10356341860e47fffaacef3aa3825f`, exactly matching the build artifact. The user reported the preflight PASS notification.

The expanded read-only collection (workflow `45069491003`) found no coredumps in `/data/sce_coredumps/`. The old RuntimeFixed package remains as a file under `/data/`, SHA-256 `a39ef4e64493b97b9551c287a82e820b9335cc35460f63b4c37bc3027972305b`; it is not the currently installed PNEU00001 package. The CE-34878-0 report remains associated with the earlier experimental RuntimeFixed/compute path, not with a reproduced PKG 012 preflight crash.

The collected prior GNM logs show:
- GNM-Link-only probe `44873622760`: static import address was null; no GNM call was made.
- GNM-Load-only probe `44876293567`: dynamic module load and `sceGnmSubmitDone` DLSYM succeeded.
- Expanded export-audit probe `45040217180`: all five required exports resolved; no GNM call or compute dispatch was made.

These observations narrow the earlier failure to the static import path and confirm the dynamic export lookup path on the prior diagnostic build. They do not validate the new integrated PKG 013 executable or clear the compute hardware gates.

### Historical release 012

Release 012 is a Full/Base application package (`CATEGORY=gde`), not a patch. It validated the manifest/PRX hash and displayed the preflight PASS notification; it did not load GNM or activate the PRX.
- Build and independent package QA: run `45062759996`, SUCCESS; PC-only.
- PS4 staging/read-back: run `45063363596`, SUCCESS.
- Size: 6,619,136 bytes; SHA-256 `240914f39c1dbfec39b0f21fdd022dd30f10356341860e47fffaacef3aa3825f`.

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
