#!/usr/bin/env python3
"""Host-only tests for the 71-block DLSS-NR packed tensor layout formulas."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.verify_dlssnr_tensor_layout import expected_lengths, fused_end, upsample_length
from tools.export_dlssnr_graph_layout import build_graph_layout


class DlssNrLayoutFormulaTests(unittest.TestCase):
    def test_all_153_records_are_described(self):
        layout = expected_lengths()
        self.assertEqual(len(layout), 153)
        self.assertEqual({int(name.split(".")[0][5:]) for name in layout}, set(range(71)))

    def test_encoder_transition_sizes(self):
        layout = expected_lengths()
        self.assertEqual(layout["block4.layer0.layer"], 22720)
        self.assertEqual(layout["block8.layer0.layer"], 69936)
        self.assertEqual(layout["block14.layer0.layer"], 229936)
        self.assertEqual(layout["block22.layer0.layer"], 820288)

    def test_split_512_and_vit_sizes(self):
        layout = expected_lengths()
        for block in list(range(23, 31)) + list(range(40, 48)):
            self.assertEqual(layout[f"block{block}.layer0.layer"], 524288)
            self.assertEqual(layout[f"block{block}.layer1.layer"], 263168)
            self.assertEqual(layout[f"block{block}.layer2.layer"], 917568)
            self.assertEqual(layout[f"block{block}.layer3.layer"], 263168)
        self.assertEqual(layout["block30.layer4.layer"], 524304)
        self.assertEqual(layout["block31.layer0.layer"], 4194320)
        self.assertEqual(layout["block31.layer1.layer"], 4196352)
        self.assertEqual(layout["block31.layer2.layer"], 3145856)
        self.assertEqual(layout["block31.layer3.layer"], 2)
        self.assertEqual(layout["block31.layer4.layer"], 1050624)

    def test_decoder_upsample_layout_sizes(self):
        layout = expected_lengths()
        self.assertEqual(layout["block48.layer0.layer"], upsample_length(512, 256))
        self.assertEqual(layout["block56.layer0.layer"], upsample_length(256, 128))
        self.assertEqual(layout["block62.layer0.layer"], upsample_length(128, 64))
        self.assertEqual(layout["block66.layer0.layer"], upsample_length(64, 32))
        self.assertEqual(layout["block48.layer0.layer"], 820784)
        self.assertEqual(layout["block56.layer0.layer"], 230176)
        self.assertEqual(layout["block62.layer0.layer"], 70048)
        self.assertEqual(layout["block66.layer0.layer"], 22784)

    def test_machine_readable_graph_export_has_all_blocks_and_records(self):
        graph = build_graph_layout()
        self.assertEqual(graph["schema"], "nr-dlssnr-reference-graph-layout-v1")
        self.assertEqual(graph["model"]["blocks"], 71)
        self.assertEqual(graph["model"]["tensor_records"], 153)
        self.assertEqual(len(graph["blocks"]), 71)
        self.assertEqual(sum(len(block["tensor_records"]) for block in graph["blocks"]), 153)
        self.assertEqual(len(graph["transitions"]), 11)
        self.assertEqual(graph["blocks"][0]["channels_in"], 16)
        self.assertEqual(graph["blocks"][70]["channels_out"], 4)

    def test_post_head_and_fused_widths(self):
        layout = expected_lengths()
        self.assertEqual(layout["block70.layer0.layer"], 21808)
        self.assertEqual(layout["block70.layer0.blend_scale"], 2)
        self.assertEqual(fused_end(32) + 16, 20672)
        self.assertEqual(fused_end(64) + 16, 61760)
        self.assertEqual(fused_end(128) + 16, 197184)
        self.assertEqual(fused_end(256) + 16, 689232)


if __name__ == "__main__":
    unittest.main(verbosity=2)
