#!/usr/bin/env bash
set -euo pipefail
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1

: "${OO_PS4_TOOLCHAIN:?OO_PS4_TOOLCHAIN is required}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS="$OO_PS4_TOOLCHAIN/bin/linux"
PKGTOOL="${PKGTOOL_CORE:-$TOOLS/PkgTool.Core}"
RUN_ID="${GITHUB_RUN_ID:-manual}"
if [[ ! "$RUN_ID" =~ ^[A-Za-z0-9_-]+$ ]]; then
  echo "Invalid GITHUB_RUN_ID" >&2
  exit 1
fi
MODE="${GCN8_SMOKE_MODE:-compute}"
case "$MODE" in
  compute)
    TITLE_ID="PNEU00002"
    CONTENT_ID="IV0000-PNEU00002_00-GCN8SMOKE0000000"
    APP_TITLE="Neural Rendering GCN8 Smoke Test"
    ;;
  startup)
    TITLE_ID="PNEU00003"
    CONTENT_ID="IV0000-PNEU00003_00-GCN8STARTUP00000"
    APP_TITLE="Neural Rendering Startup Probe"
    ;;
  *) echo "Unsupported GCN8_SMOKE_MODE: $MODE" >&2; exit 1 ;;
esac
if [[ ${#CONTENT_ID} -ne 36 || ! "$CONTENT_ID" =~ ^[A-Z]{2}[0-9]{4}-[A-Z0-9]{4}[0-9]{5}_00-[A-Z0-9]{16}$ ]]; then
  echo "Invalid PS4 CONTENT_ID (expected 36 characters and canonical format): $CONTENT_ID" >&2
  exit 1
fi
APP_VERSION="01.00"
STAGE="$ROOT/build/gcn8-smoke-stage/$RUN_ID/$MODE"
OUT="$ROOT/dist/gcn8-smoke/$RUN_ID/$MODE"
SAMPLE="$OO_PS4_TOOLCHAIN/samples/piglet"
OPEN_GNM_INCLUDE="${OPEN_GNM_INCLUDE:-$ROOT/OpenGNM/include}"
test -s "$OPEN_GNM_INCLUDE/gnm_drawcommandbuffer.h"
mkdir -p "$ROOT/build/gcn8-smoke-stage" "$ROOT/dist/gcn8-smoke"
rm -rf "$STAGE"
mkdir -p "$STAGE/sce_sys/about" "$STAGE/sce_module" "$OUT"

if [[ "$MODE" == "compute" ]]; then
  : "${GCN8_SHADER_FILE:?GCN8_SHADER_FILE must point to the compiled compute_smoke_add.sb}"
  test -s "$GCN8_SHADER_FILE"
  cp "$GCN8_SHADER_FILE" "$STAGE/compute_smoke_add.sb"
  SHADER_SHA256="$(sha256sum "$STAGE/compute_smoke_add.sb" | awk '{print $1}')"
else
  SHADER_SHA256="none"
fi

for required in "$SAMPLE/sce_module/libc.prx" \
                "$SAMPLE/sce_module/libSceFios2.prx" \
                "$SAMPLE/sce_sys/about/right.sprx"; do
  test -s "$required" || {
    echo "Missing OpenOrbis runtime support file: $required" >&2
    exit 1
  }
done
cp "$SAMPLE/sce_module/libc.prx" "$STAGE/sce_module/libc.prx"
cp "$SAMPLE/sce_module/libSceFios2.prx" "$STAGE/sce_module/libSceFios2.prx"
cp "$SAMPLE/sce_sys/about/right.sprx" "$STAGE/sce_sys/about/right.sprx"
python3 "$ROOT/tools/make_icon.py" "$STAGE/sce_sys/icon0.png"

COMMON=(--target=x86_64-pc-freebsd12-elf -std=c++17 -g -fPIC -fno-exceptions -fno-rtti \
  -DOPENGNM_REQUIRE_ABI=1 -isysroot "$OO_PS4_TOOLCHAIN" \
  -isystem "$OO_PS4_TOOLCHAIN/include" \
  -isystem /usr/include/x86_64-linux-gnu \
  -isystem "$OO_PS4_TOOLCHAIN/include/c++/v1" \
  -I"$OPEN_GNM_INCLUDE" -I"$ROOT/platform/ps4/gcn8" \
  -I"$ROOT/platform/ps4/gcn8/host")
RUN_DEFINE="-DPNE_GCN8_SMOKE_RUN_ID=\"$RUN_ID\""
LOGGER_FLAGS=()
if [[ "${GCN8_LOG_UDP:-0}" == "1" ]]; then
  LOGGER_FLAGS+=(-DSTAGE_LOG_UDP "-DPNE_GCN8_LOG_UDP_IP=\"${GCN8_LOG_UDP_IP:-}\"" "-DPNE_GCN8_LOG_UDP_PORT=${GCN8_LOG_UDP_PORT:-9999}")
fi
COMMON_C=(--target=x86_64-pc-freebsd12-elf -std=c11 -g -fPIC -isysroot "$OO_PS4_TOOLCHAIN" -isystem "$OO_PS4_TOOLCHAIN/include" -I"$ROOT/platform/ps4/gcn8")
clang "${COMMON_C[@]}" "${LOGGER_FLAGS[@]}" -c "$ROOT/platform/ps4/gcn8/stage_log.c" -o "$STAGE/stage_log.o"
if [[ "$MODE" == "startup" ]]; then
  clang++ "${COMMON[@]}" "${LOGGER_FLAGS[@]}" "$RUN_DEFINE" -DPNE_GCN8_STARTUP_ONLY=1 -c "$ROOT/platform/ps4/gcn8/smoke_main.cpp" -o "$STAGE/smoke_main.o"
  # Deliberately omit every GNM/compute object and import from this probe.
  ld.lld -m elf_x86_64 -pie --script "$OO_PS4_TOOLCHAIN/link.x" --eh-frame-hdr -L"$OO_PS4_TOOLCHAIN/lib" "$STAGE/smoke_main.o" "$STAGE/stage_log.o" "$OO_PS4_TOOLCHAIN/lib/crt1.o" -o "$STAGE/smoke.elf" -Map="$STAGE/smoke.map" -lc -lc++ -lkernel
else
  clang++ "${COMMON[@]}" "${LOGGER_FLAGS[@]}" "$RUN_DEFINE" -c "$ROOT/platform/ps4/gcn8/smoke_main.cpp" -o "$STAGE/smoke_main.o"
  clang++ "${COMMON[@]}" -c "$ROOT/platform/ps4/gcn8/compute_smoke.cpp" -o "$STAGE/compute_smoke.o"
  clang++ "${COMMON[@]}" -c "$ROOT/platform/ps4/gcn8/gnm_resource_binding.cpp" -o "$STAGE/gnm_resource_binding.o"
  clang++ "${COMMON[@]}" -c "$ROOT/platform/ps4/gcn8/host/resource_table_contract.cpp" -o "$STAGE/resource_table_contract.o"
  clang++ "${COMMON[@]}" -c "$ROOT/platform/ps4/gcn8/host/shader_binary_contract.cpp" -o "$STAGE/shader_binary_contract.o"
  ld.lld -m elf_x86_64 -pie --script "$OO_PS4_TOOLCHAIN/link.x" --eh-frame-hdr -L"$OO_PS4_TOOLCHAIN/lib" "$STAGE/smoke_main.o" "$STAGE/compute_smoke.o" "$STAGE/gnm_resource_binding.o" "$STAGE/resource_table_contract.o" "$STAGE/shader_binary_contract.o" "$STAGE/stage_log.o" "$OO_PS4_TOOLCHAIN/lib/crt1.o" -o "$STAGE/smoke.elf" -Map="$STAGE/smoke.map" -lc -lc++ -lkernel -lSceGnmDriver
fi
test -s "$STAGE/smoke.elf"
readelf -h "$STAGE/smoke.elf" | grep -Eq 'Class:[[:space:]]+ELF64'
readelf -h "$STAGE/smoke.elf" | grep -Eq 'Machine:[[:space:]]+Advanced Micro Devices X86-64'
readelf -h "$STAGE/smoke.elf" | grep -Eq 'Entry point address:[[:space:]]+0x[1-9a-fA-F]'

(
  cd "$STAGE"
  AUTHINFO="000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
  "$TOOLS/create-fself" -in=smoke.elf -out=smoke.oelf \
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
sfo TITLE Utf8 128 "$APP_TITLE"
sfo TITLE_ID Utf8 12 "$TITLE_ID"
sfo VERSION Utf8 8 "$APP_VERSION"
sfo PUBTOOLINFO Utf8 512 "c_date=$(date -u +%Y%m%d),img0_l0_size=7,img0_l1_size=0,img0_sc_ksize=512,img0_pc_ksize=832,st_type=digital50"

cd "$STAGE"
PAYLOAD_FILES="eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx sce_module/libc.prx sce_module/libSceFios2.prx"
if [[ "$MODE" == "compute" ]]; then
  PAYLOAD_FILES="$PAYLOAD_FILES compute_smoke_add.sb"
fi
"$TOOLS/create-gp4" -out package.gp4 --content-id="$CONTENT_ID" --files "$PAYLOAD_FILES"
"$PKGTOOL" pkg_build package.gp4 "$OUT"
pkg="$(find "$OUT" -maxdepth 1 -type f -name '*.pkg' -print -quit)"
test -n "$pkg" && test -s "$pkg"
numbered_pkg="$OUT/PS4-Neural-Rendering-GCN8-${MODE}-${RUN_ID}.pkg"
cp "$pkg" "$numbered_pkg"
"$PKGTOOL" pkg_validate --verbose "$numbered_pkg"
VERIFY="$STAGE/pkg-verify"
rm -rf "$VERIFY" && mkdir -p "$VERIFY"
"$PKGTOOL" pkg_extract --verbose "$numbered_pkg" "$VERIFY"

python3 - "$VERIFY" "$MODE" "$SHADER_SHA256" <<'PY'
import hashlib, os, sys
root, mode, expected_shader = sys.argv[1:]
found = {}
for current, _, files in os.walk(root):
    for name in files:
        found[name] = os.path.join(current, name)
required = {"eboot.bin", "libc.prx", "libSceFios2.prx", "right.sprx"}
if mode == "compute":
    required.add("compute_smoke_add.sb")
missing = sorted(required - found.keys())
if missing:
    raise SystemExit("Missing extracted payload: " + ", ".join(missing))
if mode == "compute":
    with open(found["compute_smoke_add.sb"], "rb") as stream:
        actual = hashlib.sha256(stream.read()).hexdigest()
    if actual != expected_shader:
        raise SystemExit(f"Shader payload SHA-256 mismatch: {actual} != {expected_shader}")
    print("COMPUTE_SMOKE_PKG_PAYLOAD=PASS")
else:
    if "compute_smoke_add.sb" in found:
        raise SystemExit("Startup-only PKG unexpectedly contains compute_smoke_add.sb")
    print("STARTUP_ONLY_PKG_PAYLOAD=PASS; COMPUTE_SHADER=ABSENT")
PY

PKG_SHA256="$(sha256sum "$numbered_pkg" | awk '{print $1}')"
cp "$STAGE/smoke.elf" "$OUT/smoke.elf"
cp "$STAGE/smoke.map" "$OUT/smoke.map"
ELF_SHA256="$(sha256sum "$OUT/smoke.elf" | awk '{print $1}')"
MAP_SHA256="$(sha256sum "$OUT/smoke.map" | awk '{print $1}')"
cat > "$OUT/SMOKE_PKG_MANIFEST.txt" <<EOF
schema=1
mode=$MODE
run_id=$RUN_ID
title_id=$TITLE_ID
content_id=$CONTENT_ID
app_version=$APP_VERSION
shader_file=$( [[ "$MODE" == "compute" ]] && echo compute_smoke_add.sb || echo none )
shader_sha256=$SHADER_SHA256
pkg_file=$(basename "$numbered_pkg")
pkg_sha256=$PKG_SHA256
pointer_semantics=psbc_source_confirmed_orbshdr_minus_chunkusagebaseoffsetdwords
elf_file=smoke.elf
elf_sha256=$ELF_SHA256
map_file=smoke.map
map_sha256=$MAP_SHA256
hardware_status=not_run
EOF
cp "$STAGE/package.gp4" "$OUT/package.gp4"
cp "$STAGE/sce_sys/param.sfo" "$OUT/param.sfo"
sha256sum "$numbered_pkg" "$OUT/SMOKE_PKG_MANIFEST.txt" "$OUT/package.gp4"
echo "GCN8_SMOKE_MODE=$MODE"
echo "GCN8_SMOKE_PKG=$numbered_pkg"
echo "GCN8_SMOKE_PKG_SHA256=$PKG_SHA256"
