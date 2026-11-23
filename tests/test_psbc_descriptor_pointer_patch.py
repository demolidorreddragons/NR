#!/usr/bin/env python3
"""Regression guard for the pinned PSBC two-SGPR GNM descriptor pointer patch."""
from pathlib import Path
import unittest

PATCH = Path(__file__).resolve().parents[1] / (
    "patches/opengnm-psbc/0001-gcn8-descriptor-pointer-reserve-two-sgprs.patch"
)


class DescriptorPointerPatchTests(unittest.TestCase):
    def test_descriptor_set_and_heap_reserve_two_sgprs(self):
        patch = PATCH.read_text()
        self.assertGreaterEqual(patch.count("AC_ARG_SGPR, 2, AC_ARG_CONST_ADDR"), 2)
        self.assertGreaterEqual(patch.count("ud_info->num_sgprs = 2;"), 2)
        self.assertGreaterEqual(patch.count("num_user_sgprs += 2;"), 2)

    def test_indirect_descriptor_sets_reserve_two_sgprs(self):
        patch = PATCH.read_text()
        self.assertIn(
            "RADV_ADD_UD_ARRAY_ARG(state, 2, AC_ARG_CONST_ADDR, descriptors, 0, AC_UD_INDIRECT_DESCRIPTORS)",
            patch,
        )

    def test_lowering_preserves_low_dword_and_reserved_high_sgpr(self):
        patch = PATCH.read_text()
        self.assertIn("if (ptr->num_components == 2)", patch)
        self.assertIn("ptr = nir_channel(b, ptr, 0);", patch)
        self.assertIn("state->address32_hi", patch)


if __name__ == "__main__":
    unittest.main()
