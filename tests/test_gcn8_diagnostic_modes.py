from pathlib import Path

build = Path("tools/build_compute_smoke_pkg.sh").read_text()
main = Path("platform/ps4/gcn8/smoke_main.cpp").read_text()

assert 'gnm-link)' in build
assert 'TITLE_ID="PNEU00004"' in build
assert 'CONTENT_ID="IV0000-PNEU00004_00-GCN8LINK00000000"' in build
assert 'elif [[ "$MODE" == "gnm-link" ]]' in build
branch = build.split('elif [[ "$MODE" == "gnm-link" ]]', 1)[1].split('\nelse\n', 1)[0]
assert '-DPNE_GCN8_GNM_LINK_ONLY=1' in branch
assert '-lSceGnmDriver' in branch
assert 'compute_smoke.cpp' not in branch
assert 'gnm-link' in build and 'GNM_LINK_ONLY_PKG_PAYLOAD=PASS' in build
assert 'reinterpret_cast<std::uintptr_t>(&sceGnmSubmitDone)' in main
assert 'GNM_IMPORT_RESOLVED' in main
assert 'GNM_LINK_ONLY_MAIN_REACHED_NO_GNM_CALLS' in main
assert 'RESULT_PASS_GNM_LINK_ONLY' in main
startup = build.split('if [[ "$MODE" == "startup" ]]', 1)[1].split('elif [[ "$MODE" == "gnm-link" ]]', 1)[0]
assert '-lSceGnmDriver' not in startup
assert 'PNE_GCN8_STARTUP_ONLY=1' in startup
print("GCN8 diagnostic mode invariants: PASS")
