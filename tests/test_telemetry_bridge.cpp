#include "../platform/ps4/goldhen/plugin/telemetry_bridge.h"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

int main() {
    const auto dir = std::filesystem::temp_directory_path() / "pne-bridge-test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    const auto path = dir.string();
    assert(pne_telemetry_start(path.c_str()) == 1);
    pne_telemetry_on_present(1000, 1000, 0x43555341, 1);
    pne_telemetry_on_present(1016, 1000, 0x43555341, 0);
    const std::vector<std::uint8_t> rgba = {
        255,0,0,255, 0,255,0,255,
        0,0,255,255, 255,255,255,255
    };
    assert(pne_adapter_submit_readback(rgba.data(), 2, 2, 8, 7) == 1);
    assert(pne_adapter_submit_readback(nullptr, 2, 2, 8, 8) == 0);
    pne_telemetry_stop();

    std::ifstream csv(dir / "frametimes.csv");
    const std::string log((std::istreambuf_iterator<char>(csv)), {});
    assert(log.find("frametime_ms") != std::string::npos);
    assert(log.find("16.000000") != std::string::npos);
    std::ifstream bmp(dir / "frame_00000007.bmp", std::ios::binary);
    char sig[2]{};
    bmp.read(sig, 2);
    assert(sig[0] == 'B' && sig[1] == 'M');
    bmp.seekg(0, std::ios::end);
    assert(bmp.tellg() == 70);

    // Restart after a clean stop; this also exercises worker joining.
    assert(pne_telemetry_start(path.c_str()) == 1);
    pne_telemetry_on_present(2000, 1000, 0x43555341, 1);
    pne_telemetry_stop();
    std::filesystem::remove_all(dir, ec);
}
