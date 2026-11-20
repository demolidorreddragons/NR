# Model architecture specialist task — DLSS-NR 310.8.3

Workstream: model reverse-engineering and graph architecture (Python/C++), under issue #4.

## Inputs and constraints
- The user-provided archive is `PS4-Neural-Engine-DLSSNR-model-310.8.3.zip`; it contains a packed stage of 147,683,778 bytes, 153 tensor records, and 71 blocks. The archive itself must never be committed.
- Its manifest identifies stage SHA-256 `37a293b5b781b9351a8b4b5fbeb2ff68fbd8a84891378950812ff3b99d161116`.
- The user-provided source 0.9.0 archive SHA-256 is `101ef376e4e3e3d948adf6153012bf6482f81586b57f7ebf7d2672237b2c1340`.
- Public reference graph: `maanHimself/OpenDLSS-NR` (MIT reconstruction). Do not assume 310.8.3 weights are compatible until independently verified.
- Never commit proprietary weight bytes, NVIDIA DLLs, extracted model payloads, tokens, or credentials.

## Deliverables
1. Independently inspect the OpenDLSS-NR graph specification and source, then document exact tensor/operator graph, input/output ABI, six-level topology, all 71 blocks, and the 75 intermediate validation boundaries.
2. Build a machine-readable graph manifest that maps every consumed parameter to the user-provided 153-record inventory by exact name, block/layer, byte offset, byte length, dtype, shape, layout transform, and consumer. Explicitly account for all 153 records and identify the 8 unused records if reference evidence confirms 145 are consumed.
3. Implement a safe model-manifest parser/validator that checks stage hash/length, record bounds, overlap, duplicate names, missing required tensors, unused records, alignment, and overflow. It must not load the whole 147 MB stage into RAM unnecessarily.
4. Add deterministic host tests using synthetic/fixture tensors only; do not redistribute user weights.
5. Compare the 310.8.3 manifest and tensor layouts with the 310.8.0 reference, documenting proven equivalences and unresolved differences. Do not claim parity based only on equal record counts or payload hashes.
6. Produce an ADR describing operator dependencies and the smallest runnable subgraph for later GNM execution.

## Acceptance
- All tensor mappings have explicit provenance; no guessed shapes or offsets.
- Tests fail closed on corrupted hashes, truncated stages, out-of-range slices, overlaps, unsupported dtype/layout, and missing required parameters.
- No GNM dispatch or hardware claim is made in this workstream.
- Include reproducible commands and hashes in the PR summary.
