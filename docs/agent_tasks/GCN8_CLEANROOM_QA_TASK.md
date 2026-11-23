# Clean-room GCN8 numerical and binary QA task

Workstream: independent QA under issue #7. This branch must not change implementation files or inherit implementation conclusions.

## Scope
- Inspect PR #5 and the five GFX8 shader artifacts from immutable commit/run evidence.
- Independently derive expected E4M3FN decode/encode, binary16 rounding and FP32/FP16 GEMM test vectors before evaluating implementation output.
- Build a read-only validator for GNM shader containers. Independently decode OrbShdr/GnmCsShader/GnmShaderBinaryInfo and input-usage slot tables; compare resolved pointers with actual table byte offsets, not merely CRC or "after header".
- Specifically reproduce and test issue #8: a long shader with >1 KiB code and at least one resource slot; detect 8-bit offset overflow and reject truncated pointers.
- Inspect SPIR-V decorations, descriptor bindings, push constants, memory semantics/barriers and bounds. Where ISA disassembly or hardware evidence is unavailable, mark BLOCKED.
- Differentially test FP32 GEMM, FP16 rounded accumulation, E4M3 conversions, edge dimensions, partial tiles, NaN/Inf, signed zero, subnormals, and overflow.
- Record tool versions, commit SHA, artifact SHA-256, reproducible commands and per-shader PASS/FAIL/BLOCKED.

## Independence constraints
- Do not edit shader/compiler/backend implementation under review.
- Do not treat successful compilation, CRC, green CI or implementation reports as proof.
- Do not use proprietary model weights; synthetic fixtures only.
- Do not claim PS4 Pro execution without actual console logs.
- Retest fixes only against new immutable hashes.

## Deliverables
- Independent validator/tests in this QA branch.
- Reproducible report mapped to issue #7 acceptance criteria.
- Explicit verdict for each shader and each numerical test.
