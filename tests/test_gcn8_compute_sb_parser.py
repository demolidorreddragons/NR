#!/usr/bin/env python3
"""Host tests for parsing the embedded GNM compute input-usage table."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_gcn8_compute_sb import parse_compute_shader_layout


class ComputeShaderLayoutTests(unittest.TestCase):
    def make_shader(self, *, slot_type=0x1C, api_slot=0, start_register=0,
                    slot_count=1, stage_words=10, code_offset=0x28,
                    user_sgpr_count=3):
        blob = bytearray(0x24 + 0x10 + stage_words * 4 + 16)
        gnm = 0x24
        stage = gnm + 0x10
        blob[gnm:gnm + 4] = (0x72646853).to_bytes(4, "little")
        blob[gnm + 8] = 0x04
        blob[gnm + 9] = stage_words
        blob[gnm + 11] = 0x02
        common = (slot_count << 24) | 16
        blob[stage:stage + 4] = common.to_bytes(4, "little")
        blob[stage + 8:stage + 12] = code_offset.to_bytes(4, "little")
        blob[stage + 20:stage + 24] = (user_sgpr_count << 1).to_bytes(4, "little")
        for offset, value in zip((24, 28, 32), (64, 1, 1)):
            blob[stage + offset:stage + offset + 4] = value.to_bytes(4, "little")
        if slot_count:
            slot = stage + 0x24
            blob[slot:slot + 4] = bytes((slot_type, api_slot, start_register, 0))
        return bytes(blob)

    def test_reads_descriptor_table_slot(self):
        layout = parse_compute_shader_layout(self.make_shader(start_register=3))
        self.assertEqual(layout["input_usage_slot_count"], 1)
        self.assertEqual(layout["compute_user_sgpr_count"], 3)
        self.assertEqual(layout["compute_pgm_rsrc2"], 6)
        self.assertEqual(layout["compute_num_thread"], [64, 1, 1])
        self.assertEqual(layout["descriptor_table_slot"], {
            "usage_type": 0x1C,
            "api_slot": 0,
            "start_register": 3,
            "flags": 0,
        })

    def test_rejects_missing_descriptor_table_slot(self):
        with self.assertRaisesRegex(ValueError, "expected exactly one"):
            parse_compute_shader_layout(self.make_shader(slot_type=0x19))

    def test_rejects_nonzero_api_slot(self):
        with self.assertRaisesRegex(ValueError, "API slot 0"):
            parse_compute_shader_layout(self.make_shader(api_slot=1))

    def test_rejects_multiple_descriptor_table_slots(self):
        blob = bytearray(self.make_shader(slot_count=2, stage_words=11, code_offset=0x2C))
        stage = 0x34
        blob[stage + 0x28:stage + 0x2C] = bytes((0x1C, 1, 2, 0))
        with self.assertRaisesRegex(ValueError, "expected exactly one"):
            parse_compute_shader_layout(bytes(blob))

    def test_rejects_truncated_stage_header(self):
        with self.assertRaisesRegex(ValueError, "truncated"):
            parse_compute_shader_layout(bytes(0x40))

    def test_rejects_mismatched_code_offset(self):
        with self.assertRaisesRegex(ValueError, "code offset"):
            parse_compute_shader_layout(self.make_shader(code_offset=0x24))

    def test_rejects_slot_table_larger_than_stage_header(self):
        with self.assertRaisesRegex(ValueError, "exceeds declared"):
            parse_compute_shader_layout(self.make_shader(slot_count=2, stage_words=10))


if __name__ == "__main__":
    unittest.main()
