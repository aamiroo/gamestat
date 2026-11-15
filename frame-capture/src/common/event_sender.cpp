#include "gamestat/event_sender.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

namespace gamestat {
std::uint64_t monotonic_raw_ns() noexcept {
    timespec ts{};
#ifdef CLOCK_MONOTONIC_RAW
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0) return 0;
#else
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
#endif
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(ts.tv_nsec);
}

namespace {
struct SocketState {
    int fd{-1};
    sockaddr_un address{};
    bool ready{false};
};
SocketState& socket_state() noexcept {
    static SocketState state;
    static std::once_flag initialized;
    std::call_once(initialized, [] {
        auto& s = state;
        const char* path = std::getenv("GAMESTAT_SOCKET");
        if (!path || !*path) path = "/tmp/gamestat.sock";
        if (std::strlen(path) >= sizeof(s.address.sun_path)) return;
        s.fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
        if (s.fd < 0) return;
        s.address.sun_family = AF_UNIX;
        std::strncpy(s.address.sun_path, path, sizeof(s.address.sun_path) - 1);
        s.ready = true;
    });
    return state;
}
}

bool send_frame_event(const FrameEvent& e) noexcept {
    auto& state = socket_state();
    if (!state.ready) return false;
    char payload[512];
    const int n = std::snprintf(payload, sizeof(payload),
        "{\"schema\":1,\"api\":\"%s\",\"event\":\"%s\",\"call_start_ns\":%llu,\"call_end_ns\":%llu,\"result\":%lld,\"presented_swapchain_count\":%u,\"source_id\":%llu,\"pid\":%ld,\"clock\":\"monotonic_raw\"}",
        e.api ? e.api : "unknown", e.event ? e.event : "present",
        static_cast<unsigned long long>(e.call_start_ns),
        static_cast<unsigned long long>(e.call_end_ns),
        static_cast<long long>(e.result), e.presented_swapchain_count,
        static_cast<unsigned long long>(e.source_id), static_cast<long>(getpid()));
    if (n <= 0 || static_cast<std::size_t>(n) >= sizeof(payload)) return false;
    const auto sent = sendto(state.fd, payload, static_cast<std::size_t>(n), MSG_DONTWAIT,
                             reinterpret_cast<const sockaddr*>(&state.address), sizeof(state.address));
    return sent == n;
}
}
