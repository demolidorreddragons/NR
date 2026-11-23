#!/usr/bin/env python3
"""Regression guard for the pinned PSBC two-SGPR GNM descriptor pointer patch."""
from pathlib import Path
import unittest

PATCH = Path(__file__).resolve().parents[1] / (
    "patches/opengnm-psbc/0001-gcn8-descriptor-pointer-reserve-two-sgprs.patch"
)


class DescriptorPointerPatchTests(unittest.TestCase):
    def test_scalar_aco_argument_reserves_gnm_pointer_width(self):
        patch = PATCH.read_text()
        self.assertGreaterEqual(patch.count("AC_ARG_SGPR, 1, AC_ARG_CONST_ADDR"), 2)
        self.assertGreaterEqual(patch.count("ud_info->num_sgprs = 2;"), 2)
        self.assertGreaterEqual(patch.count("state->args->ac.num_sgprs_used++;"), 2)
        self.assertGreaterEqual(patch.count("num_user_sgprs += 2;"), 2)

    def test_does_not_widen_aco_pointer_value(self):
        patch = PATCH.read_text()
        self.assertNotIn("AC_ARG_SGPR, 2, AC_ARG_CONST_ADDR", patch)
        self.assertNotIn("nir_channel", patch)
        self.assertIn("Reserve the GNM pointer high dword", patch)


if __name__ == "__main__":
    unittest.main()
