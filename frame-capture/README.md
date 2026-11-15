# GameStat Frame Capture (Linux)

Independent C++17 frame-event capture hooks for Vulkan and OpenGL, plus a Python Unix-socket collector/analyzer.

> **Measurement contract:** this project timestamps API presentation calls. It does not claim to measure the exact physical scan-out time or the number of frames actually seen by the user. Vulkan `vkQueuePresentKHR` and OpenGL `glXSwapBuffers` / `eglSwapBuffers` call intervals are useful application-side frame pacing signals, not guaranteed display timestamps.

## Architecture

```text
Game process
  Vulkan: Vulkan loader -> VK_LAYER_GAMestat_frame_capture -> driver
  OpenGL: LD_PRELOAD -> glXSwapBuffers / eglSwapBuffers -> real implementation
                         |
                         | AF_UNIX datagram, JSON schema 1
                         v
                /tmp/gamestat.sock
                         |
                         v
             Python FrameCollector -> FrameAnalyzer -> GameStat backend
```

The native hooks and Python backend are separate processes. Therefore the integration is IPC over a Unix datagram socket, not `pybind11`. This avoids loading Python into the game process and keeps the hook independent of GameStat's internal code.

## Build dependencies (Debian)

```bash
sudo apt install build-essential cmake pkg-config libvulkan-dev \
  libx11-dev libgl-dev libegl-dev
```

Some package names may vary by Debian release. Vulkan, GLX, and EGL targets are detected independently; missing development headers simply disable that target.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Install system-wide (optional):

```bash
sudo cmake --install build
sudo ldconfig
```

## Python collector

```bash
python3 -m venv .venv
. .venv/bin/activate
pip install .
python examples/python_demo.py
```

Start the Python collector **before** launching the game. The socket defaults to `/tmp/gamestat.sock`; override it consistently in both processes with `GAMESTAT_SOCKET=/path/to/socket`.

## Enable Vulkan capture

The layer manifest is installed to the Vulkan implicit layer directory. Enable it for one process with:

```bash
GAMESTAT_VULKAN_CAPTURE=1 GAMESTAT_SOCKET=/tmp/gamestat.sock your-vulkan-game
```

The generated manifest uses the install-prefix library path, so install the layer first. Depending on the Vulkan loader and distribution, implicit layers may need to be enabled in the user's Vulkan layer settings. For explicit testing, point `VK_LAYER_PATH` at the installed manifest directory and use `VK_INSTANCE_LAYERS=VK_LAYER_GAMestat_frame_capture`:

```bash
VK_LAYER_PATH="/usr/local/share/vulkan/implicit_layer.d" \
VK_INSTANCE_LAYERS=VK_LAYER_GAMestat_frame_capture \
GAMESTAT_SOCKET=/tmp/gamestat.sock your-vulkan-game
```

## Enable OpenGL capture

GLX:

```bash
GAMESTAT_SOCKET=/tmp/gamestat.sock \
LD_PRELOAD="$PWD/build/libgamestat_opengl_intercept.so" your-glx-game
```

EGL:

```bash
GAMESTAT_SOCKET=/tmp/gamestat.sock \
LD_PRELOAD="$PWD/build/libgamestat_egl_intercept.so" your-egl-game
```

Do not preload both interceptors blindly. Use the one matching the app's actual context/swap path. Games using unusual dispatch, statically linked GL, or protected runtimes may bypass LD_PRELOAD interception.

## Event schema

One datagram per intercepted call:

```json
{
  "schema": 1,
  "api": "vulkan",
  "event": "queue_present",
  "call_start_ns": 123456789000,
  "call_end_ns": 123456790000,
  "result": 0,
  "presented_swapchain_count": 1,
  "source_id": 12345,
  "pid": 1234,
  "clock": "monotonic_raw"
}
```

Timestamps are nanoseconds from `CLOCK_MONOTONIC_RAW` on the host. They are monotonic but not wall-clock timestamps. `call_start_ns` intervals feed the FPS estimator. `call_end_ns - call_start_ns` is CPU-side duration of the intercepted call, not GPU execution time.

## Current limitations

- Vulkan reports application queue-present calls. Present modes, queueing, compositor scheduling, dropped frames, and multiple swapchains affect the relationship to displayed frames.
- GLX/EGL hooks report swap calls. They do not provide a universal display-engine completion timestamp.
- This version does not yet consume `VK_GOOGLE_display_timing`, `VK_KHR_present_wait`, DRM/KMS page-flip events, or NVIDIA/Intel vendor timing extensions.
- OpenGL interception is implemented for GLX and EGL entry points; games can use other window-system paths.
- Events are best-effort UDP datagrams. If Python is not listening, events are dropped; the game is never blocked waiting for the collector.
- `1% low` and `0.1% low` are computed as 1000 divided by the 99th/99.9th percentile frame interval within the rolling window. Benchmark tools differ in their exact low-FPS definitions.
- The first intercepted frame per API seeds the timestamp; FPS appears after a second event.

## Status

This is an initial independent implementation scaffold. Build and runtime validation must be performed on the target Debian machine with the Vulkan loader and the target game's graphics path. Do not treat successful compilation alone as proof that every title is intercepted.
