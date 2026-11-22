# PKG 012 read-only preflight disposition

**Date:** 2026-11-22  
**Repository:** `demolidorreddragons/NR`  
**Branch:** `feature/gcn8-numeric-kernels`  
**Supersedes:** PKG 011 as an installation candidate  
**Scope:** prevent an ordinary app launch from deploying or activating the GoldHEN PRX.

## Finding in PKG 011

The PKG 011 entry point linked `installer/main.c`. Its `main()` copied the PRX into `/data/GoldHEN/plugins`, created/modified `/data/GoldHEN/plugins.ini`, retained backups, and registered the PRX in GoldHEN's `[default]` section. Consequently, installing and launching the app could modify system/plugin state and arrange PRX loading without a separate, explicit activation decision.

That behavior conflicts with the project's authorization boundary: package installation, PRX deployment, and PRX activation are separate operations. PKG 011 therefore remains diagnostic and must not be installed.

## PKG 012 change

The package builder now links `installer/preflight_main.c`, not the legacy mutating installer. The new entry point:

1. Reads `/app0/pne-release.ini` and `/app0/ps4_neural_engine.prx`.
2. Verifies release identity and PRX SHA-256 through the existing manifest validator.
3. Displays a notification with the result.
4. Exits without writing to `/data`, copying the PRX, editing `plugins.ini`, loading a module, starting a worker, or restarting GoldHEN.

The old mutating implementation is retained in `installer/main.c` for audit/history only and is no longer linked by `tools/build_pkg.sh`. `installer_version=2` identifies the read-only preflight behavior. The user-facing SFO title is now **PS4 Neural Engine Preflight**.

The bundled PRX remains fail-closed: `has_validated_title_adapter()` returns false, so it does not install a GNM hook or start the plugin worker. This does not make the PRX ABI or runtime validated.

## Required verification

- Host source-contract test: `tests/test_installer_preflight_contract.py`.
- Host CI runs that test before the normal build/test suite.
- PRX/PKG build must succeed on the exact resulting commit.
- Independent PKG QA must validate the resulting PKG, embedded SFO, GP4 association, manifest and exact PRX hash.
- Inspect the final artifact and QA report; do not reuse PKG 011 hashes or reports.
- Hardware validation remains outstanding. A successful PC build/QA is not proof of PS4 installation or execution.

## Build, independent QA and PS4 staging result

- Exact build run: `45062759996`, commit `2b011b16f095ab48a6e00d03c8a0eab38855c464`, overall SUCCESS.
- Independent QA job: SUCCESS. It validated the GP4/PKG structure, embedded SFO, manifest, PRX hash and both diagnostic smoke packages. Report status: `PASS_PC_ONLY_NOT_HARDWARE_TESTED`.
- Exact artifact: `PS4-Neural-Engine-012.pkg`, 6,619,136 bytes.
- PKG SHA-256: `240914f39c1dbfec39b0f21fdd022dd30f10356341860e47fffaacef3aa3825f`.
- PRX SHA-256: `1e4531711a0768cfeb29f1d6f66b9541cf087b141eda7a05fed755d12507866c`.
- PS4 staging run: `45063363596`, SUCCESS. Remote temporary upload was read back and matched before promotion. Final read-back at `/data/pkg/PS4-Neural-Engine-012.pkg` matched exact byte count and SHA-256.
- Previous root package archived with verified read-back: `/data/pkg/old/PS4-Neural-Rendering-GCN8-gnm-load-45040217180.pkg`, 6,619,136 bytes, SHA-256 `003baaa305dc6c6c9df20f0d383eba183ce795b873ead6f0f5e59a2d24b7383c`.
- Root directory check: only `PS4-Neural-Engine-012.pkg` remains as a Neural Rendering PKG.
- Read-only installed-title check: `PNEU00001` is PRESENT under `/user/app/`. Because PKG 012 is Full/Base, it must not be installed over that existing title. Any removal is manual and user-controlled.
- `INSTALLATION_PERFORMED=FALSE`; no PRX deployment, activation, `plugins.ini` change or GoldHEN restart occurred.

## Authorization and release disposition

PKG 012 is intended only as a **controlled installation/preflight test candidate** after its exact build and independent package QA pass. Installing it does not authorize PRX deployment or activation; the app is intentionally incapable of either. Any later PRX deployment, `plugins.ini` change, GoldHEN restart, or activation requires its own explicit user authorization.

No console installation, PRX deployment, plugin activation, `plugins.ini` edit, or GoldHEN restart is performed by this change.
