# Static audit: startup-only probe and GNM link smoke

Date: 2026-11-21
Branch: `feature/gcn8-numeric-kernels`
Audited commit: `9e7989072bc23382eef33f67bc0a781b8e82d881`

## Scope and method

Source-level review of:
- `platform/ps4/gcn8/smoke_main.cpp`
- `tools/build_compute_smoke_pkg.sh`
- `.github/workflows/ps4-prx.yml`
- `platform/ps4/goldhen/plugin/Makefile`
- `platform/ps4/gcn8/compute_smoke.cpp`

CI job logs were inspected for run `44866020379`. This is not a hardware test, not a runtime import-resolution test on PS4, and not an independent review.

## Findings

### F-01 — Startup-only link path is isolated at source level

The `startup` branch compiles only `smoke_main.cpp` with `PNE_GCN8_STARTUP_ONLY=1`, links `-lc -lc++ -lkernel`, and omits `compute_smoke.o`, resource binding/contract objects, and `-lSceGnmDriver`. The preprocessor removes the call to `pne_gcn8_compute_smoke_run`.

The packaging script now omits `compute_smoke_add.sb` from the startup package and its extraction check explicitly rejects that file. The compute mode still requires and hashes the shader.

**Assessment:** source-level isolation is present. The updated startup/compute PKG artifacts still require the GCN8 shader workflow to complete and pass before the packaging correction is considered CI-approved.

### F-02 — Startup logging is best-effort, not guaranteed persistent

The probe attempts append access under `/user/data`, then `/data`. If both fail, it continues with stdout and reports `PNE_GCN8_LOG_OPEN=0`. Therefore a successful launch can still lack a persistent log.

**Assessment:** intentional fallback avoids making log-path availability a startup blocker. Console diagnosis must account for the possibility of no persistent file.

### F-03 — GNM link-smoke workflow has a weak success criterion

In run `44866020379`, the separate GNM API link-smoke command emitted:

```
ld.lld: warning: cannot find entry symbol _init; not setting start address
GCN8_GNM_LINK_SMOKE=resolved; no firmware calls or GPU dispatch executed
```

The workflow then checks only that the output ELF file is non-empty. The link command returned success despite the missing entry symbol, so the current `GCN8_GNM_LINK_SMOKE=resolved` marker overstates what was proven. It demonstrates that the linker produced an ELF while resolving the requested libraries sufficiently to complete, but it does not establish a valid executable entry point or runtime import behavior.

**Assessment:** QA improvement required. Either make this explicitly a relocatable/link-resolution artifact with no executable-entry claim, or provide a valid entry/CRT and assert the ELF entry point and expected import table. Do not treat this warning as a failure of the separate full PRX build.

### F-04 — Main PRX link is a separate successful build step, not console validation

The same CI job linked `dist/ps4/ps4_neural_engine.elf` and produced `ps4_neural_engine.prx`; the job then built PKG 011 and passed its internal PKG hash/signature and payload extraction checks. The main plugin Makefile links GNM and other PS4 system libraries, including HTTP/network modules.

**Assessment:** compile/package evidence only. No claim is made that every import resolves on firmware 14.02, that GoldHEN loads the PRX, or that the plugin executes safely on the console.

## Current gate state at audit time

- PRX/PKG build job `136729730621`: PASS.
- Independent PKG QA job `136730008149`: IN_PROGRESS at last poll; no independent PASS recorded.
- Host tests run `44866020415`: PASS.
- GCN8 shader build run `44866020361`: IN_PROGRESS at last poll; startup-only PKG correction not yet approved.
- PS4 hardware execution: NOT_RUN.

## Required next actions

1. Complete and inspect the GCN8 shader workflow, including both extracted smoke PKGs and manifests; verify startup package has no compute shader.
2. Complete independent PKG QA before promoting any artifact.
3. Correct the GNM link-smoke gate so its status matches the actual ELF/entry/import evidence.
4. Add binary-level `readelf` checks for the startup-only ELF's entry point and undefined/import symbols; source isolation alone is insufficient.
5. Keep all console execution blocked until the reversible plan is approved and the user explicitly authorizes the hardware operation.

## Decision

**PARTIAL PASS — STATIC SOURCE REVIEW ONLY.** Startup-only source isolation is present. Binary-level import proof, independent PKG QA completion, shader PKG correction CI, and all PS4 runtime claims remain pending.
