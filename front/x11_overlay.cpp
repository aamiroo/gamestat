#include "x11_overlay.h"

#ifdef GAMESTAT_HAVE_X11

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

namespace x11_overlay {

namespace {

Display* g_display = nullptr;
::Window g_window = 0;
bool g_active = false;
std::chrono::steady_clock::time_point g_last_raise;
constexpr std::chrono::milliseconds kRaiseInterval{250};

// GAMESTAT_X11_OVERRIDE_REDIRECT=0 restores the plain managed (Always-on-Top) window.
bool enabled_by_env() {
    const char* v = std::getenv("GAMESTAT_X11_OVERRIDE_REDIRECT");
    return !(v && std::strcmp(v, "0") == 0);
}

bool running_on_x11() {
#ifdef GLFW_PLATFORM_X11
    return glfwGetPlatform() == GLFW_PLATFORM_X11;  // GLFW >= 3.4 can also be on Wayland
#else
    return true;  // older GLFW is X11-only on Linux
#endif
}

}  // namespace

bool prepare(GLFWwindow* window) {
    g_active = false;
    if (!window || !enabled_by_env() || !running_on_x11()) return false;

    Display* dpy = glfwGetX11Display();
    ::Window win = glfwGetX11Window(window);
    if (!dpy || !win) return false;

    // override-redirect has to be set while the window is still unmapped, otherwise the
    // window manager has already taken it over.
    XWindowAttributes attr;
    if (!XGetWindowAttributes(dpy, win, &attr) || attr.map_state != IsUnmapped) {
        std::cerr << "X11: overlay window is already mapped, cannot make it override-redirect "
                     "(create it with GLFW_VISIBLE=false)" << std::endl;
        return false;
    }

    XSetWindowAttributes set_attr;
    set_attr.override_redirect = True;
    XChangeWindowAttributes(dpy, win, CWOverrideRedirect, &set_attr);

    // An override-redirect window gets no WM help, so make sure input really goes through it:
    // an empty ShapeInput region means every pointer event reaches the window below
    // (same thing GLFW_MOUSE_PASSTHROUGH does; asserting it here keeps click-through
    // independent of the GLFW version).
    int shape_event = 0, shape_error = 0;
    if (XShapeQueryExtension(dpy, &shape_event, &shape_error)) {
        XShapeCombineRectangles(dpy, win, ShapeInput, 0, 0, nullptr, 0, ShapeSet, Unsorted);
    } else {
        std::cerr << "X11: SHAPE extension missing, click-through relies on GLFW_MOUSE_PASSTHROUGH"
                  << std::endl;
    }
    XFlush(dpy);

    g_display = dpy;
    g_window = win;
    g_active = true;
    g_last_raise = std::chrono::steady_clock::now();
    std::cerr << "X11: overlay window is override-redirect (above fullscreen windows, click-through)"
              << std::endl;
    return true;
}

void keep_on_top() {
    if (!g_active) return;
    const auto now = std::chrono::steady_clock::now();
    if (now - g_last_raise < kRaiseInterval) return;
    g_last_raise = now;
    // No-op for the X server when the window is already on top.
    XRaiseWindow(g_display, g_window);
    XFlush(g_display);
}

}  // namespace x11_overlay

#else  // !GAMESTAT_HAVE_X11

namespace x11_overlay {
bool prepare(GLFWwindow*) { return false; }
void keep_on_top() {}
}  // namespace x11_overlay

#endif
