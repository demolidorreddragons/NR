# GNM compute runtime specialist task — PS4 Pro GFX8

Workstream: GNM backend (C/C++) under issue #4. First milestone is a reversible compute smoke test, not neural inference.

## Required sequence
1. Inspect and pin exact upstream OpenGNM and opengnm-psbc commits; verify licenses, APIs, toolchain compatibility, and existing build scripts. Do not rely on unpinned upstream main.
2. Implement a PS4-side executable/PRX test harness with explicit GPU buffer allocation, CPU upload, compute shader binding, descriptor/resource table setup, dispatch, required cache operations/barriers, completion fence/EOP, GPU-to-CPU readback, and deterministic output comparison.
3. Start with the smallest buffer A -> shader -> buffer B kernel (e.g. elementwise add/copy with known answer). Do not start with tiled GEMM.
4. Add host-side unit tests for descriptor encoding, address/range/alignment validation, overflow, resource lifetime and dispatch geometry. Clearly separate host-only contract tests from actual Orbis/GNM runtime calls.
5. Compile shaders for PS4 Pro GFX8 and independently inspect the binary/container, including input-usage slots and shader metadata. Resolve issue #8 before accepting affected artifacts; never mask/truncate an 8-bit offset.
6. Add a hardware test procedure that is reversible and writes only to a unique temporary test allocation/file. Do not edit plugins.ini, overwrite canonical PRX, install PKG, activate plugins, or touch saves/app data.
7. Keep compute_available false unless the real PS4 Pro test returns the exact expected output and passes repeated stability checks.

## Environment target
PS4 Pro / firmware 14.02 / GoldHEN 2.4b18.14. The runner may have LAN access, but never infer console access from a successful CI build. Confirm actual hardware access from logs before claiming it.

## Deliverables
- Compilable backend source with pinned dependencies.
- Reproducible clean build and host tests.
- PS4 smoke-test binary and exact deployment/test instructions, separate from installer/release packaging.
- Logs with shader hash, GPU target, allocation sizes, dispatch dimensions, fence/readback evidence and expected/actual output.
- A concise ADR documenting GNM memory, descriptor, barrier and lifetime decisions.

## Acceptance
A green host build or shader compile is not a hardware pass. Mark unavailable hardware evidence BLOCKED. No full neural graph integration until the smoke test is independently accepted.
