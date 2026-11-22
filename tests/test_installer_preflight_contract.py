#!/usr/bin/env python3
"""Guard the release preflight against PRX activation and GNM execution."""
from pathlib import Path
import subprocess
import sys
import tempfile

source = Path("installer/preflight_main.c").read_text(encoding="utf-8")
build = Path("tools/build_pkg.sh").read_text(encoding="utf-8")

assert "pne_validate_release_manifest(RELEASE_MANIFEST, SOURCE_PRX" in source
assert "sceKernelSendNotificationRequest" in source
assert "GNM compute exports 7/7" in source
assert "GCN8 assets 6/6" in source
assert "audit_gcn8_assets" in source
assert "pne_sha256_file" in source
assert "/app0/nr_gcn8_" in source
assert "prepare_gcn8_pkg_assets.py" in build
for asset in ("compute_smoke_add", "e4m3_decode", "e4m3_encode", "gemm_fp16_accum", "gemm_fp32", "gemm_fp32_wave64_tiled"):
    assert f"nr_gcn8_{asset}.gcn.bin" in build
assert "nr_gcn8_SHA256SUMS" in build
assert "sceKernelLoadStartModule" in source
assert "sceKernelDlsym" in source
assert "sceKernelGetFsSandboxRandomWord" in source
for export in (
    "sceGnmSubmitDone",
    "sceGnmDispatchInitDefaultHardwareState",
    "sceGnmSetCsShader",
    "sceGnmDispatchDirect",
    "sceGnmFlushGarlic",
    "sceGnmSubmitCommandBuffers",
    "sceGnmSubmitAndFlipCommandBuffers",
):
    assert f'"{export}"' in source

# Address resolution only: no GNM entry point may be invoked.
for forbidden_call in (
    "sceGnmSubmitDone(",
    "sceGnmDispatchInitDefaultHardwareState(",
    "sceGnmSetCsShader(",
    "sceGnmDispatchDirect(",
    "sceGnmFlushGarlic(",
    "sceGnmSubmitCommandBuffers(",
    "sceGnmSubmitAndFlipCommandBuffers(",
):
    assert forbidden_call not in source, f"preflight must not invoke {forbidden_call}"

assert "PRX NOT activated" in source
assert "installer/preflight_main.c" in build
assert 'installer/main.c" -o "$STAGE/installer.o"' not in build
assert "installer/hybrid_state.c" not in build
assert 'PS4 Neural Engine Preflight' in build
assert "installer_version=3" in build
assert "APP_VERSION:-01.09" in build
assert 'mv "$pkg" "$numbered_pkg"' in build, "release output must contain only the canonical numbered PKG"
assert 'cp "$pkg" "$numbered_pkg"' not in build, "do not retain a duplicate generic PKG"

# The app may load the system GNM module into its own process and query exports.
# It must not install/activate NR's PRX or mutate system/user files.
for forbidden in (
    "/data/",
    "plugins.ini",
    "sceKernelMprotect",
    "sceKernelMunmap",
    "scePthreadCreate",
    "sceKernelStopUnloadModule",
    "sceKernelUnloadStartModule",
    "mkdir(",
    "rename(",
    "remove(",
    "unlink(",
    "fopen(",
    "open(",
    "write(",
    "fwrite(",
    "system(",
):
    assert forbidden not in source, f"preflight must not perform {forbidden}"

assert "abi_status=unverified" in build
assert "hardware_status=not_tested" in build
with tempfile.TemporaryDirectory(prefix="nr-gcn8-assets-") as temp:
    output = Path(temp) / "gcn8"
    subprocess.run(
        [sys.executable, "tools/prepare_gcn8_pkg_assets.py", str(output)],
        check=True,
    )
    subprocess.run(
        [sys.executable, "tools/verify_gcn8_assets.py", str(output)],
        check=True,
    )

print("PKG 015 preflight contract: PRX integrity + six GFX8 asset SHA-256 checks + seven process-local GNM compute export resolutions; no GNM calls or NR PRX activation PASS")
