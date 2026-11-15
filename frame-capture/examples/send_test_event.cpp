#include "gamestat/event_sender.hpp"
#include <thread>
#include <chrono>
int main() {
    for (int i=0; i<120; ++i) {
        auto start=gamestat::monotonic_raw_ns();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        auto end=gamestat::monotonic_raw_ns();
        gamestat::send_frame_event({"test", "swap_buffers", start, end, 0, 1, 1});
    }
}
