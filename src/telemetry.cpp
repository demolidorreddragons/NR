#include "pne/telemetry.hpp"
#include <algorithm>
#include <limits>
#include <vector>
#include <filesystem>

namespace pne::telemetry {
namespace {
void put16(std::FILE* f, std::uint16_t v) {
    std::fputc(v & 0xff, f); std::fputc((v >> 8) & 0xff, f);
}
void put32(std::FILE* f, std::uint32_t v) {
    put16(f, static_cast<std::uint16_t>(v));
    put16(f, static_cast<std::uint16_t>(v >> 16));
}
}

Writer::Writer(const std::string& directory) : directory_(directory) {}
Writer::~Writer() { stop(); }

bool Writer::start() {
    if (csv_) return true;
    std::error_code ec;
    std::filesystem::create_directories(directory_, ec);
    if (ec) return false;
    const auto path = (std::filesystem::path(directory_) / "frametimes.csv").string();
    csv_ = std::fopen(path.c_str(), "ab");
    if (!csv_) return false;
    if (std::ftell(csv_) == 0)
        std::fputs("ticks,frequency,title_id,display_buffer,delta_ticks,frametime_ms\n", csv_);
    std::fflush(csv_);
    previous_ticks_ = 0;
    return true;
}

void Writer::stop() {
    if (csv_) { std::fflush(csv_); std::fclose(csv_); csv_ = nullptr; }
    previous_ticks_ = 0;
}
bool Writer::active() const noexcept { return csv_ != nullptr; }

bool Writer::append_present(const FrameInfo& i) {
    if (!csv_ || !i.frequency) return false;
    const std::uint64_t delta = previous_ticks_ && i.ticks >= previous_ticks_
        ? i.ticks - previous_ticks_ : 0;
    const double ms = delta ? (1000.0 * static_cast<double>(delta) /
                               static_cast<double>(i.frequency)) : 0.0;
    if (std::fprintf(csv_, "%llu,%llu,%u,%u,%llu,%.6f\n",
        static_cast<unsigned long long>(i.ticks),
        static_cast<unsigned long long>(i.frequency),
        i.title_id, i.display_buffer,
        static_cast<unsigned long long>(delta), ms) < 0) return false;
    previous_ticks_ = i.ticks;
    return std::fflush(csv_) == 0;
}

bool Writer::write_rgba8_bmp(const std::string& filename,
                             const std::uint8_t* pixels,
                             std::uint32_t w, std::uint32_t h,
                             std::size_t pitch) {
    if (!csv_ || !pixels || !w || !h) return false;
    if (w > 16384 || h > 16384) return false;
    const std::uint64_t row_bytes = static_cast<std::uint64_t>(w) * 4;
    if (pitch < row_bytes || pitch > (1ull << 32)) return false;
    const std::uint64_t image_bytes = row_bytes * h;
    if (image_bytes > std::numeric_limits<std::uint32_t>::max() - 54) return false;

    std::FILE* f = std::fopen(filename.c_str(), "wb");
    if (!f) return false;
    std::fputc('B', f); std::fputc('M', f);
    put32(f, static_cast<std::uint32_t>(54 + image_bytes));
    put32(f, 0); put32(f, 54);
    put32(f, 40); put32(f, w); put32(f, h);
    put16(f, 1); put16(f, 32); put32(f, 0);
    put32(f, static_cast<std::uint32_t>(image_bytes));
    put32(f, 2835); put32(f, 2835); put32(f, 0); put32(f, 0);

    std::vector<std::uint8_t> row(static_cast<std::size_t>(row_bytes));
    bool ok = true;
    for (std::uint32_t y = 0; y < h && ok; ++y) {
        const auto* src = pixels + static_cast<std::size_t>(h - 1 - y) * pitch;
        for (std::uint32_t x = 0; x < w; ++x) {
            row[x*4+0] = src[x*4+2]; // R -> B
            row[x*4+1] = src[x*4+1];
            row[x*4+2] = src[x*4+0]; // B -> R
            row[x*4+3] = src[x*4+3];
        }
        ok = std::fwrite(row.data(), 1, row.size(), f) == row.size();
    }
    ok = (std::fclose(f) == 0) && ok;
    return ok;
}
} // namespace pne::telemetry
