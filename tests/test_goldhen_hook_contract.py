#!/usr/bin/env python3
from pathlib import Path

source = Path("platform/ps4/goldhen/plugin/main.c").read_text(encoding="utf-8")
sdk_header = Path("platform/ps4/goldhen/plugin/README.md").read_text(encoding="utf-8")

assert "HOOK32(sceGnmSubmitAndFlipCommandBuffers)" not in source
assert "Detour_Construct(&Detour_sceGnmSubmitAndFlipCommandBuffers, DetourMode_x64)" in source
assert "Detour_DetourFunction(" in source
assert "if (!stub || !Detour_sceGnmSubmitAndFlipCommandBuffers.StubPtr)" in source
assert "sceKernelMunmap(Detour_sceGnmSubmitAndFlipCommandBuffers.TrampolinePtr" in source
assert "static void remove_gnm_hook(void)" in source
assert "remove_gnm_hook();" in source
assert "HOOK_CONTINUE(sceGnmSubmitAndFlipCommandBuffers" in source
assert "x64" in source.lower()
print("GoldHEN GNM hook contract: x64 detour, install-result check, trampoline cleanup PASS")
