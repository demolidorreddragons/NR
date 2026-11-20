# GCN8 numerical kernel bring-up

This directory starts the PS4 Pro compute-kernel port with small, inspectable
kernels and explicit numerical contracts. It is **not** yet the 71-block
renderer or a tuned production backend.

## Kernels

| File | Contract | Intended use |
|---|---|---|
| `shaders/e4m3_decode.comp` | E4M3FN byte stream to FP32 | Model upload / validation |
| `shaders/e4m3_encode.comp` | FP32 to nearest E4M3FN code, ties-to-even | Offline quantization / fixtures |
| `shaders/gemm_fp32.comp` | Row-major GEMM, FP32 FMA accumulation | Scalar correctness baseline |
| `shaders/gemm_fp32_wave64_tiled.comp` | 16x16 output tile, 64 lanes, LDS staging, 2x2 outputs/lane | GCN8-oriented FP32 bring-up baseline |
| `shaders/gemm_fp16_accum.comp` | FP16-rounded operands and accumulator | Numerical emulation / parity |

The E4M3 format is finite-only E4M3FN: exponent bias 7, subnormals at
(2^{-9}), maximum finite magnitude 448, and 0x7f/0xff NaN. Encoding uses
an exhaustive nearest-representable search to make the rounding rule clear;
it is intentionally not a fast inference quantizer.

The FP16 accumulation shader rounds operands and each FMA result to binary16.
It uses FP32 arithmetic plus explicit binary16 conversion, so it is a
portable *reference semantics* kernel. It does not assert that the PS4 Pro
executes native FP16 FMA. The FP32 GEMM is likewise a scalar correctness
baseline, not a tiled performance implementation.


## GCN8 mapping and current estimates

The tiled kernel is deliberately mapped to one 64-lane workgroup, matching a
GCN wave64. Each lane owns a 2x2 output micro-tile; a workgroup covers 16x16
outputs. A and B are staged through LDS as 16x17 float arrays (2,176 bytes
total per workgroup); the extra column is padding intended to reduce LDS bank
conflicts. Global tile loads are issued as four contiguous 64-element
segments per matrix and K tile. The kernel uses FP32 FMA and two workgroup
barriers per K tile.

For an MxNxK GEMM, the dispatch is ceil(N/16) x ceil(M/16). The nominal work
per full output tile is 8,192 floating-point operations per 16-wide K tile
(4,096 FMAs). The LDS footprint is 2,176 bytes/workgroup, excluding registers,
descriptor state, and runtime allocations. These are static resource/work
estimates, **not measured throughput or occupancy**. Register pressure,
compiler lowering, LDS bank behavior, wave scheduling, and memory coalescing
must be confirmed from PSBC output and a real PS4 Pro run.

## Measurement status

No trustworthy GCN8 timing, bandwidth, occupancy, power, or frame-time result
is available yet. The current execution environment has no
`glslangValidator`, `opengnm-psbc`, or `orbis-clang`, and there is no
attached PS4 GPU timing/readback session. Host-side numeric fixtures do not
measure GPU performance. Do not compare this baseline to DLSS or DLSS 5 until
the same inputs, output quality metric, resolution, and GPU timing method are
used on hardware.

Required next measurement: compile for GFX8 with the pinned PSBC toolchain,
record VGPR/SGPR/LDS and scratch metadata, then run warm-up plus at least 100
timed dispatches on PS4 Pro using GPU timestamps. Report median, p95, variance,
effective GFLOP/s, and max absolute/RMS error against a double-precision
reference for aligned and edge dimensions.

## ABI

All kernels use GLSL 4.50 compute, std430 storage buffers, and a small push
constant block. The host must bind four-byte-aligned buffers and dispatch
ceil(N/16) x ceil(M/16) for the 16x16 tiled GEMM; scalar kernels must use their own local size and dispatch contract, and ceil(element_count/256) groups for E4M3
conversion (64 workgroups lanes, four values per invocation).

The host must provide valid C storage even when beta is zero; a later optimized
ABI may add a no-residual specialization. Insert a GNM UAV/cache barrier
between producer and consumer dispatches and wait for completion before CPU
readback. These kernels do not implement GNM allocation, descriptor binding,
barriers, fences, or command submission.

## Validation gates

1. Compile every shader with the pinned PSBC toolchain and inspect the emitted
   GFX8 shader metadata.
2. Run E4M3 decode/encode against all 256 byte codes and boundary/tie fixtures.
3. Compare FP32 GEMM against a double-precision host reference.
4. Compare FP16-rounded GEMM against a software IEEE-754 binary16 reference.
5. Run buffer -> compute -> buffer on PS4 Pro and verify deterministic readback.
6. Only after those pass, tile GEMM, fuse operators, and schedule graph blocks.

No PS4 hardware validation is claimed by adding these sources.
