#!/usr/bin/env python3
"""Validate OpenDLSS-NR model-directory stage hashes and tensor slices.

This checks container integrity only; it does not infer tensor shapes or prove
310.8.3/310.8.0 semantic compatibility, numerical parity, or GPU execution.
"""
from __future__ import annotations
import argparse, hashlib, json, re, sys
from pathlib import Path, PurePosixPath

FORMAT = "PNE-exported-DLSSNR-packed-v1"
NAME = re.compile(r"^block(\d+)\.layer(\d+)\.([A-Za-z0-9_]+)$")
SHA = re.compile(r"^[0-9a-f]{64}$")

class ValidationError(ValueError): pass

def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""): h.update(chunk)
    return h.hexdigest()

def integer(v, label, minimum=0):
    if isinstance(v, bool) or not isinstance(v, int) or v < minimum:
        raise ValidationError(f"{label} must be an integer >= {minimum}")
    return v

def validate(manifest_path: Path, inventory_path: Path | None = None,
             expected_blocks: int | None = None, expected_tensors: int | None = None,
             full_coverage: bool = False, reference_layout: bool = False) -> dict:
    root = manifest_path.parent.resolve()
    try: m = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as e:
        raise ValidationError(f"cannot read manifest: {e}") from e
    if not isinstance(m, dict) or m.get("format") != FORMAT:
        raise ValidationError(f"manifest format must be {FORMAT}")
    totals = m.get("totals")
    if not isinstance(totals, dict): raise ValidationError("totals must be an object")
    blocks = integer(totals.get("blockCount"), "totals.blockCount", 1)
    if expected_blocks is not None and blocks != expected_blocks:
        raise ValidationError(f"blockCount {blocks} != expected {expected_blocks}")

    stages = m.get("stages")
    if not isinstance(stages, list) or not stages: raise ValidationError("stages must be a non-empty array")
    stage_map = {}
    for s in stages:
        if not isinstance(s, dict): raise ValidationError("stage entry must be an object")
        sid, rel, size, digest = s.get("id"), s.get("file"), s.get("packedByteLength"), s.get("sha256")
        if not isinstance(sid, str) or not sid or sid in stage_map: raise ValidationError(f"invalid/duplicate stage id: {sid!r}")
        if not isinstance(rel, str) or not rel: raise ValidationError(f"{sid}: missing file path")
        p = PurePosixPath(rel)
        if p.is_absolute() or any(x in ("", ".", "..") for x in p.parts) or "\\" in rel or ":" in rel:
            raise ValidationError(f"{sid}: unsafe stage path {rel!r}")
        path = root / "model" / Path(*p.parts)
        base = (root / "model").resolve()
        resolved = path.resolve(strict=False)
        if resolved != base and base not in resolved.parents: raise ValidationError(f"{sid}: path escapes model directory")
        if path.is_symlink() or not path.is_file(): raise ValidationError(f"{sid}: stage file missing or symlink: {path}")
        size = integer(size, f"{sid}.packedByteLength", 1)
        if not isinstance(digest, str) or not SHA.fullmatch(digest): raise ValidationError(f"{sid}: invalid SHA-256")
        if path.stat().st_size != size: raise ValidationError(f"{sid}: stage size mismatch")
        actual = sha256(path)
        if actual != digest: raise ValidationError(f"{sid}: SHA-256 mismatch ({actual})")
        stage_map[sid] = {"size": size, "sha256": actual, "spans": []}

    tensors = m.get("tensors")
    if not isinstance(tensors, list) or not tensors: raise ValidationError("tensors must be a non-empty array")
    if expected_tensors is not None and len(tensors) != expected_tensors:
        raise ValidationError(f"tensor count {len(tensors)} != expected {expected_tensors}")
    seen, present, total = set(), set(), 0
    for t in tensors:
        if not isinstance(t, dict): raise ValidationError("tensor entry must be an object")
        name = t.get("name")
        match = NAME.fullmatch(name) if isinstance(name, str) else None
        if not match: raise ValidationError(f"invalid tensor name: {name!r}")
        if name in seen: raise ValidationError(f"duplicate tensor {name}")
        seen.add(name)
        block = integer(t.get("block"), f"{name}.block")
        layer = integer(t.get("layer"), f"{name}.layer")
        if block >= blocks or block != int(match.group(1)) or layer != int(match.group(2)):
            raise ValidationError(f"{name}: block/layer metadata mismatch")
        if t.get("parameter") != match.group(3): raise ValidationError(f"{name}: parameter metadata mismatch")
        sid = t.get("stage")
        if sid not in stage_map: raise ValidationError(f"{name}: unknown stage {sid!r}")
        off = integer(t.get("stageOffset"), f"{name}.stageOffset")
        length = integer(t.get("byteLength"), f"{name}.byteLength", 1)
        if off > stage_map[sid]["size"] or length > stage_map[sid]["size"] - off:
            raise ValidationError(f"{name}: tensor slice exceeds stage")
        stage_map[sid]["spans"].append((off, off + length, name))
        present.add(block); total += length

    coverage = {}
    for sid, s in stage_map.items():
        spans = sorted(s["spans"]); end = 0; gaps = 0
        for start, stop, name in spans:
            if start < end: raise ValidationError(f"{sid}: overlapping tensor slice at {name}")
            gaps += start - end; end = stop
        gaps += s["size"] - end
        if full_coverage and gaps: raise ValidationError(f"{sid}: {gaps} unreferenced stage bytes")
        coverage[sid] = {"tensor_count": len(spans), "unreferenced_bytes": gaps}

    if inventory_path:
        try: inv = json.loads(inventory_path.read_text(encoding="utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError) as e: raise ValidationError(f"cannot read inventory: {e}") from e
        records = inv.get("records") if isinstance(inv, dict) else None
        if not isinstance(records, list): raise ValidationError("inventory.records must be an array")
        expected = {}
        for r in records:
            if not isinstance(r, dict) or not isinstance(r.get("name"), str): raise ValidationError("malformed inventory record")
            n = r["name"]
            if n in expected: raise ValidationError(f"duplicate inventory record {n}")
            expected[n] = integer(r.get("payload_bytes"), f"{n}.payload_bytes", 1)
        actual = {t["name"]: t["byteLength"] for t in tensors}
        if expected != actual: raise ValidationError("manifest tensor names/byte lengths differ from source inventory")

    reference_layout_count = 0
    reference_consumed_count = 0
    reference_unread_names = []
    if reference_layout:
        expected_layout = reference_layout_sizes()
        actual_layout = {t["name"]: t["byteLength"] for t in tensors}
        if actual_layout != expected_layout:
            missing = sorted(set(expected_layout) - set(actual_layout))[:5]
            extra = sorted(set(actual_layout) - set(expected_layout))[:5]
            mismatch = sorted(n for n in set(expected_layout) & set(actual_layout) if expected_layout[n] != actual_layout[n])[:5]
            raise ValidationError(f"reference layout mismatch: missing={missing}, extra={extra}, sizes={mismatch}")
        reference_layout_count = len(expected_layout)
        consumed_names = reference_consumed_names()
        reference_consumed_count = len(consumed_names)
        reference_unread_names = sorted(set(actual_layout) - consumed_names)
    return {"status":"PASS","block_count":blocks,"blocks_present":len(present),
            "tensor_count":len(tensors),"stage_count":len(stage_map),"tensor_payload_bytes":total,
            "stages":{k:{"bytes":v["size"],"sha256":v["sha256"],**coverage[k]} for k,v in stage_map.items()},
            "inventory_compared":bool(inventory_path),"full_coverage_required":full_coverage,
            "reference_layout_checked":reference_layout,"reference_layout_records":reference_layout_count,
            "reference_consumed_records":reference_consumed_count,"reference_unread_records":len(reference_unread_names),
            "reference_unread_names":reference_unread_names,
            "scope":"Container/slice integrity only; no semantic shapes, version parity, or inference validation."}

def reference_layout_sizes() -> dict[str, int]:
    """Expected record byte lengths from OpenDLSS-NR's pinned nr_graph.cpp."""
    def align_up(v, a): return (v + a - 1) // a * a
    def fused_end(c):
        heads = c // 32
        expert = c >= 64
        count = c // 32 if expert else 0
        expand = count * c * 128 if expert else c * 128
        weights = expand + (count * 128 * 32 + count * 32 * c if expert else 128 * c)
        ffn_skip = weights + 16
        qkv = ffn_skip + c * 2 + 16
        relative = qkv + 3 * c * c
        scale = relative + heads * 8192
        projection = scale + align_up(heads * 4, 16)
        attention_skip = projection + c * c
        return attention_skip + c * 2
    def upsample_size(input_channels, c):
        heads = c // 32
        expert = c >= 64
        count = c // 32 if expert else 0
        expand = count * c * 128 if expert else c * 128
        weights = expand + (count * 128 * 32 + count * 32 * c if expert else 128 * c)
        narrow_padding = 16 if c == 32 else 0
        ffn_skip = weights + input_channels * c + narrow_padding
        transition_scale = ffn_skip + c * 2 + narrow_padding
        qkv = transition_scale + c * 2
        relative = qkv + 3 * c * c
        scale = relative + heads * 8192
        projection = scale + align_up(heads * 4, 16)
        attention_skip = projection + c * c
        return attention_skip + c * 2 + 16

    sizes = {"block0.layer0.layer": 21696}
    for b in range(1, 5):
        sizes[f"block{b}.layer0.layer"] = fused_end(32) + 16 + (32 * 64 if b == 4 else 0)
    for b in range(5, 9):
        sizes[f"block{b}.layer0.layer"] = fused_end(64) + (64 * 128 if b == 8 else 16)
    for b in range(9, 15):
        sizes[f"block{b}.layer0.layer"] = fused_end(128) + (128 * 256 if b == 14 else 16)
    for b in range(15, 23):
        sizes[f"block{b}.layer0.layer"] = fused_end(256) + (256 * 512 if b == 22 else 16)

    split_sizes = (524288, 263168, 917568, 263168)
    for b in list(range(23, 31)) + list(range(40, 48)):
        for layer, size in enumerate(split_sizes):
            sizes[f"block{b}.layer{layer}.layer"] = size
    sizes["block30.layer4.layer"] = 512 * 1024 + 16

    vit_sizes = (1024 * 4096 + 16, 1024 * 4096 + 2048,
                 3 * 1024 * 1024 + 32 * 4, 2, 1024 * 1024 + 2048)
    for b in range(31, 39):
        for layer, size in enumerate(vit_sizes):
            sizes[f"block{b}.layer{layer}.layer"] = size
    sizes["block39.layer0.layer"] = 1024 * 512 + 512 * 2

    for channels, first, last in ((256, 48, 55), (128, 56, 61),
                                  (64, 62, 65), (32, 66, 69)):
        for b in range(first, last + 1):
            sizes[f"block{b}.layer0.layer"] = (
                upsample_size(channels * 2, channels) if b == first else fused_end(channels) + 16
            )
    sizes["block70.layer0.layer"] = 21808
    sizes["block70.layer0.blend_scale"] = 2
    return sizes


def reference_consumed_names() -> set[str]:
    """Names read by the reference graph; ViT layer3 contains 2 unread bytes."""
    names = set(reference_layout_sizes())
    for block in range(31, 39):
        names.remove(f"block{block}.layer3.layer")
    return names


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("manifest", type=Path); ap.add_argument("--inventory", type=Path)
    ap.add_argument("--expected-blocks", type=int); ap.add_argument("--expected-tensors", type=int)
    ap.add_argument("--require-full-coverage", action="store_true")
    ap.add_argument("--check-reference-layout", action="store_true", help="check all record sizes against OpenDLSS-NR nr_graph.cpp")
    a = ap.parse_args()
    try: result = validate(a.manifest, a.inventory, a.expected_blocks, a.expected_tensors, a.require_full_coverage, a.check_reference_layout)
    except ValidationError as e:
        print(json.dumps({"status":"FAIL","error":str(e)}, indent=2), file=sys.stderr); return 1
    print(json.dumps(result, indent=2)); return 0
if __name__ == "__main__": raise SystemExit(main())
