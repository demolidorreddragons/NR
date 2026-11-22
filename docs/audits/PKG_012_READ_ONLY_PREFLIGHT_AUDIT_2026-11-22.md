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

## Authorization and release disposition

PKG 012 is intended only as a **controlled installation/preflight test candidate** after its exact build and independent package QA pass. Installing it does not authorize PRX deployment or activation; the app is intentionally incapable of either. Any later PRX deployment, `plugins.ini` change, GoldHEN restart, or activation requires its own explicit user authorization.

No console installation, PRX deployment, plugin activation, `plugins.ini` edit, or GoldHEN restart is performed by this change.
