# REA specialists — GCN8 unblock team

Updated: 2026-11-20  
Branch: `feature/gcn8-numeric-kernels`

## Added specialist responsibilities

These are additional specialist workstreams in the project team, not claims that separate autonomous agents or external contractors have been hired.

### REA-1 — Mesa / PSBC compiler reverse engineering

**Mission:** get the pinned `opengnm-psbc` host compiler to build reproducibly, then establish the exact SPIR-V-to-GFX8 lowering and binary metadata contract.

Tasks:
1. Trace every generated Mesa header/source dependency reached by the PSBC Makefile.
2. Add deterministic generation steps to CI; never commit generated vendor output unless the upstream build requires it.
3. Keep the PSBC, Mesa-derived source, OpenGNM and SPIR-V dependency revisions pinned.
4. Capture the first compiler failure and fix one reproducible blocker at a time.
5. Once the compiler builds, compile the five NR compute shaders and inspect the actual GNM binary metadata: stage, registers, resource slots, user SGPRs, wave mode and resource descriptor class.
6. Reject any conclusion that is not supported by the emitted binary or compiler source.

### REA-2 — GCN8 ISA / GNM binary and resource ABI

**Mission:** independently reverse engineer the GFX8 compute binary contract and connect it to the Orbis GNM command path without assuming Vulkan semantics map directly.

Tasks:
1. Trace PSBC's GFX8 code-generation path and the GNM shader-container format.
2. Identify the exact CS register metadata and dispatch geometry expected by the compiled binary.
3. Determine whether each shader resource is a V# buffer descriptor, RW# image/buffer descriptor, or another supported form; do not infer this from GLSL `std430`.
4. Cross-check descriptor encoding, alignment, address units and resource slots against pinned OpenGNM sources and actual compiler output.
5. Produce a minimal compute bring-up contract: allocation, GPU VA, mapping, cache ownership, command recording, submit, completion and error recovery.
6. Keep hardware-only claims marked blocked until a reversible PS4 Pro dispatch is actually observed.

## First blocker found and corrective change

The shader compiler CI run `44674071326` failed while building pinned PSBC at:

```
src/util/format/u_formats.h:33:10:
fatal error: util/format/u_format_gen.h: No such file or directory
```

This is a generated Mesa header, not a missing GCN8 tool binary. Mesa's own build definitions generate it by running `src/util/format/u_format_table.py <format-description> --enums` with stdout captured as `u_format_gen.h`.

Corrective commit `2c83ae4334029f75a4f62c8bfd9778c4a0a64f34` adds an explicit deterministic generation step before `make` in `.github/workflows/gcn8-shader-build.yml`. The next CI run must confirm that this fixes the first failure; subsequent generated-file or dependency failures must be handled from their actual logs.

## Acceptance gates

- **Compiler gate:** pinned PSBC builds successfully from a clean CI runner.
- **Shader gate:** all five SPIR-V inputs compile to GFX8 GNM compute binaries and pass container validation.
- **Metadata gate:** actual emitted register/resource metadata is decoded and reviewed; container validity alone is insufficient.
- **Runtime gate:** a minimal, reversible GNM dispatch completes on PS4 Pro and returns a verified result.
- **Performance gate:** only then collect GPU latency, occupancy/register and throughput measurements.

No source/host test, compiler build, or container check by itself proves execution on PS4 Pro. No PKG installation, plugin activation, or persistent console modification is authorized by this workstream.
