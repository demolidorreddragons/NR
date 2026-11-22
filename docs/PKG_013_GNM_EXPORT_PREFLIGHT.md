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

## Release gate

Do not stage for user installation unless host tests, target compilation, package validation, independent PKG QA, and artifact hash checks all pass. Staging is separate from installation. The user must manually remove the previously installed `PNEU00001` Full/Base app before installing a new Full/Base package with the same TITLE_ID. No automatic installation or PRX activation is authorized.
