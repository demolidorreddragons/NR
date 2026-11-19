#include "pne/telemetry.hpp"
#include "../platform/ps4/goldhen/plugin/telemetry_bridge.h"
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>

namespace {
struct Item {
    enum Kind { Present, Frame } kind{};
    pne::telemetry::FrameInfo info{};
    std::uint32_t width{}, height{}, number{};
    std::size_t pitch{};
    std::vector<std::uint8_t> pixels;
};
std::mutex mutex;
std::condition_variable cv;
std::deque<Item> queue;
std::thread worker;
std::atomic<bool> running{false};
std::string root;
constexpr std::size_t kQueueLimit = 32;
std::atomic<std::uint64_t> dropped{0};

void run_worker() {
    pne::telemetry::Writer writer(root);
    if (!writer.start()) { running.store(false); return; }
    for (;;) {
        Item item;
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [] { return !queue.empty() || !running.load(); });
            if (queue.empty() && !running.load()) break;
            item = std::move(queue.front());
            queue.pop_front();
        }
        if (item.kind == Item::Present) {
            writer.append_present(item.info);
        } else {
            char name[64];
            std::snprintf(name, sizeof(name), "frame_%08u.bmp", item.number);
            const auto path = (std::filesystem::path(root) / name).string();
            writer.write_rgba8_bmp(path, item.pixels.data(), item.width,
                                   item.height, item.pitch);
        }
    }
    writer.stop();
}
bool enqueue(Item&& item) {
    if (!running.load(std::memory_order_acquire)) return false;
    std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);
    if (!lock.owns_lock() || queue.size() >= kQueueLimit) {
        dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    queue.emplace_back(std::move(item));
    lock.unlock();
    cv.notify_one();
    return true;
}
}

extern "C" int pne_telemetry_start(const char* directory) {
    if (!directory || !*directory || running.exchange(true)) return 0;
    root = directory;
    {
        std::lock_guard<std::mutex> lock(mutex);
        queue.clear();
    }
    try { worker = std::thread(run_worker); }
    catch (...) { running.store(false); return 0; }
    return 1;
}

extern "C" void pne_telemetry_stop(void) {
    running.store(false, std::memory_order_release);
    cv.notify_all();
    if (worker.joinable()) worker.join();
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
    const std::size_t bytes = row_pitch * static_cast<std::size_t>(height);
    if (bytes > 64u * 1024u * 1024u) return 0;
    Item item;
    item.kind = Item::Frame;
    item.width = width;
    item.height = height;
    item.pitch = row_pitch;
    item.number = frame_number;
    item.pixels.assign(rgba, rgba + bytes);
    return enqueue(std::move(item)) ? 1 : 0;
}
