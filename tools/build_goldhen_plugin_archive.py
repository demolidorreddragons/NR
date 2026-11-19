#!/usr/bin/env python3
"""Build a GoldHEN Plugin Source-compatible archive containing only the PNE PRX."""
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile
import hashlib
import sys

root = Path(__file__).resolve().parents[1]
source = root / "dist/ps4/ps4_neural_engine.prx"
out_dir = root / "dist/goldhen"
archive = out_dir / "PS4-Neural-Engine-GoldHEN.zip"

if not source.is_file() or source.stat().st_size == 0:
    raise SystemExit(f"PRX missing or empty: {source}")

out_dir.mkdir(parents=True, exist_ok=True)
with ZipFile(archive, "w", compression=ZIP_DEFLATED, compresslevel=9) as zf:
    # GoldHEN's plugin archive is extracted into /data/GoldHEN/plugins.
    # Keep the PRX at the archive root; do not include or overwrite plugins.ini.
    zf.write(source, arcname="ps4_neural_engine.prx")

with ZipFile(archive, "r") as zf:
    names = zf.namelist()
    if names != ["ps4_neural_engine.prx"]:
        raise SystemExit(f"Unexpected archive contents: {names}")
    info = zf.getinfo(names[0])
    if info.file_size != source.stat().st_size or info.file_size == 0:
        raise SystemExit("Archive PRX size validation failed")
    if zf.testzip() is not None:
        raise SystemExit("Archive CRC validation failed")

digest = hashlib.sha256(archive.read_bytes()).hexdigest()
(archive.with_suffix(".zip.sha256")).write_text(
    f"{digest}  {archive.name}\n", encoding="ascii"
)
print(f"GOLDHEN_ARCHIVE_OK: {archive}")
print(f"GOLDHEN_ARCHIVE_SIZE: {archive.stat().st_size}")
print(f"GOLDHEN_ARCHIVE_SHA256: {digest}")
print("GOLDHEN_ARCHIVE_CONTENTS: ps4_neural_engine.prx (archive root)")
