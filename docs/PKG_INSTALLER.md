# PS4 PKG installer

## Current release state — PKG 013 staged (2026-11-22)

Release 013 is a **Full/Base** application package (`CATEGORY=gde`), not a patch. It upgrades PKG 012 with a process-local GNM runtime export audit:

- Validates `/app0/pne-release.ini` against the bundled PRX SHA-256.
- Locates or loads `libSceGnmDriver.sprx` into the app process and resolves five exports with `sceKernelDlsym`: `sceGnmSubmitDone`, `sceGnmDispatchInitDefaultHardwareState`, `sceGnmFlushGarlic`, `sceGnmSubmitCommandBuffers`, and `sceGnmSubmitAndFlipCommandBuffers`.
- Never invokes a GNM export, submits command buffers, dispatches a shader, installs/activates the Neural Rendering PRX, edits GoldHEN configuration, or changes persistent system configuration.
- PASS means only that the five symbols were resolved. It does not validate function-call ABI, GPU memory, cache policy, command submission, shader execution, or Neural Rendering.
- Manifest remains `abi_status=unverified` and `hardware_status=not_tested`.

### Build and independent QA

- Build + independent package QA: run `45069773465`, SUCCESS; source commit `bea3b39df248ed71c7b13064a6a402dea800a32a`.
- Independent PKG inspector: PKG validation PASS; extraction PASS; SFO semantic fields PASS; GP4/manifest/payload association PASS.
- QA report: `QA_STATUS=PASS_PC_ONLY_NOT_HARDWARE_TESTED`.
- PKG: `PS4-Neural-Engine-013.pkg`, 6,619,136 bytes.
- SHA-256: `73c4c9a43002ae55fadec532a9e97217d23e1742d5f4db2bed8a60abdec6f7d1`.
- PRX SHA-256 inside package: `1e4531711a0768cfeb29f1d6f66b9541cf087b141eda7a05fed755d12507866c`.

### PS4 staging/read-back

- Staging workflow: run `45070164574`, SUCCESS.
- Final path: `/data/pkg/PS4-Neural-Engine-013.pkg`.
- Final remote read-back: 6,619,136 bytes; SHA-256 exactly matches the approved build artifact.
- Previous PKG 012 archived intact at `/data/pkg/old/PS4-Neural-Engine-012.pkg`; 6,619,136 bytes; SHA-256 `240914f39c1dbfec39b0f21fdd022dd30f10356341860e47fffaacef3aa3825f`.
- Exactly one Neural Rendering PKG remains in the `/data/pkg/` root: PKG 013.
- `TITLE_ID=PNEU00001` is still installed. PKG 013 is Full/Base with the same TITLE_ID, so the user must manually remove the existing app before installing 013, considering any saved app data. The assistant did not uninstall it.
- `INSTALLATION_PERFORMED=FALSE`. Staging is complete; installation and all console execution remain separate and have not been performed.

### Manual installation handoff

Only after the user decides to install:
1. On the PS4, manually remove the existing Neural Engine app with TITLE_ID `PNEU00001` if a clean install is required. Preserve any app data the user wants to keep.
2. Open **Configurações → GoldHEN → Package Installer** (or the Package Installer available in the installed environment).
3. Select `PS4-Neural-Engine-013.pkg` from `/data/pkg/` and confirm installation.
4. Wait for completion, then launch **PS4 Neural Engine Preflight** from the PS4 content menu.
5. Report the exact notification shown (PASS/FAIL) and any crash code. Do not enable/activate the PRX or alter GoldHEN configuration.

PKG 013 is a diagnostic preflight only. It must not be described as a Neural Rendering runtime release or as a fix for CE-34878-0. No PRX activation, compute dispatch, or GPU validation is authorized by this staging.

### PKG 012 evidence and CE-34878-0 distinction

The user's PKG 012 installation is confirmed by the installed `/user/app/PNEU00001/app.pkg` read-back: 6,619,136 bytes and SHA-256 `240914f39c1dbfec39b0f21fdd022dd30f10356341860e47fffaacef3aa3825f`, exactly matching its build artifact. The user reported the preflight PASS notification.

The expanded read-only collection (workflow `45069491003`) found no coredumps in `/data/sce_coredumps/`. The old RuntimeFixed package remains as a file under `/data/`, SHA-256 `a39ef4e64493b97b9551c287a82e820b9335cc35460f63b4c37bc3027972305b`; it is not the currently installed PNEU00001 package. The CE-34878-0 report remains associated with the earlier experimental RuntimeFixed/compute path, not with a reproduced PKG 012 preflight crash.

The collected prior GNM logs show:
- GNM-Link-only probe `44873622760`: static import address was null; no GNM call was made.
- GNM-Load-only probe `44876293567`: dynamic module load and `sceGnmSubmitDone` DLSYM succeeded.
- Expanded export-audit probe `45040217180`: all five required exports resolved; no GNM call or compute dispatch was made.

These observations confirm that the old link-only probe had a null static import and the later dynamic lookup probe resolved the exports. They do **not** establish the root cause of CE-34878-0. The new integrated PKG 013 app still requires user execution to validate its own startup path; it does not clear the compute hardware gates.

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
