#!/usr/bin/env python3
"""Install the exact user-confirmed 512x512 Neural Rendering icon without modification."""
from pathlib import Path
import hashlib
import shutil
import sys

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit("Missing Pillow. Install python3-pil before validating icon0.png.") from exc

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "neural-rendering-icon-source.png"
OUTPUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("sce_sys/icon0.png")
EXPECTED_SIZE = (512, 512)
EXPECTED_SHA256 = "7fe6a11ad7b4d37e5741c77822730a82487237dbed4a3eca529e995a29421ae4"

if not SOURCE.is_file():
    raise SystemExit(
        "APPROVED_ICON_SOURCE_MISSING: refusing to generate a substitute. "
        f"Add the exact user-confirmed PNG at {SOURCE}."
    )

source_hash = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
if source_hash != EXPECTED_SHA256:
    raise SystemExit(
        "APPROVED_ICON_HASH_MISMATCH: refusing to modify or substitute the approved artwork. "
        f"Expected {EXPECTED_SHA256}; got {source_hash}."
    )

with Image.open(SOURCE) as image:
    image.verify()
with Image.open(SOURCE) as image:
    if image.size != EXPECTED_SIZE or image.mode != "RGB" or image.format != "PNG":
        raise SystemExit(
            f"APPROVED_ICON_FORMAT_MISMATCH: expected RGB PNG 512x512; "
            f"got {image.format} {image.mode} {image.width}x{image.height}."
        )

OUTPUT.parent.mkdir(parents=True, exist_ok=True)
shutil.copyfile(SOURCE, OUTPUT)

output_hash = hashlib.sha256(OUTPUT.read_bytes()).hexdigest()
if output_hash != EXPECTED_SHA256:
    raise SystemExit(
        f"ICON0_COPY_MISMATCH: expected {EXPECTED_SHA256}; got {output_hash}."
    )
with Image.open(OUTPUT) as image:
    image.verify()

print(f"ICON0_SOURCE={SOURCE}")
print(f"ICON0_GENERATED={OUTPUT} ({OUTPUT.stat().st_size} bytes, 512x512 RGB PNG)")
print(f"ICON0_SHA256={output_hash}")
