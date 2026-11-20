#!/usr/bin/env python3
"""Build icon0.png only from the exact, approved Neural Rendering artwork.

The source image is intentionally not recreated from SVG or vector primitives.
Place the approved original at assets/neural-rendering-icon-source.png.
"""
from pathlib import Path
import sys

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit("Missing Pillow. Install python3-pil before building icon0.png.") from exc

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "neural-rendering-icon-source.png"
OUTPUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("sce_sys/icon0.png")
EXPECTED_SIZE = (512, 512)

if not SOURCE.is_file():
    raise SystemExit(
        "APPROVED_ICON_SOURCE_MISSING: refusing to generate a substitute. "
        f"Add the exact approved image at {SOURCE}."
    )

with Image.open(SOURCE) as original:
    original.verify()

with Image.open(SOURCE) as original:
    if original.width < 512 or original.height < 512:
        raise SystemExit(
            f"Approved source is too small ({original.width}x{original.height}); "
            "refusing an upscaled icon."
        )
    if original.width != original.height:
        raise SystemExit(
            f"Approved icon source must be square to preserve its exact composition; "
            f"got {original.width}x{original.height}."
        )
    icon = original.convert("RGB").resize(EXPECTED_SIZE, Image.Resampling.LANCZOS)

OUTPUT.parent.mkdir(parents=True, exist_ok=True)
icon.save(OUTPUT, format="PNG", optimize=True)
with Image.open(OUTPUT) as check:
    check.verify()
with Image.open(OUTPUT) as check:
    if check.size != EXPECTED_SIZE or check.mode != "RGB":
        raise SystemExit("Generated icon0.png failed 512x512 RGB validation.")
print(f"ICON0_SOURCE={SOURCE}")
print(f"ICON0_GENERATED={OUTPUT} ({OUTPUT.stat().st_size} bytes, 512x512 RGB)")
