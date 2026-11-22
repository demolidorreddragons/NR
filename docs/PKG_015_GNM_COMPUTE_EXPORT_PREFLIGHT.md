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
