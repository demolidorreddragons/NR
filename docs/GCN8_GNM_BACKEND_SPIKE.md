# GCN8 GNM backend spike — API audit

Status: **PSBC GFX8 host compilation and five shader container/ISA artifacts pass; no GNM runtime backend or hardware validation.**

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
compilation and lists GFX8/Polaris as a supported target. Its repository head
was audited and pinned for evaluation at commit
`28697f007862427ebec54388ec972ce6e3545912` (MIT). The CLI documents compute
compilation with `-s compute`; `-n` selects PS4 Pro/GFX8. A candidate command
for the existing tiled shader is therefore:

```sh
opengnm-psbc -s compute -f platform/ps4/gcn8/shaders/gemm_fp32_wave64_tiled.spv \\
  -o dist/ps4/gcn8/gemm_fp32_wave64_tiled.sb -n
```

This is a command template, not a successful build result. The pinned compiler
has a large vendored Mesa/NIR/ACO build and requires GNU Make, C11/C++17
compilers, Python 3 with Mako, and its sibling `opengnm`/`SPIRV-Headers`
sources. NR does not yet provision those dependencies or execute this compiler
in CI. The next reproducible step is to pin those source revisions and add a
host build + shader-binary verification job before attempting runtime use.

## Backend boundary to implement

The first hardware backend should own these operations explicitly:

1. Allocate and map GPU-visible A/B/C/output buffers with documented alignment
   and memory-type requirements.
2. Upload input matrices and initialize residual/output storage.
3. Load and validate the GFX8 compute shader binary and its register metadata.
4. Bind the CS stage and map each SPIR-V storage binding using the actual
   shader input-usage metadata. The first successful GFX8 artifact identifies
   one `PTR_INDIRECTRESOURCETABLE` pointer at user register 2. Pinned PSBC source
   explicitly defines storage buffers as 16-byte V# entries in set 0 at
   binding offsets N×64; the four GEMM bindings occupy offsets 0/64/128/192.
   The pointer-user-data setter is the candidate for the table pointer, not
   four direct V# user-data writes. Writable descriptor fields, GPU VA
   constraints and cache policy still require validation.
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
- [x] Build a minimal PS4 PRX link-smoke using the typed GNM declarations (run `44673126332`; compile/link only).
- [x] Pin an audited `opengnm-psbc` revision and document its GFX8 CLI/build
      prerequisites (`28697f007862427ebec54388ec972ce6e3545912`).
- [x] Provision the pinned compiler dependencies and build `opengnm-psbc` reproducibly in CI (run `44739871394`).
- [x] Compile the existing tiled shader to a GFX8 GNM binary; inspect CS metadata and resource usage (compiler-side only; run `44739871394`).
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

The compile/link smoke checks function-pointer signatures and the sizes
of `GnmBuffer`, `GnmCommandBuffer`, `GnmCsStageRegisters`, and `GnmCsShader`
against the pinned headers. `GnmBuffer` is a 16-byte V# descriptor; its
presence does not prove it is the correct descriptor type for the compiler's
RW storage-buffer lowering. The link stage attempts to resolve the two firmware imports against the
pinned OpenOrbis libraries. A successful link would establish import-library
resolution only, not that the functions are safe to call on a given firmware.
Neither function is invoked.
No external runtime code has been vendored, no firmware function was called,
and no console state was changed.


## CI evidence update — 2026-11-20

The PS4 PRX/PKG workflow now completes the GNM API/ABI compile smoke and the
firmware-import link smoke. Run
[44673126332](https://github.com/demolidorreddragons/NR/actions/runs/44673126332)
completed successfully on commit `df22fe1453cdb98b7fd9d6f7bf6388e3b614968e`.
The earlier link failure was caused by referencing the GoldHEN SDK's
`build/crtprx.o` before that object existed. The smoke ELF now links the
GNM import library without that unrelated startup object.

This verifies compile-time declarations and link-time import resolution only.
It does not prove firmware runtime availability on a particular console,
valid resource binding, command submission, synchronization, or GPU execution.

A separate workflow, `.github/workflows/gcn8-shader-build.yml`, pins `opengnm-psbc` and its header dependencies, builds the host compiler, compiles all five NR compute shaders for GFX8/NEO, validates all five containers and CRCs, checks the indirect-resource-table pointer, and disassembles the five GCN code ranges. Run `44739871394` passed; artifact details are in `docs/GCN8_GFX8_ARTIFACT_44739871394.md`. This is host/compiler evidence, not a runtime dispatch.
