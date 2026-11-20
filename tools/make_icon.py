#!/usr/bin/env python3
"""Rasterize the official Neural Rendering SVG into the PS4 PKG icon0.png."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "neural-rendering-logo.svg"
OUTPUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("sce_sys/icon0.png")
PNG_SIGNATURE = bytes.fromhex("89504e470d0a1a0a")

try:
    import cairosvg
except ImportError as exc:
    raise SystemExit(
        "Missing dependency cairosvg. Install it with: "
        "python3 -m pip install cairosvg"
    ) from exc

if not SOURCE.is_file():
    raise SystemExit(f"Official logo SVG not found: {SOURCE}")

OUTPUT.parent.mkdir(parents=True, exist_ok=True)
cairosvg.svg2png(
    url=str(SOURCE),
    write_to=str(OUTPUT),
    output_width=512,
    output_height=512,
)
if not OUTPUT.is_file() or OUTPUT.stat().st_size < 1024:
    raise SystemExit("Generated icon0.png is missing or unexpectedly small.")

with OUTPUT.open("rb") as stream:
    if stream.read(8) != PNG_SIGNATURE:
        raise SystemExit("Generated icon0.png is not a valid PNG.")
print(f"ICON0_GENERATED={OUTPUT} ({OUTPUT.stat().st_size} bytes)")
