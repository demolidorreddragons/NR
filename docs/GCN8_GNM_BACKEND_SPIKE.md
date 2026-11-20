# GCN8 GNM backend spike — API audit

Status: **API discovery only; no GNM backend is implemented or hardware-validated.**

## Findings

The current PRX build already links `-lSceGnmDriver`, but the pinned
OpenOrbis 0.5.4 SDK's public `include/orbis/GnmDriver.h` exposes several
compute entry points as parameterless declarations (for example
`sceGnmDispatchDirect()`). Those declarations are not sufficient to safely
call the command-buffer API from this project. Do not cast these declarations
to guessed function-pointer signatures.

A candidate typed GNM interface was identified in
[PS4-OpenGNM/opengnm](https://github.com/PS4-OpenGNM/opengnm), pinned for
evaluation at commit
`4b295ca54c82c83acf308d1c646a2dfa9ae57350` (MIT license). Its public headers
include:

- `include/gnmdriver.h`: typed runtime declarations, including
  `sceGnmSetCsShader`, `sceGnmDispatchDirect`, and command submission.
- `include/gnm_drawcommandbuffer.h` and `include/gnm_commandbuffer.h`:
  command-buffer helpers and dispatch command emission.
- `include/gnm_shaderbinary.h`: shader binary container definitions.
- `include/gnm_helpers.h`: helper APIs, including direct-memory utilities.

The pinned headers provide two layers: the low-level runtime declarations in
`gnmdriver.h` and typed command-buffer helpers in
`gnm_drawcommandbuffer.h`. The latter exposes `sceGnmDrawCmdSetCsShader`,
`sceGnmDrawCmdSetVsharpUserData`, `sceGnmDrawCmdSetPointerUserData`, and
`sceGnmDrawCmdDispatchDirect`. These are enough to type-check a first command
recording path, but not to claim a working compute backend. Dispatch only emits
a packet; memory allocation, descriptor construction, shader register
metadata, user-data slot mapping, command submission, synchronization, and
completion handling remain separate responsibilities.

The companion `opengnm-psbc` project documents SPIR-V-to-GNM shader binary
compilation and lists GFX8/Polaris as a supported target. This is a plausible
replacement path for the missing shader compiler, but the current NR workflow
does not yet pin or execute that compiler.

## Backend boundary to implement

The first hardware backend should own these operations explicitly:

1. Allocate and map GPU-visible A/B/C/output buffers with documented alignment
   and memory-type requirements.
2. Upload input matrices and initialize residual/output storage.
3. Load and validate the GFX8 compute shader binary and its register metadata.
4. Bind the CS stage and all four storage-buffer resources.
5. Bind the 20-byte parameter block using the actual GNM user-data/constant
   mechanism; the current host struct size alone does not prove this ABI.
6. Emit dispatch dimensions from `GemmDispatch.groups_x/groups_y`, with the
   shader's local size 8x8 (64 invocations per group).
7. Emit the required cache/UAV synchronization, submit the command buffer,
   wait for completion, and read back output.
8. Validate output against the host double-precision reference, including
   odd edge dimensions and `beta=0`.

Do not equate the OpenOrbis declaration, a successful link, or a host PM4
emission test with a successful PS4 Pro compute dispatch.

## Integration gates

- [x] Pin the exact opengnm header revision and compile a PS4-targeted,
      compile-only API/ABI smoke test (`tests/gcn8_gnm_api_smoke.cpp`).
- [ ] Audit runtime symbol availability, firmware delegation, and link behavior;
      header declarations alone do not prove exports.
- [ ] Build a minimal PS4 PRX link-smoke using the typed GNM declarations.
- [ ] Compile the existing tiled shader to a GFX8 GNM binary with a pinned
      opengnm-psbc revision; inspect CS metadata and resource usage.
- [ ] Implement buffer allocation, resource binding, user-data constants,
      command submission, synchronization, and readback.
- [ ] Run a no-op/known-answer compute smoke test on PS4 Pro before GEMM.
- [ ] Run GEMM readback tests and collect GPU timestamps plus numerical error.

## Sources reviewed

- OpenOrbis PS4 Toolchain `GnmDriver.h`:
  https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/include/orbis/GnmDriver.h
- opengnm API and backend overview:
  https://github.com/PS4-OpenGNM/opengnm
- opengnm driver runtime reference:
  https://ps4-opengnm.github.io/opengnm/reference/driver/
- opengnm shader binary guide:
  https://ps4-opengnm.github.io/opengnm/guides/shaders/
- opengnm-psbc compiler:
  https://github.com/PS4-OpenGNM/opengnm-psbc

The compile-only smoke test checks function-pointer signatures and the sizes
of `GnmBuffer`, `GnmCommandBuffer`, `GnmCsStageRegisters`, and `GnmCsShader`
against the pinned headers. It does not link against or call firmware exports.
No external runtime code has been vendored, no firmware function was called,
and no console state was changed.
