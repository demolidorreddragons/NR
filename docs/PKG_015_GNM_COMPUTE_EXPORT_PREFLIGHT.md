# PKG 015 — GNM compute export preflight

Date: 2026-11-22  
Branch: `feature/gcn8-numeric-kernels`  
Release: Full/Base `gde`, TITLE_ID `PNEU00001`, APP_VER `01.10`, package number `015`.

## Finding addressed

The PKG 014 audit resolved five symbols, but that set was insufficient to establish that the compute-specific shader-bind and direct-dispatch entry points were present. The compute path needs at least the CS bind and dispatch emitters in addition to default compute-state initialization and command-buffer submission.

PKG 015 extends the process-local symbol audit to seven exports:

1. `sceGnmSubmitDone`
2. `sceGnmDispatchInitDefaultHardwareState`
3. `sceGnmSetCsShader`
4. `sceGnmDispatchDirect`
5. `sceGnmFlushGarlic`
6. `sceGnmSubmitCommandBuffers`
7. `sceGnmSubmitAndFlipCommandBuffers`

The signatures for the CS bind, direct dispatch, default-state initialization and submit APIs are cross-checked against the pinned OpenGNM source and its driver API reference: https://ps4-opengnm.github.io/opengnm/reference/driver/ . This is third-party reverse-engineered/API-compatibility evidence, not Sony firmware ABI confirmation for FW 14.02.

## Safety contract

- The app still performs symbol resolution only. DLSYM addresses are checked for non-null and discarded.
- No GNM export is called.
- No command buffer is built/submitted and no shader is dispatched.
- No GPU allocation, GPU VA mapping, cache operation, synchronization or readback is attempted.
- The six GFX8 binaries are hash-verified as package files only.
- The Neural Rendering PRX is not activated.
- No GoldHEN configuration, persistent system file or user data is modified.
- Manifest remains `abi_status=unverified` and `hardware_status=not_tested`.

## What PASS means

`PNE preflight PASS. GCN8 assets 6/6; GNM compute exports 7/7; no GNM calls; NR PRX NOT activated.`

This confirms the two missing compute-specific symbols are discoverable in the process. It does not prove their call ABI, packet validity, GPU execution, firmware compatibility, render-target capture, output correctness, or a working neural renderer.

## Next technical gates

1. Verify FW 14.02 ABI and actual retail behavior for the seven required exports, including the CS register block and dispatch packet contract.
2. Establish a valid GPU-visible memory allocation/VA and mapping contract for input/output buffers.
3. Validate GFX8 shader metadata, wave size, resource descriptors, V# formats and input-usage offsets.
4. Define GPU completion, cache coherency, readback and safe resource lifetime.
5. Integrate a title-specific frame source/consumer adapter; the generic GoldHEN hook remains disabled.
6. Only after the above gates are supported by evidence, add a separately gated compute smoke path. Do not enable unvalidated GPU submission merely to obtain a PASS.

PKG 015 is a more complete **compute-export preflight**, not a functioning neural-rendering runtime. PC/CI QA cannot substitute for a PS4 Pro hardware test.

## Build, independent QA and PS4 staging evidence

- Build workflow: `45074310271`, SUCCESS; source commit `2c9e3c12aed2791f2973496c31c8993259c64430`.
- Build artifact: `ps4-neural-engine-build`, artifact ID `14774258357`, 31,951,024 bytes; ZIP SHA-256 `f149ba8f9f64385dbb28cd65456ab9894c7880bc11086d374cac603fb3f319b2`.
- PKG: `PS4-Neural-Engine-015.pkg`, Full/Base (`gde`), TITLE_ID `PNEU00001`, APP_VER `01.10`, 6,619,136 bytes; SHA-256 `2fcaf679766fefbed4bc60cc31beb0227b8bfefd4eb8070160f65a402be747e9`.
- Independent QA job: `independent-pkg-qa`, SUCCESS. QA artifact ID `14774516745`, 5,531 bytes; ZIP SHA-256 `107805ce1eae694b91468c363fab00706f4e518a52b9f6c4aef1dc8c355cea28`.
- Independent inspector: PKG header, outer/inner PFS, digests, signatures and filesystem walk PASS. PARAM.SFO semantic fields PASS; CATEGORY=gde, APP_VER/VERSION=01.10, TITLE_ID and CONTENT_ID match. PRX SHA-256 `1e4531711a0768cfeb29f1d6f66b9541cf087b141eda7a05fed755d12507866c`. Six GFX8 assets 6/6 hashes PASS. QA status: `PASS_PC_ONLY_NOT_HARDWARE_TESTED`.
- Host tests: workflow `45074310300`, SUCCESS.
- PS4 staging: workflow `45074626193`, job `137368991264`, SUCCESS. PKG 014 was preserved at `/data/pkg/old/PS4-Neural-Engine-014.pkg`, 6,619,136 bytes, SHA-256 `96c15b46e7579c5f728a131279e986af04a9e4683c9903221e418ca52a3842db`. PKG 015 temporary upload readback and final readback both matched 6,619,136 bytes and SHA-256 `2fcaf679766fefbed4bc60cc31beb0227b8bfefd4eb8070160f65a402be747e9`.
- Final PS4 path: `/data/pkg/PS4-Neural-Engine-015.pkg`. Exactly one Neural Rendering PKG remained in the root. Existing installed TITLE_ID `PNEU00001` was present. No installation, PRX activation, GNM call, plugins.ini change or GoldHEN restart was performed.

The package is staged and hash-verified, but it remains a compute-export diagnostic preflight. It is not a functional neural-rendering runtime; ABI, dispatch, GPU memory, synchronization, capture adapter and hardware gates remain open.
