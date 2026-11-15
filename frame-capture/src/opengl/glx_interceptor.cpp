#include <GL/glx.h>
#include "gamestat/event_sender.hpp"
#include <dlfcn.h>
extern "C" void glXSwapBuffers(Display* display, GLXDrawable drawable) {
    using Fn = void(*)(Display*, GLXDrawable);
    static Fn real = reinterpret_cast<Fn>(dlsym(RTLD_NEXT, "glXSwapBuffers"));
    if (!real) return;
    const auto start = gamestat::monotonic_raw_ns();
    real(display, drawable);
    const auto end = gamestat::monotonic_raw_ns();
    gamestat::send_frame_event({"opengl-glx", "swap_buffers", start, end, 0, 1, static_cast<std::uint64_t>(drawable)});
}
