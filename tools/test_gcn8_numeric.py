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

    # Binary16 conversion must preserve representative normal/subnormal values.
    assert half(1.0) == 1.0
    assert half(2.0 ** -24) == 2.0 ** -24
    assert half(65504.0) == 65504.0
    assert half(1.00048828125) == 1.0  # ties-to-even

    print("PASS: 256 E4M3FN codes, extrema, signed zero, saturation, binary16 fixtures")


if __name__ == "__main__":
    main()
