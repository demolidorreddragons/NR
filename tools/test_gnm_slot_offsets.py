#!/usr/bin/env python3
"""Regression tests for GnmShaderBinaryInfo's 8-bit chunk-usage offset."""

import unittest


def resolve_slot_start(orb_offset: int, chunk_offset_dwords: int) -> int:
    """OpenGNM start-pointer interpretation: OrbShdr - offset * 4."""
    return orb_offset - chunk_offset_dwords * 4


def expected_offset(orb_offset: int, slot_start: int, num_slots: int) -> int:
    if num_slots == 0:
        return 0
    delta = orb_offset - slot_start
    if delta < 0 or delta % 4:
        raise ValueError("slot table target is out of range or unaligned")
    value = delta // 4
    if value > 0xff:
        raise OverflowError(f"{value} dwords do not fit the 8-bit field")
    return value


class GnmSlotOffsetTests(unittest.TestCase):
    def test_four_short_shader_outputs_resolve_to_header_slot_table(self):
        # Real PSBC output offsets from run 44681174240; slot table at 0x58.
        fixtures = (
            (0x3B4, 215),  # e4m3_decode.sb
            (0x278, 136),  # e4m3_encode.sb
            (0x1C8, 92),   # gemm_fp16_accum.sb
            (0x180, 74),   # gemm_fp32.sb
        )
        for orb, stored in fixtures:
            with self.subTest(orb=hex(orb)):
                self.assertEqual(resolve_slot_start(orb, stored), 0x58)
                self.assertEqual(expected_offset(orb, 0x58, 1), stored)

    def test_tiled_shader_requires_unrepresentable_offset(self):
        orb = 0x990
        slot_start = 0x58
        num_slots = 1
        raw_expected = (orb - slot_start) // 4
        self.assertEqual(raw_expected, 590)
        self.assertGreater(raw_expected, 0xff)
        with self.assertRaises(OverflowError):
            expected_offset(orb, slot_start, num_slots)

    def test_emitted_wrapped_value_points_into_code_not_slot_table(self):
        orb = 0x990
        emitted = 78
        self.assertEqual(resolve_slot_start(orb, emitted), 0x858)
        self.assertNotEqual(resolve_slot_start(orb, emitted), 0x58)

    def test_fp_ps4_end_pointer_interpretation_differs_by_one_slot(self):
        orb = 0x3B4
        stored = 215
        num_slots = 1
        # fpPS4's parser subtracts num_slots after resolving the base pointer.
        # That points four bytes before the header slot table in these fixtures.
        self.assertEqual(resolve_slot_start(orb, stored) - num_slots * 4, 0x54)
        self.assertEqual(resolve_slot_start(orb, stored), 0x58)

    def test_unaligned_and_out_of_range_offsets_rejected(self):
        with self.assertRaises(ValueError):
            expected_offset(0x101, 0x58, 1)
        with self.assertRaises(ValueError):
            expected_offset(0x50, 0x58, 1)
        with self.assertRaises(OverflowError):
            expected_offset(0x990, 0x58, 1)

    def test_maximum_code_size_with_one_header_slot(self):
        # The slot starts at 0x58; code starts immediately after its 4-byte entry.
        # OrbShdr must remain within 255 dwords of the slot-table start.
        max_code_bytes = 0xff * 4 - 4
        self.assertEqual(max_code_bytes, 1016)
        orb = 0x5c + max_code_bytes
        self.assertEqual(expected_offset(orb, 0x58, 1), 0xff)

    def test_code_size_one_dword_over_limit_is_rejected(self):
        max_code_bytes = 0xff * 4 - 4
        orb = 0x5c + max_code_bytes + 4
        with self.assertRaises(OverflowError):
            expected_offset(orb, 0x58, 1)

    def test_zero_slots_use_zero_offset(self):
        self.assertEqual(expected_offset(0x990, 0x58, 0), 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
