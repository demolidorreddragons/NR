#!/usr/bin/env bash
set -euo pipefail
: "\${OO_PS4_TOOLCHAIN:?OO_PS4_TOOLCHAIN is required}"
: "\${PKGTOOL_CORE:?PKGTOOL_CORE is required}"
: "\${BASE_PKG:?BASE_PKG is required}"
: "\${UPDATE_PKG:?UPDATE_PKG is required}"
: "\${UPDATE_PRX:?UPDATE_PRX is required}"
: "\${UPDATE_MANIFEST:?UPDATE_MANIFEST is required}"
: "\${OUT_DIR:?OUT_DIR is required}"
TITLE_ID="PNEU00001"
CONTENT_ID="IV0000-PNEU00001_00-PS4NEURALENG0000"
BASE_APP_VERSION="01.06"
PATCH_APP_VERSION="\${PATCH_APP_VERSION:-01.07}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$ROOT/build/patch-stage"
TOOLS="$OO_PS4_TOOLCHAIN/bin/linux"
[[ "$PATCH_APP_VERSION" =~ ^[0-9]{2}\.[0-9]{2}$ ]] || { echo "Invalid PATCH_APP_VERSION" >&2; exit 1; }
[[ "$PATCH_APP_VERSION" > "$BASE_APP_VERSION" ]] || { echo "Patch version must be newer than installed base $BASE_APP_VERSION" >&2; exit 1; }
test -s "$BASE_PKG" && test -s "$UPDATE_PKG" && test -s "$UPDATE_PRX" && test -s "$UPDATE_MANIFEST"
BASE_PKG_SHA256="015c9e2c266561fda005c15766fe4833412c658d9a8d0b7334fd2cfe3fec8ae2"
actual_base_sha="$(sha256sum "$BASE_PKG" | awk '{print $1}')"
[[ "$actual_base_sha" == "$BASE_PKG_SHA256" ]] || { echo "Base PKG SHA mismatch" >&2; exit 1; }
rm -rf "$WORK"
mkdir -p "$WORK/extracted-update" "$WORK/sce_sys" "$OUT_DIR"
"$PKGTOOL_CORE" pkg_validate --verbose "$BASE_PKG"
"$PKGTOOL_CORE" pkg_validate --verbose "$UPDATE_PKG"
"$PKGTOOL_CORE" pkg_extract --verbose "$UPDATE_PKG" "$WORK/extracted-update"
test -s "$WORK/extracted-update/eboot.bin"
cp "$WORK/extracted-update/eboot.bin" "$WORK/eboot.bin"
cp "$UPDATE_PRX" "$WORK/ps4_neural_engine.prx"
cp "$UPDATE_MANIFEST" "$WORK/pne-release.ini"
"$PKGTOOL_CORE" sfo_new "$WORK/sce_sys/param.sfo"
sfo() { "$PKGTOOL_CORE" sfo_setentry "$WORK/sce_sys/param.sfo" "$1" --type "$2" --maxsize "$3" --value "$4"; }
sfo APP_TYPE Integer 4 1
sfo APP_VER Utf8 8 "$PATCH_APP_VERSION"
sfo ATTRIBUTE Integer 18 0
sfo CATEGORY Utf8 4 gpe
sfo FORMAT Utf8 4 obs
sfo CONTENT_ID Utf8 48 "$CONTENT_ID"
sfo DOWNLOAD_DATA_SIZE Integer 4 0
sfo SYSTEM_VER Integer 4 1020
sfo TITLE Utf8 128 "PS4 Neural Engine Installer"
sfo TITLE_ID Utf8 12 "$TITLE_ID"
sfo VERSION Utf8 8 "$PATCH_APP_VERSION"
(
 cd "$WORK"
 "$TOOLS/create-gp4" -out package.gp4 --content-id="$CONTENT_ID" --files "eboot.bin ps4_neural_engine.prx pne-release.ini sce_sys/param.sfo"
)
python3 - "$WORK/package.gp4" "$CONTENT_ID" <<'PY'
import sys
import xml.etree.ElementTree as ET
path, content_id = sys.argv[1:]
tree = ET.parse(path)
root = tree.getroot()
volume = root.find("volume")
if volume is None: raise SystemExit("GP4 has no volume element")
vtype, package = volume.find("volume_type"), volume.find("package")
if vtype is None or package is None: raise SystemExit("GP4 metadata incomplete")
vtype.text = "pkg_ps4_patch"
if package.get("content_id") != content_id: raise SystemExit("GP4 CONTENT_ID mismatch")
package.set("storage_type", "digital25")
package.set("app_type", "full")
tree.write(path, encoding="utf-8", xml_declaration=True)
check = ET.parse(path).getroot()
assert check.findtext("volume/volume_type") == "pkg_ps4_patch"
assert check.find("volume/package").get("storage_type") == "digital25"
print("PATCH_GP4_METADATA=PASS")
PY
"$PKGTOOL_CORE" pkg_build "$WORK/package.gp4" "$OUT_DIR"
pkg="$(find "$OUT_DIR" -maxdepth 1 -type f -name '*.pkg' -print -quit)"
test -n "$pkg" && test -s "$pkg"
final="$OUT_DIR/PS4-Neural-Engine-011-PATCH.pkg"
cp "$pkg" "$final"
"$PKGTOOL_CORE" pkg_validate --verbose "$final"
sha256sum "$final"
echo "PATCH_CANDIDATE=$final"
echo "PATCH_BASE_VERSION=$BASE_APP_VERSION"
echo "PATCH_APP_VERSION=$PATCH_APP_VERSION"
echo "PATCH_BASE_SHA256=$actual_base_sha"
