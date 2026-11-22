#!/usr/bin/env python3
from pathlib import Path

source = Path("platform/ps4/goldhen/plugin/main.c").read_text(encoding="utf-8")
readme = Path("platform/ps4/goldhen/plugin/README.md").read_text(encoding="utf-8")

assert "HOOK32(sceGnmSubmitAndFlipCommandBuffers)" not in source
assert "Detour_Construct(&Detour_sceGnmSubmitAndFlipCommandBuffers, DetourMode_x64)" in source
assert "Detour_DetourFunction(" in source
assert "if (!stub || !Detour_sceGnmSubmitAndFlipCommandBuffers.StubPtr)" in source
assert "sceKernelMunmap(Detour_sceGnmSubmitAndFlipCommandBuffers.TrampolinePtr" in source
assert "static void remove_gnm_hook(void)" in source
assert "HOOK_CONTINUE(sceGnmSubmitAndFlipCommandBuffers" in source

# GoldHEN registers this PRX in [default]. Protect the NR control app from
# loading its own global GNM hook and worker thread. Unknown app identity must
# also fail closed. The guard must execute before any hook or worker startup.
assert '#include <orbis/libkernel.h>' in source
assert 'sceKernelGetAppInfo(getpid(), &app_info)' in source
assert 'PNE_TITLE_ID_CURRENT "PNEU00004"' in source
assert 'PNE_TITLE_ID_LEGACY  "PNEU00001"' in source
guard = source.index('if (skip_neural_rendering_title()) {')
frequency = source.index('g_frequency = sceKernelGetProcessTimeCounterFrequency();')
hook = source.index('if (!install_gnm_hook())')
thread = source.index('scePthreadCreate(&g_input_thread')
assert guard < frequency < hook < thread

assert "control application as well as games" in source
assert "if (g_skip_current_title)" in source
assert "if (g_input_thread_started)" in source
assert "control-app Title IDs" in readme
assert "OrbisAppInfo" in readme
print("GoldHEN PRX contract: x64 detour, cleanup, own-title exclusion and fail-closed app identity PASS")
