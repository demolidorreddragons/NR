#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace pne::telemetry {
struct FrameInfo {
    uint64_t ticks;
    uint64_t frequency;
    uint32_t title_id;
    uint32_t display_buffer;
};
class Writer {
public:
    explicit Writer(const char* directory);
    ~Writer();
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    bool start();
    void stop();
    bool active() const;
    bool append_present(const FrameInfo& info);
    bool write_rgba8_bmp(const char* filename, const uint8_t* pixels,
                         uint32_t width, uint32_t height, size_t row_pitch);
private:
    char directory_[1024];
    FILE* csv_;
    uint64_t previous_ticks_;
};
}
