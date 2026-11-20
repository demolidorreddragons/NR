#!/usr/bin/env python3
"""Host-side E4M3FN and binary16 reference checks for the GCN8 shader kernels."""
from __future__ import annotations

import math
import struct


def decode_e4m3fn(code: int) -> float:
    code &= 0xFF
    sign = -1.0 if code & 0x80 else 1.0
    exponent = (code >> 3) & 0x0F
    mantissa = code & 0x07
    if exponent == 0 and mantissa == 0:
        return math.copysign(0.0, sign)
    if exponent == 0:
        return sign * mantissa / 512.0
    if exponent == 15 and mantissa == 7:
        return math.nan
    return sign * (1.0 + mantissa / 8.0) * (2.0 ** (exponent - 7))


def encode_e4m3fn(value: float) -> int:
    if math.isnan(value):
        return 0x7F
    if value == 0.0:
        return 0x80 if math.copysign(1.0, value) < 0 else 0
    if math.isinf(value) or abs(value) >= 448.0:
        return (0x80 if value < 0 else 0) | 0x7E

    best_code = 0
    best_error = math.inf
    for code in range(255):
        candidate = decode_e4m3fn(code)
        if math.isnan(candidate):
            continue
        error = abs(value - candidate)
        if error < best_error or (
            error == best_error and (code & 1) == 0 and (best_code & 1) != 0
        ):
            best_error = error
            best_code = code
    return best_code


def half(value: float) -> float:
    return struct.unpack("<e", struct.pack("<e", value))[0]



def tiled_gemm_emulation(a, b, m: int, n: int, k: int, alpha: float, beta: float, c):
    """CPU emulation of the shader's 16x16 / 64-lane / 2x2 mapping."""
    out = [0.0] * (m * n)
    for group_y in range((m + 15) // 16):
        for group_x in range((n + 15) // 16):
            block_row, block_col = group_y * 16, group_x * 16
            accum = [[[0.0, 0.0], [0.0, 0.0]] for _ in range(8 * 8)]
            for base_k in range(0, k, 16):
                tile_a = [[0.0] * 16 for _ in range(16)]
                tile_b = [[0.0] * 16 for _ in range(16)]
                for lane in range(64):
                    for item in range(4):
                        linear = lane + item * 64
                        row, col = linear >> 4, linear & 15
                        ar, ak = block_row + row, base_k + col
                        bk, bc = base_k + row, block_col + col
                        if ar < m and ak < k:
                            tile_a[row][col] = a[ar * k + ak]
                        if bk < k and bc < n:
                            tile_b[row][col] = b[bk * n + bc]
                for lane in range(64):
                    lx, ly = lane & 7, lane >> 3
                    acc = accum[lane]
                    for kk in range(16):
                        a0, a1 = tile_a[ly * 2][kk], tile_a[ly * 2 + 1][kk]
                        b0, b1 = tile_b[kk][lx * 2], tile_b[kk][lx * 2 + 1]
                        acc[0][0] += a0 * b0
                        acc[0][1] += a0 * b1
                        acc[1][0] += a1 * b0
                        acc[1][1] += a1 * b1
            for lane in range(64):
                lx, ly = lane & 7, lane >> 3
                r0, r1 = block_row + ly * 2, block_row + ly * 2 + 1
                c0, c1 = block_col + lx * 2, block_col + lx * 2 + 1
                acc = accum[lane]
                for rr, row in enumerate((r0, r1)):
                    for cc, col in enumerate((c0, c1)):
                        if row < m and col < n:
                            idx = row * n + col
                            residual = 0.0 if beta == 0.0 else beta * c[idx]
                            out[idx] = alpha * acc[rr][cc] + residual
    return out


def test_tiled_gemm_mapping() -> None:
    # Deterministic, non-symmetric values expose transposition and edge-index bugs.
    for m, n, k in ((1, 1, 1), (3, 5, 7), (15, 17, 9), (16, 16, 16),
                    (17, 31, 19), (31, 15, 33)):
        a = [((i * 7) % 19 - 9) / 8.0 for i in range(m * k)]
        b = [((i * 11) % 23 - 11) / 9.0 for i in range(k * n)]
        c = [((i * 5) % 13 - 6) / 7.0 for i in range(m * n)]
        alpha, beta = 0.75, -0.25
        got = tiled_gemm_emulation(a, b, m, n, k, alpha, beta, c)
        for row in range(m):
            for col in range(n):
                dot = sum(a[row * k + kk] * b[kk * n + col] for kk in range(k))
                expected = alpha * dot + beta * c[row * n + col]
                assert abs(got[row * n + col] - expected) <= 1e-10, (
                    m, n, k, row, col, got[row * n + col], expected
                )


def main() -> None:
    # Every finite encoding round-trips through decode -> encode exactly.
    for code in range(256):
        value = decode_e4m3fn(code)
        if math.isnan(value):
            assert encode_e4m3fn(value) == 0x7F
        else:
            assert encode_e4m3fn(value) == code, (hex(code), value, hex(encode_e4m3fn(value)))

    assert decode_e4m3fn(0x01) == 2.0 ** -9
    assert decode_e4m3fn(0x7E) == 448.0
    assert math.isnan(decode_e4m3fn(0x7F))
    assert decode_e4m3fn(0xFE) == -448.0
    assert encode_e4m3fn(500.0) == 0x7E
    assert encode_e4m3fn(-500.0) == 0xFE
    assert encode_e4m3fn(0.0) == 0x00
    assert encode_e4m3fn(-0.0) == 0x80

    test_tiled_gemm_mapping()

    # Binary16 conversion must preserve representative normal/subnormal values.
    assert half(1.0) == 1.0
    assert half(2.0 ** -24) == 2.0 ** -24
    assert half(65504.0) == 65504.0
    assert half(1.00048828125) == 1.0  # ties-to-even

    print("PASS: E4M3FN 256-code round-trip, binary16 fixtures, tiled GEMM mapping and edge dimensions")


if __name__ == "__main__":
    main()
