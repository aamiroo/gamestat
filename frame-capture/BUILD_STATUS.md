# Build and verification status (initial delivery)

Verified in the current build container:

- CMake configure succeeds.
- `gamestat_event` common C++ shared library builds.
- `gamestat_event_test` builds.
- Python package byte-compiles.
- Python analyzer unit tests pass.
- End-to-end local IPC smoke test received all 120 synthetic native events and measured approximately 62 FPS.

Not verified in this container:

- Vulkan layer compilation/runtime: Vulkan development headers and loader development package are not installed here.
- GLX interceptor compilation/runtime: GLX/OpenGL development headers are not installed here.
- EGL interceptor compilation/runtime: EGL development headers are not installed here.
- No real game was launched, so game compatibility and runtime overhead have not been measured.

On Debian, install the development dependencies listed in README.md and build again. Treat Vulkan/GLX/EGL capture as pending target-machine validation until those tests pass.
