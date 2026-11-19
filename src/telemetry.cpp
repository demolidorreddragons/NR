#include "pne/telemetry.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sys/stat.h>

namespace pne::telemetry {
namespace {
void put16(FILE* f, uint16_t v) { fputc(v & 0xff, f); fputc((v >> 8) & 0xff, f); }
void put32(FILE* f, uint32_t v) { put16(f, static_cast<uint16_t>(v)); put16(f, static_cast<uint16_t>(v >> 16)); }
}
Writer::Writer(const char* directory) : csv_(nullptr), previous_ticks_(0) {
    if (!directory) directory_[0] = '\0';
    else { std::strncpy(directory_, directory, sizeof(directory_) - 1); directory_[sizeof(directory_) - 1] = '\0'; }
}
Writer::~Writer() { stop(); }
static bool ensure_directories(const char* path) {
    char copy[1024];
    std::strncpy(copy, path, sizeof(copy)-1);
    copy[sizeof(copy)-1] = '\0';
    for (char* p = copy + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(copy, 0777) != 0 && errno != EEXIST) return false;
            *p = '/';
        }
    }
    return mkdir(copy, 0777) == 0 || errno == EEXIST;
}
bool Writer::start() {
    if (csv_) return true;
    if (!directory_[0]) return false;
    if (!ensure_directories(directory_)) return false;
    char path[1200];
    if (std::snprintf(path, sizeof(path), "%s/%s", directory_, "frametimes.csv") >= static_cast<int>(sizeof(path))) return false;
    csv_ = std::fopen(path, "ab");
    if (!csv_) return false;
    if (std::ftell(csv_) == 0) std::fputs("ticks,frequency,title_id,display_buffer,delta_ticks,frametime_ms\n", csv_);
    std::fflush(csv_);
    previous_ticks_ = 0;
    return true;
}
void Writer::stop() {
    if (csv_) { std::fflush(csv_); std::fclose(csv_); csv_ = nullptr; }
    previous_ticks_ = 0;
}
bool Writer::active() const { return csv_ != nullptr; }
bool Writer::append_present(const FrameInfo& i) {
    if (!csv_ || !i.frequency) return false;
    const uint64_t delta = previous_ticks_ && i.ticks >= previous_ticks_ ? i.ticks - previous_ticks_ : 0;
    const double ms = delta ? (1000.0 * static_cast<double>(delta) / static_cast<double>(i.frequency)) : 0.0;
    if (std::fprintf(csv_, "%llu,%llu,%u,%u,%llu,%.6f\n",
        static_cast<unsigned long long>(i.ticks), static_cast<unsigned long long>(i.frequency),
        i.title_id, i.display_buffer, static_cast<unsigned long long>(delta), ms) < 0) return false;
    previous_ticks_ = i.ticks;
    return std::fflush(csv_) == 0;
}
bool Writer::write_rgba8_bmp(const char* filename, const uint8_t* pixels,
                             uint32_t w, uint32_t h, size_t pitch) {
    if (!csv_ || !filename || !pixels || !w || !h || w > 16384 || h > 16384) return false;
    const uint64_t row_bytes = static_cast<uint64_t>(w) * 4;
    if (pitch < row_bytes || pitch > (1ull << 32)) return false;
    const uint64_t image_bytes = row_bytes * h;
    if (image_bytes > std::numeric_limits<uint32_t>::max() - 54) return false;
    FILE* f = std::fopen(filename, "wb");
    if (!f) return false;
    put16(f, 0x4d42); put32(f, static_cast<uint32_t>(54 + image_bytes));
    put32(f, 0); put32(f, 54); put32(f, 40); put32(f, w); put32(f, h);
    put16(f, 1); put16(f, 32); put32(f, 0); put32(f, static_cast<uint32_t>(image_bytes));
    put32(f, 2835); put32(f, 2835); put32(f, 0); put32(f, 0);
    uint8_t* row = static_cast<uint8_t*>(std::malloc(static_cast<size_t>(row_bytes)));
    bool ok = row != nullptr;
    for (uint32_t y = 0; y < h && ok; ++y) {
        const uint8_t* src = pixels + static_cast<size_t>(h - 1 - y) * pitch;
        for (uint32_t x = 0; x < w; ++x) {
            row[x*4] = src[x*4+2]; row[x*4+1] = src[x*4+1];
            row[x*4+2] = src[x*4]; row[x*4+3] = src[x*4+3];
        }
        ok = std::fwrite(row, 1, static_cast<size_t>(row_bytes), f) == row_bytes;
    }
    std::free(row);
    return (std::fclose(f) == 0) && ok;
}
}
