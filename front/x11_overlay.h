#pragma once

// X11-only window tweaks that let the overlay stay visible above fullscreen games.
//
// Why this exists: a normal (window-manager-managed) window is placed in a stacking layer
// by the WM. KWin puts an *active fullscreen* window in a layer above "keep above"
// (_NET_WM_STATE_ABOVE) windows, so Always-on-Top can never win against a fullscreen game.
// An override-redirect window is not managed at all: KWin keeps it in its topmost
// "unmanaged" layer and orders it using the X server's stacking order.
//
// Everything here is a no-op (returns false / does nothing) on non-X11 platforms, when the
// build has no X11 support, or when GAMESTAT_X11_OVERRIDE_REDIRECT=0 is set.

struct GLFWwindow;

namespace x11_overlay {

// Call right after glfwCreateWindow() and BEFORE the window is first shown (create it with
// GLFW_VISIBLE = GLFW_FALSE). Marks the window override-redirect and makes its input region
// empty so that clicks always pass through to the game. Returns true if it was applied.
bool prepare(GLFWwindow* window);

// Call regularly from the main loop. Cheap: re-raises the window to the top of the X
// stacking order a few times per second, so a game window mapped after the overlay
// (e.g. a Wine/Proton fullscreen window, which is itself override-redirect) cannot cover it.
void keep_on_top();

}  // namespace x11_overlay
