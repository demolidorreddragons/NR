# PKG 013 — GNM export-only preflight

Date: 2026-11-22  
Target branch: `feature/gcn8-numeric-kernels`  
Release: Full/Base `gde`, TITLE_ID `PNEU00001`, APP_VER `01.08`, package number `013`.

## Purpose

Combine the proven read-only release-manifest/PRX integrity check with a process-local audit of the GNM runtime module. The app locates or loads `libSceGnmDriver.sprx` and resolves five exports through `sceKernelDlsym`. The returned addresses are checked for non-null and discarded.

## Safety contract

- No GNM function is invoked.
- No command buffer is submitted and no GPU dispatch is attempted.
- The Neural Rendering PRX bundled in app0 is not copied, loaded, or activated.
- No `plugins.ini`, GoldHEN setting, persistent system file, or user data is modified.
- The GNM system module is loaded only into the app process if it is not already present.
- `abi_status=unverified`; `hardware_status=not_tested`.

## What PASS means

`PNE preflight PASS. GNM exports 5/5; no GNM calls; NR PRX NOT activated.`

This proves only that the five named symbols were resolved in this process. It does not prove that their function signatures are callable, that GPU memory mapping or cache policy is correct, or that compute dispatch works.

## Historical evidence

- PKG 012 installed package read-back matched SHA-256 `240914f39c1dbfec39b0f21fdd022dd30f10356341860e47fffaacef3aa3825f`.
- The prior dynamic GNM export probe `45040217180` resolved all five exports without invoking them.
- The older GNM-Link-only probe `44873622760` observed a null static import address.
- No coredump was present in the read-only collection for PKG 012. The historical CE-34878-0 report remains tied to the earlier experimental RuntimeFixed/compute path and is not represented as fixed by this preflight.

## Build, QA and staging result

- Build + independent QA run: `45069773465`, SUCCESS; commit `bea3b39df248ed71c7b13064a6a402dea800a32a`.
- PKG: `PS4-Neural-Engine-013.pkg`; 6,619,136 bytes; SHA-256 `73c4c9a43002ae55fadec532a9e97217d23e1742d5f4db2bed8a60abdec6f7d1`.
- Independent package validation/extraction/SFO/GP4/manifest/payload QA: PASS; `QA_STATUS=PASS_PC_ONLY_NOT_HARDWARE_TESTED`.
- Staging run: `45070164574`, SUCCESS.
- Final PS4 path: `/data/pkg/PS4-Neural-Engine-013.pkg`; remote read-back size and SHA-256 match the artifact.
- PKG 012 archived intact at `/data/pkg/old/PS4-Neural-Engine-012.pkg`; exactly one Neural Rendering PKG remains in the staging root.
- `PNEU00001` remains installed. Installation was not performed.

## Manual installation handoff

If the user chooses to test PKG 013, the existing app with TITLE_ID `PNEU00001` must first be removed manually (consider saved app data). Then use **Configurações → GoldHEN → Package Installer**, select `PS4-Neural-Engine-013.pkg` from `/data/pkg/`, wait for completion, and launch **PS4 Neural Engine Preflight** from the content menu. Report the exact notification or crash code. Do not activate the PRX, dispatch compute, or alter GoldHEN configuration.

The package remains a diagnostic preflight only; QA is PC-only and hardware status is not tested.


## PKG 013 post-install read-only evidence — 2026-11-22

- Evidence workflow 45070687025, job 137356718064: SUCCESS; artifact ps4-postinstall-preflight-evidence-013, ID 14772794263, 13,184,639 bytes; ZIP SHA-256 64443bf504c2af606be9392138c4bb2801273869e7496b3ecfdcd37598a25e99.
- The runner read back /user/app/PNEU00001/app.pkg: 6,619,136 bytes, SHA-256 73c4c9a43002ae55fadec532a9e97217d23e1742d5f4db2bed8a60abdec6f7d1, exactly matching PKG 013.
- /data/sce_coredumps/ listing contained only . and ..; no coredump was present at collection time.
- /data/sce_logs/log.bin was preserved as a 16 MiB binary, SHA-256 acc3459fca68cb26e456a7b31def600928a152546de2c08fd4ed6db25972eb82. Its content did not yield usable printable NR/GNM diagnostics in the current pass; it is not interpreted as proof of no system errors.
- The two expected PKG 013-specific text logs were absent (FTP 550): /data/pne_gcn8_gnm_export_audit_013.log and /data/pne_gnm_export_preflight_013.log. The user's reported PASS remains user-provided; it was not independently confirmed from those missing logs.
- Historical probe logs were recovered: static import probe had a null address; dynamic load resolved sceGnmSubmitDone; expanded audit resolved all five exports. Those are historical probes, not PKG 013 execution logs.
- Read-only collection used LIST/GET only; no remote files modified, no PRX activation, no plugins.ini change.

