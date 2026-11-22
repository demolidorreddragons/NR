# HDE64 guard-page harness results — 2026-11-22

## Scope

This report records the host-side characterization of the pinned GoldHEN SDK HDE64 decoder and the test-only bounded adapter added to the Neural Rendering test harness.

- Neural Rendering repository: `demolidorreddragons/NR`
- Branch: `feature/gcn8-numeric-kernels`
- Tested project commit: `9cdca96f46b6ce3f2b3d75a60ced5c7bd421e9dd`
- CI workflow run: [host-tests #1501](https://github.com/demolidorreddragons/NR/actions/runs/45055134505)
- Run ID: `45055134505`
- Workflow result: `completed / success`
- Telemetry job ID: `137308561817` — `completed / success`
- PS4 evidence retrieval job ID: `137308561582` — `completed / success`
- GoldHEN SDK repository: `GoldHEN/GoldHEN_Plugins_SDK`
- Exact SDK revision checked out by CI: `bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`

## Reproduced raw-decoder faults

The harness placed each truncated byte sequence at the end of a readable page and protected the following page with `PROT_NONE`. Each raw `hde64_disasm()` call ran in a child process so the expected memory fault could be observed without terminating the test process.

All five expected guard-page faults were reproduced:

1. Truncated operand-size prefix: `66`.
2. Truncated REX prefix: `48`.
3. Truncated instruction before ModRM completion: `48 8B`.
4. Truncated RIP-relative displacement: `48 8B 05`.
5. Truncated `CALL rel32` immediate: `E8 11 22`.

The CI log emitted: `HDE64 raw characterization: five truncated-input guard-page faults reproduced.`

## Test-only bounded adapter

The harness also tests a local adapter that:

- rejects null input/output and zero available bytes;
- caps the copied instruction window at 15 bytes;
- copies only the caller-declared available bytes into a zero-initialized 32-byte local window;
- invokes the pinned upstream decoder on that padded local window;
- rejects zero length, decoded length greater than the supplied byte count, and decoder `F_ERROR` results.

Observed results:

- All five truncated windows were rejected.
- A valid `CALL rel32` was accepted with length 5 and `F_RELATIVE`.
- A valid RIP-relative `MOV` structural encoding was accepted with length 7 and the expected `F_DISP32`, ModRM mod=0, r/m=5 fields.

The CI log emitted: `Test-only bounded adapter: valid relative decodes pass; all five truncated windows rejected.`

### Critical limitation

This adapter is **test-only and is not integrated into production detour code**. The caller must already guarantee that the entire supplied byte count is genuinely readable. The adapter's `memcpy` reads that many bytes from the caller's pointer; it cannot establish pointer validity, mapping continuity, permissions, or lifetime. Zero padding contains the upstream decoder's speculative reads, while the post-decode length check rejects instructions that require bytes beyond the caller-supplied span. This characterization does not, by itself, prove safety against concurrent unmapping or mapping changes.

The valid RIP-relative case is only a structural classification check. It does not implement or validate trampoline relocation.

## CI verification

The complete workflow run finished successfully. The telemetry job's relevant results were:

- pinned SDK checkout at the exact revision above — success;
- HDE64 guard-page characterization and bounded-adapter test — success;
- installer state-machine tests — success;
- release manifest and SHA-256 integrity tests — success;
- stage logger host tests — success;
- GCN8 numeric reference and tiled mapping checks — success;
- GNM input-usage offset arithmetic tests — success;
- DLSS-NR tensor layout formula tests — success;
- model-input intake verifier — success;
- CMake configure and build — success;
- CTest — 9/9 passed.

The workflow's separate read-only PS4 evidence retrieval job also completed successfully. That job result is not evidence of runtime GPU execution or hardware validation.

## Impact and safety

- No production HDE64 decoder or detour implementation was changed by this harness result.
- No production PRX was built or deployed as part of this characterization.
- No PKG was staged or installed.
- No PRX was activated; no GoldHEN restart or PS4 configuration/data mutation was performed.
- No PS4 runtime, GNM dispatch, GPU execution, firmware-14.02 compatibility, or hardware stability claim follows from these host tests.
- Independent human review was not performed as part of this CI run.

## Required next engineering gates

Do not integrate a decoder adapter into the live hook until the production design and tests establish, at minimum:

1. A bounded input-span API whose readable-memory contract is explicit and whose caller can safely establish the span without a query-then-read race.
2. Complete instruction metadata needed to distinguish relative control flow and RIP-relative memory operands, plus relocation tests for each supported encoding.
3. A fail-closed relocation/trampoline implementation with range checks and no target mutation before all prerequisites are ready.
4. Checked protection changes, transactional rollback, and restore APIs that report failure without freeing reachable trampoline memory.
5. Callback quiescence/rendezvous before teardown or unmapping.
6. Correct instruction-cache/data-cache publication semantics based on an authoritative PS4 contract.
7. Firmware-14.02 API/ABI evidence for every console-specific operation.

Until those gates are closed, the existing production detour path remains unsuitable for enabling a live GNM hook on the basis of this harness alone.
