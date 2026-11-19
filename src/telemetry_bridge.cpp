#include "pne/telemetry.hpp"
#include "../platform/ps4/goldhen/plugin/telemetry_bridge.h"
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
struct Item {
    enum Kind { Present, Frame } kind{};
    pne::telemetry::FrameInfo info{};
    std::uint32_t width{}, height{}, number{};
    std::size_t pitch{};
    std::vector<std::uint8_t> pixels;
};
std::mutex lifecycle_mutex, mutex;
std::condition_variable cv, startup_cv;
std::deque<Item> queue;
std::thread worker;
std::atomic<bool> running{false};
std::string root;
std::size_t queued_frame_bytes = 0;
bool startup_done = false, startup_ok = false;
constexpr std::size_t kQueueLimit = 8;
constexpr std::size_t kFrameQueueByteLimit = 64u * 1024u * 1024u;
std::atomic<std::uint64_t> dropped{0};

void run_worker() {
    pne::telemetry::Writer writer(root);
    const bool opened = writer.start();
    {
        std::lock_guard<std::mutex> lock(mutex);
        startup_ok = opened;
        startup_done = true;
        if (!opened) running.store(false, std::memory_order_release);
    }
    startup_cv.notify_all();
    if (!opened) return;

    for (;;) {
        Item item;
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [] { return !queue.empty() || !running.load(std::memory_order_acquire); });
            if (queue.empty() && !running.load(std::memory_order_acquire)) break;
            item = std::move(queue.front());
            queue.pop_front();
            if (item.kind == Item::Frame) queued_frame_bytes -= item.pixels.size();
        }
        if (item.kind == Item::Present) {
            (void)writer.append_present(item.info);
        } else {
            char name[64];
            std::snprintf(name, sizeof(name), "frame_%08u.bmp", item.number);
            const auto path = (std::filesystem::path(root) / name).string();
            (void)writer.write_rgba8_bmp(path, item.pixels.data(), item.width,
                                         item.height, item.pitch);
        }
    }
    writer.stop();
}

bool enqueue(Item&& item) noexcept {
    if (!running.load(std::memory_order_acquire)) return false;
    try {
        std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
        if (!lock.owns_lock() || queue.size() >= kQueueLimit ||
            (item.kind == Item::Frame &&
             item.pixels.size() > kFrameQueueByteLimit - queued_frame_bytes)) {
            dropped.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        if (item.kind == Item::Frame) queued_frame_bytes += item.pixels.size();
        queue.emplace_back(std::move(item));
        lock.unlock();
        cv.notify_one();
        return true;
    } catch (...) {
        dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
}
}

extern "C" int pne_telemetry_start(const char* directory) {
    if (!directory || !*directory) return 0;
    std::lock_guard<std::mutex> lifecycle(lifecycle_mutex);
    if (running.load(std::memory_order_acquire)) return 0;
    if (worker.joinable()) worker.join();
    {
        std::lock_guard<std::mutex> lock(mutex);
        queue.clear();
        queued_frame_bytes = 0;
        startup_done = false;
        startup_ok = false;
        root = directory;
    }
    running.store(true, std::memory_order_release);
    try {
        worker = std::thread(run_worker);
    } catch (...) {
        running.store(false, std::memory_order_release);
        return 0;
    }
    std::unique_lock<std::mutex> lock(mutex);
    startup_cv.wait(lock, [] { return startup_done; });
    const bool ok = startup_ok;
    lock.unlock();
    if (!ok && worker.joinable()) worker.join();
    return ok ? 1 : 0;
}

extern "C" void pne_telemetry_stop(void) {
    std::lock_guard<std::mutex> lifecycle(lifecycle_mutex);
    running.store(false, std::memory_order_release);
    cv.notify_all();
    if (worker.joinable()) worker.join();
    std::lock_guard<std::mutex> lock(mutex);
    queue.clear();
    queued_frame_bytes = 0;
}

extern "C" void pne_telemetry_on_present(std::uint64_t ticks,
    std::uint64_t frequency, std::uint32_t title_id,
    std::uint32_t display_buffer) {
    Item item;
    item.kind = Item::Present;
    item.info = {ticks, frequency, title_id, display_buffer};
    (void)enqueue(std::move(item));
}

extern "C" int pne_adapter_submit_readback(const std::uint8_t* rgba,
    std::uint32_t width, std::uint32_t height, std::size_t row_pitch,
    std::uint32_t frame_number) {
    if (!rgba || !width || !height || width > 4096 || height > 4096 ||
        row_pitch < static_cast<std::size_t>(width) * 4 ||
        row_pitch > (1u << 24)) return 0;
    if (height > (64u * 1024u * 1024u) / row_pitch) return 0;
    const std::size_t bytes = row_pitch * static_cast<std::size_t>(height);
    Item item;
    item.kind = Item::Frame;
    item.width = width;
    item.height = height;
    item.pitch = row_pitch;
    item.number = frame_number;
    try { item.pixels.assign(rgba, rgba + bytes); }
    catch (...) { return 0; }
    return enqueue(std::move(item)) ? 1 : 0;
}
