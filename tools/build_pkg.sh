#!/usr/bin/env bash
set -euo pipefail
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
: "${OO_PS4_TOOLCHAIN:?OO_PS4_TOOLCHAIN is required}"
: "${GOLDHEN_SDK:?GOLDHEN_SDK is required}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="$ROOT/build/pkg-stage"
OUT="$ROOT/dist/pkg"
TOOLS="$OO_PS4_TOOLCHAIN/bin/linux"
TITLE_ID="PNEU00001"
CONTENT_ID="IV0000-PNEU00001_00-PS4NEURALENG0000"

rm -rf "$STAGE"
mkdir -p "$STAGE/sce_sys/about" "$STAGE/sce_module" "$OUT"
test -s "$ROOT/dist/ps4/ps4_neural_engine.prx"
cp "$ROOT/dist/ps4/ps4_neural_engine.prx" "$STAGE/ps4_neural_engine.prx"
python3 "$ROOT/tools/make_icon.py" "$STAGE/sce_sys/icon0.png"

# OpenOrbis homebrew packages need these runtime support modules in app0.
# Without them, the PKG can install but the PS4 may report that the app is
# corrupted when it is launched.
SAMPLE="$OO_PS4_TOOLCHAIN/samples/piglet"
for required in "$SAMPLE/sce_module/libc.prx" \
                "$SAMPLE/sce_module/libSceFios2.prx" \
                "$SAMPLE/sce_sys/about/right.sprx"; do
  test -s "$required" || {
    echo "PKG_STAGE: required OpenOrbis runtime support file missing: $required" >&2
    exit 1
  }
done
cp "$SAMPLE/sce_module/libc.prx" "$STAGE/sce_module/libc.prx"
cp "$SAMPLE/sce_module/libSceFios2.prx" "$STAGE/sce_module/libSceFios2.prx"
cp "$SAMPLE/sce_sys/about/right.sprx" "$STAGE/sce_sys/about/right.sprx"

clang --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables \
  -isysroot "$OO_PS4_TOOLCHAIN" -isystem "$OO_PS4_TOOLCHAIN/include" \
  -isystem /usr/include/x86_64-linux-gnu -I"$GOLDHEN_SDK/include" \
  -c "$ROOT/installer/main.c" -o "$STAGE/installer.o"

# Match the OpenOrbis sample linker flags: let link.x/crt1.o define startup.
ld.lld -m elf_x86_64 -pie --script "$OO_PS4_TOOLCHAIN/link.x" \
  --eh-frame-hdr -L"$OO_PS4_TOOLCHAIN/lib" "$STAGE/installer.o" \
  -lc -lkernel -lSceSystemService "$OO_PS4_TOOLCHAIN/lib/crt1.o" \
  -o "$STAGE/installer.elf"
test -s "$STAGE/installer.elf"
readelf -h "$STAGE/installer.elf" | grep -Eq 'Class:[[:space:]]+ELF64'
readelf -h "$STAGE/installer.elf" | grep -Eq 'Machine:[[:space:]]+Advanced Micro Devices X86-64'
readelf -h "$STAGE/installer.elf" | grep -Eq 'Entry point address:[[:space:]]+0x[1-9a-fA-F]'

# create-fself embeds DT_SCE_FILENAME; use a package-relative input filename.
(
  cd "$STAGE"
  "$TOOLS/create-fself" -in=installer.elf -out=installer.oelf \
    --eboot eboot.bin --paid 0x3800000000000011
)
test -s "$STAGE/eboot.bin"

"$TOOLS/PkgTool.Core" sfo_new "$STAGE/sce_sys/param.sfo"
sfo() {
  "$TOOLS/PkgTool.Core" sfo_setentry "$STAGE/sce_sys/param.sfo" "$1" \
    --type "$2" --maxsize "$3" --value "$4"
}
sfo APP_TYPE Integer 4 1
sfo APP_VER Utf8 8 1.00
sfo ATTRIBUTE Integer 4 0
sfo CATEGORY Utf8 4 gd
sfo CONTENT_ID Utf8 48 "$CONTENT_ID"
sfo DOWNLOAD_DATA_SIZE Integer 4 0
sfo SYSTEM_VER Integer 4 0
sfo TITLE Utf8 128 "PS4 Neural Engine Installer"
sfo TITLE_ID Utf8 12 "$TITLE_ID"
sfo VERSION Utf8 8 1.00

# Validate all staged files before building. param.sfo and icon0.png are
# PKG metadata entries; pkg_extract exposes the uroot payload, not those entries.
for required in "$STAGE/ps4_neural_engine.prx" "$STAGE/eboot.bin" \
                "$STAGE/sce_sys/param.sfo" "$STAGE/sce_sys/icon0.png" \
                "$STAGE/sce_sys/about/right.sprx" \
                "$STAGE/sce_module/libc.prx" "$STAGE/sce_module/libSceFios2.prx"; do
  test -s "$required" || { echo "PKG_STAGE: missing or empty: $required" >&2; exit 1; }
done

cd "$STAGE"
"$TOOLS/create-gp4" -out package.gp4 --content-id="$CONTENT_ID" \
  --files "eboot.bin ps4_neural_engine.prx sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx sce_module/libc.prx sce_module/libSceFios2.prx"
"$TOOLS/PkgTool.Core" pkg_build package.gp4 "$OUT"
pkg="$(find "$OUT" -maxdepth 1 -type f -name '*.pkg' -print -quit)"
test -n "$pkg" && test -s "$pkg"
echo "VALIDATION_STAGE: checking PKG hashes and signatures"
"$TOOLS/PkgTool.Core" pkg_validate --verbose "$pkg"
echo "VALIDATION_STAGE: PKG hash/signature check passed"
VERIFY="$STAGE/pkg-verify"
rm -rf "$VERIFY" && mkdir -p "$VERIFY"
echo "EXTRACTION_STAGE: extracting PKG payload for file-level verification"
"$TOOLS/PkgTool.Core" pkg_extract --verbose "$pkg" "$VERIFY"
echo "EXTRACTION_STAGE: PKG payload extraction completed"
echo "VERIFY_STAGE: checking extracted uroot payload"
find "$VERIFY" -maxdepth 5 -type f -printf '%p (%s bytes)\\n' | sort
python3 - "$VERIFY" <<'PY'
import os
import sys

root = sys.argv[1]
required = {"ps4_neural_engine.prx", "eboot.bin", "keystone", "libc.prx", "libSceFios2.prx", "right.sprx"}
found = set()
for current, _, files in os.walk(root):
    for name in files:
        path = os.path.join(current, name)
        size = os.path.getsize(path)
        if name in required and size > 0:
            found.add(name)
            print(f"VERIFY_STAGE: found {name} ({size} bytes)")
missing = sorted(required - found)
if missing:
    print("VERIFY_STAGE: missing or empty extracted payload: " + ", ".join(missing), file=sys.stderr)
    raise SystemExit(1)
print("VERIFY_STAGE: extracted executable payload is complete")
PY
sha256sum "$pkg"
echo "PKG generated and validated: $pkg"
