#!/usr/bin/env bash
set -euo pipefail
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
: "${OO_PS4_TOOLCHAIN:?OO_PS4_TOOLCHAIN is required}"
: "${GOLDHEN_SDK:?GOLDHEN_SDK is required}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="$ROOT/build/pkg-stage"
OUT="$ROOT/dist/pkg"
TOOLS="$OO_PS4_TOOLCHAIN/bin/linux"
PKGTOOL="${PKGTOOL_CORE:-$TOOLS/PkgTool.Core}"
TITLE_ID="PNEU00001"
APP_VERSION="${APP_VERSION:-01.02}"
if [[ ! "$APP_VERSION" =~ ^[0-9]{2}\.[0-9]{2}$ ]]; then
  echo "APP_VERSION must use Sony XX.YY format (for example 01.01)." >&2
  exit 1
fi
CONTENT_ID="IV0000-PNEU00001_00-PS4NEURALENG0000"
# Release number is embedded in the user-facing PKG filename and preserved by the PS4 bridge.
# The bridge clears its persistent workspace before downloading this build's artifact.
PKG_NUMBER="$(tr -d '[:space:]' < "$ROOT/PKG_BUILD_NUMBER")"
if [[ ! "$PKG_NUMBER" =~ ^[0-9]{3,}$ ]]; then
  echo "PKG_BUILD_NUMBER must contain at least three digits." >&2
  exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/sce_sys/about" "$STAGE/sce_module" "$OUT"
test -s "$ROOT/dist/ps4/ps4_neural_engine.prx"
cp "$ROOT/dist/ps4/ps4_neural_engine.prx" "$STAGE/ps4_neural_engine.prx"
PRX_SHA256="$(sha256sum "$STAGE/ps4_neural_engine.prx" | awk '{print $1}')"
cat > "$STAGE/pne-release.ini" <<EOF
schema=1
release_number=$PKG_NUMBER
installer_version=1
app_version=$APP_VERSION
title_id=$TITLE_ID
content_id=$CONTENT_ID
prx_file=ps4_neural_engine.prx
prx_sha256=$PRX_SHA256
abi_status=unverified
hardware_status=not_tested
EOF
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
  AUTHINFO="000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
  "$TOOLS/create-fself" -in=installer.elf -out=installer.oelf \
    --eboot "eboot.bin" --paid 0x3800000000000035 --authinfo "$AUTHINFO"
)
test -s "$STAGE/eboot.bin"

"$PKGTOOL" sfo_new "$STAGE/sce_sys/param.sfo"
sfo() {
  "$PKGTOOL" sfo_setentry "$STAGE/sce_sys/param.sfo" "$1" \
    --type "$2" --maxsize "$3" --value "$4"
}
sfo APP_TYPE Integer 4 1
sfo APP_VER Utf8 8 "$APP_VERSION"
sfo ATTRIBUTE Integer 18 0
sfo CATEGORY Utf8 4 gde
sfo FORMAT Utf8 4 obs
sfo CONTENT_ID Utf8 48 "$CONTENT_ID"
sfo DOWNLOAD_DATA_SIZE Integer 4 0
sfo SYSTEM_VER Integer 4 1020
sfo TITLE Utf8 128 "PS4 Neural Engine Installer"
sfo TITLE_ID Utf8 12 "$TITLE_ID"
sfo VERSION Utf8 8 "$APP_VERSION"

# Fake PKG Tools resolves an associated base package's storage type from
# PARAM.SFO PUBTOOLINFO. Keep it explicit in the base PKG (GP4 storage_type
# alone is not sufficient for its patch app:unknown lookup).
sfo PUBTOOLINFO Utf8 512 "c_date=$(date -u +%Y%m%d),img0_l0_size=7,img0_l1_size=0,img0_sc_ksize=512,img0_pc_ksize=832,st_type=digital50"

# Validate all staged files before building. param.sfo and icon0.png are
# PKG metadata entries; pkg_extract exposes the uroot payload, not those entries.
for required in "$STAGE/ps4_neural_engine.prx" "$STAGE/pne-release.ini" "$STAGE/eboot.bin" \
                "$STAGE/sce_sys/param.sfo" "$STAGE/sce_sys/icon0.png" \
                "$STAGE/sce_sys/about/right.sprx" \
                "$STAGE/sce_module/libc.prx" "$STAGE/sce_module/libSceFios2.prx"; do
  test -s "$required" || { echo "PKG_STAGE: missing or empty: $required" >&2; exit 1; }
done

cd "$STAGE"
"$TOOLS/create-gp4" -out package.gp4 --content-id="$CONTENT_ID" \
  --files "eboot.bin ps4_neural_engine.prx pne-release.ini sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx sce_module/libc.prx sce_module/libSceFios2.prx"
"$PKGTOOL" pkg_build package.gp4 "$OUT"
pkg="$(find "$OUT" -maxdepth 1 -type f -name '*.pkg' -print -quit)"
test -n "$pkg" && test -s "$pkg"
numbered_pkg="$OUT/PS4-Neural-Engine-${PKG_NUMBER}.pkg"
cp "$pkg" "$numbered_pkg"
pkg="$numbered_pkg"
echo "PKG_NUMBER: $PKG_NUMBER"
echo "VALIDATION_STAGE: checking PKG hashes and signatures"
"$PKGTOOL" pkg_validate --verbose "$pkg"
echo "VALIDATION_STAGE: PKG hash/signature check passed"
VERIFY="$STAGE/pkg-verify"
rm -rf "$VERIFY" && mkdir -p "$VERIFY"
echo "EXTRACTION_STAGE: extracting PKG payload for file-level verification"
"$PKGTOOL" pkg_extract --verbose "$pkg" "$VERIFY"
echo "EXTRACTION_STAGE: PKG payload extraction completed"
echo "VERIFY_STAGE: checking extracted uroot payload"
find "$VERIFY" -maxdepth 5 -type f -printf '%p (%s bytes)\\n' | sort
python3 - "$VERIFY" "$PRX_SHA256" <<'PY'
import hashlib
import os
import sys

root, expected_prx_sha256 = sys.argv[1:]
required = {"ps4_neural_engine.prx", "pne-release.ini", "eboot.bin", "keystone", "libc.prx", "libSceFios2.prx", "right.sprx"}
found = {}
for current, _, files in os.walk(root):
    for name in files:
        path = os.path.join(current, name)
        size = os.path.getsize(path)
        if name in required and size > 0:
            found[name] = path
            print(f"VERIFY_STAGE: found {name} ({size} bytes)")
missing = sorted(required - found.keys())
if missing:
    print("VERIFY_STAGE: missing or empty extracted payload: " + ", ".join(missing), file=sys.stderr)
    raise SystemExit(1)
actual = hashlib.sha256(open(found["ps4_neural_engine.prx"], "rb").read()).hexdigest()
if actual != expected_prx_sha256:
    raise SystemExit(f"VERIFY_STAGE: PRX SHA-256 mismatch: expected {expected_prx_sha256}, got {actual}")
manifest = open(found["pne-release.ini"], encoding="ascii").read().splitlines()
if f"prx_sha256={actual}" not in manifest or "schema=1" not in manifest:
    raise SystemExit("VERIFY_STAGE: release manifest does not describe the extracted PRX")
print(f"VERIFY_STAGE: manifest and PRX SHA-256 match ({actual})")
print("VERIFY_STAGE: extracted executable payload is complete")
PY
sha256sum "$pkg"
echo "PKG generated and validated: $pkg"
