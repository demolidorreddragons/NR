from pathlib import Path

build = Path("tools/build_compute_smoke_pkg.sh").read_text()
main = Path("platform/ps4/gcn8/smoke_main.cpp").read_text()
plugin_make = Path("platform/ps4/goldhen/plugin/Makefile").read_text()

assert 'gnm-link)' in build
assert 'gnm-load)' in build
assert 'TITLE_ID="PNEU00004"' in build
assert 'CONTENT_ID="IV0000-PNEU00004_00-GCN8LINK00000000"' in build
assert 'elif [[ "$MODE" == "gnm-link" ]]' in build
branch = build.split('elif [[ "$MODE" == "gnm-link" ]]', 1)[1].split('\nelse\n', 1)[0]
assert '-DPNE_GCN8_GNM_LINK_ONLY=1' in branch
assert '-lSceGnmDriver' in branch
assert 'compute_smoke.cpp' not in branch
assert 'gnm-link' in build and 'GNM_LINK_ONLY_PKG_PAYLOAD=PASS' in build
assert 'gnm-load' in build and 'GNM_LOAD_ONLY_PKG_PAYLOAD=PASS' in build
assert 'reinterpret_cast<std::uintptr_t>(&sceGnmSubmitDone)' in main
assert 'GNM_IMPORT_ADDRESS=0x%llx' in main
assert 'stage_log(import_line)' in main
assert 'GNM_IMPORT_RESOLVED' in main
assert 'GNM_LINK_ONLY_MAIN_REACHED_NO_GNM_CALLS' in main
assert 'RESULT_PASS_GNM_LINK_ONLY' in main
assert '$(OBJDIR)/stage_log.o' in plugin_make
assert 'stage_log.c' in plugin_make
startup = build.split('if [[ "$MODE" == "startup" ]]', 1)[1].split('elif [[ "$MODE" == "gnm-link" ]]', 1)[0]
assert '-lSceGnmDriver' not in startup
assert 'PNE_GCN8_STARTUP_ONLY=1' in startup
print("GCN8 diagnostic mode invariants: PASS")

load_branch = build.split('elif [[ "$MODE" == "gnm-load" ]]; then\n  clang++', 1)[1].split('\nelse\n', 1)[0]
assert '-DPNE_GCN8_GNM_LOAD_ONLY=1' in load_branch
assert '-lkernel' in load_branch
assert '-lSceGnmDriver' not in load_branch
assert 'sceKernelLoadStartModule_then_sceKernelDlsym_no_gnm_call' in build
assert 'PNE_GCN8_GNM_LOAD_ONLY' in main
assert 'sceKernelGetModuleList' in main and 'sceKernelGetModuleInfo' in main
assert 'sceKernelGetFsSandboxRandomWord' in main
assert 'sceKernelLoadStartModule' in main and 'sceKernelDlsym' in main
assert 'GNM_EXPORT_OK sceGnmSubmitDone' in main
assert 'sceGnmDispatchInitDefaultHardwareState' in main
assert 'sceGnmFlushGarlic' in main
assert 'sceGnmSubmitCommandBuffers' in main
assert 'RESULT_PASS_GNM_REQUIRED_EXPORT_AUDIT' in main
assert 'RESULT_FAIL_GNM_REQUIRED_EXPORT_AUDIT' in main
assert 'RESULT_FAIL_GNM_MODULE_LOAD' in main

workflow = Path(".github/workflows/ps4-prx.yml").read_text()
assert 'GCN8_SMOKE_MODE: gnm-link' in workflow
assert 'Build OpenOrbis GNM-Link-Only diagnostic PKG' in workflow
assert 'Independently validate GNM-Link-Only diagnostic PKG' in workflow
assert 'yvZ73uQUqrk#C#C' in workflow
assert 'GNM_LINK_QA_STATUS=PASS_PC_ONLY_NOT_HARDWARE_TESTED' in workflow
assert 'gnm_function_calls=none' in workflow
print("GNM-Link-Only build/independent QA workflow invariants: PASS")

compute = Path("platform/ps4/gcn8/compute_smoke.cpp").read_text()
census = Path("platform/ps4/gcn8/gnm_provider_census.h").read_text()
assert "sceKernelLoadStartModule" not in compute, "compute smoke must never load/start a GNM module"
assert "sceKernelGetFsSandboxRandomWord" not in compute, "compute smoke must not derive a driver load path"
assert "libSceGnmDriver" in census and "libSceGnmDriverForNeoMode" in census
assert "census_loaded_gnm_providers" in compute, "compute smoke must use the tested passive census policy"
assert "ProviderCensusStatus::capacity_exceeded" in census
assert "ProviderCensusStatus::module_info_failed" in census
print("Compute smoke passive GNM census invariants: PASS")
