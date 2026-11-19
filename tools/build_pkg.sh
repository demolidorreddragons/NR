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
mkdir -p "$STAGE/sce_sys" "$OUT"
test -s "$ROOT/dist/ps4/ps4_neural_engine.prx"
cp "$ROOT/dist/ps4/ps4_neural_engine.prx" "$STAGE/ps4_neural_engine.prx"
python3 "$ROOT/tools/make_icon.py" "$STAGE/sce_sys/icon0.png"

clang --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables \
  -isysroot "$OO_PS4_TOOLCHAIN" -isystem "$OO_PS4_TOOLCHAIN/include" \
  -isystem /usr/include/x86_64-linux-gnu -I"$GOLDHEN_SDK/include" \
  -c "$ROOT/installer/main.c" -o "$STAGE/installer.o"
ld.lld -m elf_x86_64 -pie --script "$OO_PS4_TOOLCHAIN/link.x" \
  --eh-frame-hdr -L"$OO_PS4_TOOLCHAIN/lib" "$STAGE/installer.o" \
  "$OO_PS4_TOOLCHAIN/lib/crt1.o" -o "$STAGE/installer.elf" \
  -lc -lkernel -lSceSystemService
"$TOOLS/create-fself" -in="$STAGE/installer.elf" \
  -out="$STAGE/installer.oelf" --eboot "$STAGE/eboot.bin" \
  --paid 0x3800000000000011

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

cd "$STAGE"
"$TOOLS/create-gp4" -out package.gp4 --content-id="$CONTENT_ID" \
  --files "eboot.bin ps4_neural_engine.prx sce_sys/param.sfo sce_sys/icon0.png"
"$TOOLS/PkgTool.Core" pkg_build package.gp4 "$OUT"
pkg="$(find "$OUT" -maxdepth 1 -type f -name '*.pkg' -print -quit)"
test -n "$pkg" && test -s "$pkg"
"$TOOLS/PkgTool.Core" pkg_validate "$pkg"
VERIFY="$STAGE/pkg-verify"
rm -rf "$VERIFY" && mkdir -p "$VERIFY"
"$TOOLS/PkgTool.Core" pkg_extract "$pkg" "$VERIFY"
find "$VERIFY" -type f -name ps4_neural_engine.prx -size +0c | grep -q .
sha256sum "$pkg"
echo "PKG generated and validated: $pkg"
