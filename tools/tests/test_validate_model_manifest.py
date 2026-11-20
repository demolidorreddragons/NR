import hashlib, json, tempfile, unittest
from pathlib import Path
from tools.validate_model_manifest import ValidationError, validate, reference_layout_sizes, reference_consumed_names

class ManifestTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.root = Path(self.tmp.name)
        (self.root / "model" / "stages").mkdir(parents=True)
        self.payload = b"0123456789abcdef"
        (self.root / "model" / "stages" / "s00.bin").write_bytes(self.payload)
        self.path = self.root / "manifest.json"
        self.m = {"format":"PNE-exported-DLSSNR-packed-v1","totals":{"blockCount":1},
          "stages":[{"id":"s00","file":"stages/s00.bin","packedByteLength":16,"sha256":hashlib.sha256(self.payload).hexdigest()}],
          "tensors":[
            {"name":"block0.layer0.layer","block":0,"layer":0,"parameter":"layer","stage":"s00","stageOffset":0,"byteLength":8},
            {"name":"block0.layer1.layer","block":0,"layer":1,"parameter":"layer","stage":"s00","stageOffset":8,"byteLength":8}]}
        self.write()
    def tearDown(self): self.tmp.cleanup()
    def write(self): self.path.write_text(json.dumps(self.m))
    def invalid(self, **kwargs):
        self.write()
        with self.assertRaises(ValidationError): validate(self.path, **kwargs)
    def test_valid_and_inventory(self):
        inv=self.root/"inventory.json"; inv.write_text(json.dumps({"records":[
          {"name":"block0.layer0.layer","payload_bytes":8},{"name":"block0.layer1.layer","payload_bytes":8}]}))
        self.assertEqual(validate(self.path, inv, 1, 2, True)["status"],"PASS")
    def test_reference_graph_layout_table(self):
        sizes = reference_layout_sizes()
        self.assertEqual(len(sizes), 153)
        self.assertEqual(sizes["block0.layer0.layer"], 21696)
        self.assertEqual(sizes["block31.layer3.layer"], 2)
        self.assertEqual(sizes["block70.layer0.blend_scale"], 2)
        consumed = reference_consumed_names()
        self.assertEqual(len(consumed), 145)
        self.assertEqual(sorted(set(sizes) - consumed), [f"block{b}.layer3.layer" for b in range(31, 39)])

    def test_bad_hash(self): self.m["stages"][0]["sha256"]="0"*64; self.invalid()
    def test_duplicate_name(self): self.m["tensors"][1]=dict(self.m["tensors"][0]); self.invalid()
    def test_overflow(self): self.m["tensors"][1]["stageOffset"]=15; self.invalid()
    def test_overlap(self): self.m["tensors"][1]["stageOffset"]=7; self.invalid()
    def test_path_traversal(self): self.m["stages"][0]["file"]="../../bad"; self.invalid()
    def test_metadata_disagreement(self): self.m["tensors"][0]["block"]=1; self.invalid()
    def test_gap(self): self.m["tensors"][1]["stageOffset"]=9; self.invalid(full_coverage=True)
    def test_inventory_mismatch(self):
        inv=self.root/"inventory.json"; inv.write_text(json.dumps({"records":[
          {"name":"block0.layer0.layer","payload_bytes":7},{"name":"block0.layer1.layer","payload_bytes":8}]}))
        self.invalid(inventory_path=inv)
if __name__ == "__main__": unittest.main()
