#!/usr/bin/env python3
"""Guard the PS4 PKG entry point against implicit PRX deployment/activation."""
from pathlib import Path

source = Path("installer/preflight_main.c").read_text(encoding="utf-8")
build = Path("tools/build_pkg.sh").read_text(encoding="utf-8")

assert "pne_validate_release_manifest(RELEASE_MANIFEST, SOURCE_PRX" in source
assert "sceKernelSendNotificationRequest" in source
assert "PRX NOT installed or activated" in source
assert "installer/preflight_main.c" in build
assert 'installer/main.c" -o "$STAGE/installer.o"' not in build
assert "installer/hybrid_state.c" not in build
assert 'PS4 Neural Engine Preflight' in build
assert "installer_version=2" in build
assert 'mv "$pkg" "$numbered_pkg"' in build, "release output must contain only the canonical numbered PKG"
assert 'cp "$pkg" "$numbered_pkg"' not in build, "do not retain a duplicate generic PKG"

# The executable itself must remain read-only. The validator opens the two
# app0 inputs for reading; it does not mutate the console.
for forbidden in (
    "/data/",
    "plugins.ini",
    "sceKernelLoadStartModule",
    "sceKernelMprotect",
    "sceKernelMunmap",
    "scePthreadCreate",
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
    assert forbidden not in source, f"preflight executable must not perform {forbidden}"

assert "abi_status=unverified" in build
assert "hardware_status=not_tested" in build
print("PKG preflight contract: manifest/PRX read-only verification; no PRX deployment, activation, plugins.ini edit, or console mutation PASS")
