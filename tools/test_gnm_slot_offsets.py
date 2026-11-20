#!/usr/bin/env python3
"""Regression tests for GnmShaderBinaryInfo's 8-bit chunk-usage offset."""

import unittest


def resolve_slot_start(orb_offset: int, chunk_offset_dwords: int,
                       num_slots: int, points_to_end: bool) -> int:
    base = orb_offset - chunk_offset_dwords * 4
    return base - (num_slots * 4 if points_to_end else 0)


def expected_offset(orb_offset: int, slot_start: int,
                    num_slots: int, points_to_end: bool) -> int:
    target = slot_start + (num_slots * 4 if points_to_end else 0)
    delta = orb_offset - target
    if delta < 0 or delta % 4:
        raise ValueError("slot table target is out of range or unaligned")
    value = delta // 4
    if value > 0xff:
        raise OverflowError(f"{value} dwords do not fit the 8-bit field")
    return value


class GnmSlotOffsetTests(unittest.TestCase):
    def test_real_tiled_gemm_requires_unrepresentable_offset(self):
        orb = 0x990
        slot_start = 0x58
        num_slots = 1

        # OpenGNM's header comment describes a start pointer; fpPS4 subtracts
        # num_slots after resolving the base pointer (end-pointer convention).
        start_pointer = expected_offset(orb, slot_start, num_slots, False)
        end_pointer = expected_offset(orb, slot_start, num_slots, True)
        self.assertEqual(start_pointer, 590)
        self.assertEqual(end_pointer, 589)
        self.assertGreater(start_pointer, 0xff)
        self.assertGreater(end_pointer, 0xff)

    def test_emitted_wrapped_value_does_not_resolve_to_header_table(self):
        orb = 0x990
        emitted = 78
        num_slots = 1
        self.assertEqual(resolve_slot_start(orb, emitted, num_slots, False), 0x858)
        self.assertEqual(resolve_slot_start(orb, emitted, num_slots, True), 0x854)
        self.assertNotEqual(resolve_slot_start(orb, emitted, num_slots, False), 0x58)
        self.assertNotEqual(resolve_slot_start(orb, emitted, num_slots, True), 0x58)

    def test_small_shader_offset_is_representable(self):
        orb = 0x100
        slot_start = 0x58
        num_slots = 1
        start_pointer = expected_offset(orb, slot_start, num_slots, False)
        end_pointer = expected_offset(orb, slot_start, num_slots, True)
        self.assertEqual(start_pointer, 42)
        self.assertEqual(end_pointer, 41)
        self.assertEqual(resolve_slot_start(orb, start_pointer, num_slots, False), slot_start)
        self.assertEqual(resolve_slot_start(orb, end_pointer, num_slots, True), slot_start)

    def test_unaligned_and_out_of_range_offsets_rejected(self):
        with self.assertRaises(ValueError):
            expected_offset(0x101, 0x58, 1, True)
        with self.assertRaises(ValueError):
            expected_offset(0x50, 0x58, 1, True)
        with self.assertRaises(OverflowError):
            expected_offset(0x990, 0x58, 1, True)

    def test_zero_slots_use_zero_offset(self):
        self.assertEqual(expected_offset(0x990, 0x58, 0, True), 0x990 // 4 - 0x58 // 4)


if __name__ == "__main__":
    unittest.main(verbosity=2)
