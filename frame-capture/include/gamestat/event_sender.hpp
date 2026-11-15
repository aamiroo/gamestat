#pragma once
#include <cstdint>
#include <string>

namespace gamestat {
struct FrameEvent {
    const char* api;
    const char* event;
    std::uint64_t call_start_ns;
    std::uint64_t call_end_ns;
    std::int64_t result;
    std::uint32_t presented_swapchain_count;
    std::uint64_t source_id;
};

std::uint64_t monotonic_raw_ns() noexcept;
// Sends one compact JSON datagram to GAMESTAT_SOCKET (default /tmp/gamestat.sock).
// Non-blocking and best-effort: capture must never stall the game if the collector is absent.
bool send_frame_event(const FrameEvent& event) noexcept;
}
