#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace pne::telemetry {

struct FrameInfo {
    std::uint64_t ticks{};
    std::uint64_t frequency{};
    std::uint32_t title_id{};
    std::uint32_t display_buffer{};
};

// Session writes are explicit; callers must never start capture automatically.
class Writer {
public:
    explicit Writer(const std::string& directory);
    ~Writer();
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    bool start();
    void stop();
    bool active() const noexcept;
    bool append_present(const FrameInfo& info);

    // Only call with a CPU-readable RGBA8 staging buffer after a completed GPU
    // readback and cache-coherency operation. Never pass a GPU virtual address.
    bool write_rgba8_bmp(const std::string& filename,
                         const std::uint8_t* pixels,
                         std::uint32_t width,
                         std::uint32_t height,
                         std::size_t row_pitch);

private:
    std::string directory_;
    std::FILE* csv_{};
    std::uint64_t previous_ticks_{};
};

} // namespace pne::telemetry
