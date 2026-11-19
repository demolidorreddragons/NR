#!/usr/bin/env bash
set -euo pipefail
: "${OO_PS4_TOOLCHAIN:?OO_PS4_TOOLCHAIN is required}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLUGIN_DIR="$ROOT/platform/ps4/goldhen/plugin"
STAGE="$ROOT/build/pkg-stage"
OUT="$ROOT/dist/pkg"
TOOLS="$OO_PS4_TOOLCHAIN/bin/linux"
TITLE_ID="PNEU00001"
CONTENT_ID="IV0000-PNEU00001_00-PS4NEURALENG0000"
mkdir -p "$STAGE/sce_sys" "$OUT"
cp "$ROOT/dist/ps4/ps4_neural_engine.prx" "$STAGE/ps4_neural_engine.prx"
python3 "$ROOT/tools/make_icon.py"
cp "$ROOT/sce_sys/icon0.png" "$STAGE/sce_sys/icon0.png"
mkdir -p "$ROOT/sce_sys"
cd "$ROOT"
"$TOOLS/clang" --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -isysroot "$OO_PS4_TOOLCHAIN" -isystem "$OO_PS4_TOOLCHAIN/include" -isystem /usr/include/x86_64-linux-gnu -I"$OO_PS4_TOOLCHAIN/include/orbis" -c installer/main.c -o "$STAGE/installer.o"
"$TOOLS/ld.lld" -m elf_x86_64 -pie --script "$OO_PS4_TOOLCHAIN/link.x" --eh-frame-hdr -L"$OO_PS4_TOOLCHAIN/lib" "$STAGE/installer.o" "$OO_PS4_TOOLCHAIN/lib/crt1.o" -o "$STAGE/installer.elf" -lSceLibcInternal -lkernel -lSceSystemService
"$TOOLS/create-fself" -in="$STAGE/installer.elf" -out="$STAGE/installer.oelf" --eboot "$STAGE/eboot.bin" --paid 0x3800000000000011
"$TOOLS/PkgTool.Core" sfo_new "$STAGE/sce_sys/param.sfo"
sfo() { "$TOOLS/PkgTool.Core" sfo_setentry "$STAGE/sce_sys/param.sfo" "$1" --type "$2" --maxsize "$3" --value "$4"; }
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
"$TOOLS/create-gp4" -out package.gp4 --content-id="$CONTENT_ID" --files "eboot.bin ps4_neural_engine.prx sce_sys/param.sfo sce_sys/icon0.png"
"$TOOLS/PkgTool.Core" pkg_build package.gp4 "$OUT"
echo "PKG generated in $OUT"
