#include <EGL/egl.h>
#include "gamestat/event_sender.hpp"
#include <dlfcn.h>
extern "C" EGLBoolean eglSwapBuffers(EGLDisplay display, EGLSurface surface) {
    using Fn = EGLBoolean(*)(EGLDisplay, EGLSurface);
    static Fn real = reinterpret_cast<Fn>(dlsym(RTLD_NEXT, "eglSwapBuffers"));
    if (!real) return EGL_FALSE;
    const auto start = gamestat::monotonic_raw_ns();
    EGLBoolean result = real(display, surface);
    const auto end = gamestat::monotonic_raw_ns();
    gamestat::send_frame_event({"opengl-egl", "swap_buffers", start, end, result == EGL_TRUE ? 1 : 0, 1, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(surface))});
    return result;
}
