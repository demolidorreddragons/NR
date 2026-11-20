from __future__ import annotations

import contextlib
import hashlib
import io
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import verify_model_inputs


class ModelInputVerifierTests(unittest.TestCase):
    def test_missing_input_is_reported(self):
        with tempfile.TemporaryDirectory() as temp:
            with patch.object(verify_model_inputs, "EXPECTED", {"model.bin": {"sha256": "00"}}):
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    result = verify_model_inputs.verify(Path(temp))
        self.assertEqual(result, 2)
        self.assertIn("MISSING  model.bin", output.getvalue())

    def test_hash_mismatch_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "model.bin").write_bytes(b"wrong")
            with patch.object(verify_model_inputs, "EXPECTED", {"model.bin": {"sha256": "00"}}):
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    result = verify_model_inputs.verify(root)
        self.assertEqual(result, 1)
        self.assertIn("INVALID  model.bin", output.getvalue())

    def test_exact_hash_is_accepted(self):
        payload = b"known model fixture"
        digest = hashlib.sha256(payload).hexdigest()
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "model.bin").write_bytes(payload)
            with patch.object(verify_model_inputs, "EXPECTED", {"model.bin": {"sha256": digest, "size": len(payload)}}):
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    result = verify_model_inputs.verify(root)
        self.assertEqual(result, 0)
        self.assertIn("VERIFIED model.bin", output.getvalue())


if __name__ == "__main__":
    unittest.main()
