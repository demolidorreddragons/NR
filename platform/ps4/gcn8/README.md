# GCN8 numerical kernel bring-up

This directory starts the PS4 Pro compute-kernel port with small, inspectable
kernels and explicit numerical contracts. It is **not** yet the 71-block
renderer or a tuned production backend.

## Kernels

| File | Contract | Intended use |
|---|---|---|
| `shaders/e4m3_decode.comp` | E4M3FN byte stream to FP32 | Model upload / validation |
| `shaders/e4m3_encode.comp` | FP32 to nearest E4M3FN code, ties-to-even | Offline quantization / fixtures |
| `shaders/gemm_fp32.comp` | Row-major GEMM, FP32 FMA accumulation | Reference and parity |
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

## ABI

All kernels use GLSL 4.50 compute, std430 storage buffers, and a small push
constant block. The host must bind four-byte-aligned buffers and dispatch
ceil(N/8) x ceil(M/8) for GEMM, and ceil(element_count/256) groups for E4M3
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
