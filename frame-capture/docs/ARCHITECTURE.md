# Architecture and measurement semantics

## Why IPC instead of pybind11

A Vulkan layer or OpenGL preload library executes inside the game process. The GameStat Python backend is a different process. Embedding Python into each game would add complexity, runtime coupling, and crash surface. The native hooks therefore emit compact Unix datagrams; Python owns collection, analysis, buffering, and UI integration.

## Data flow

1. Hook records `CLOCK_MONOTONIC_RAW` immediately before the intercepted presentation function.
2. Hook calls the next implementation exactly once.
3. Hook records a second timestamp immediately after return.
4. Hook sends a small schema-versioned JSON datagram using a non-blocking Unix datagram socket.
5. Python validates the schema and computes intervals between successive `call_start_ns` values for each API/process/source stream.

## What each timestamp means

- `call_start_ns`: application-side start time of the present/swap call.
- `call_end_ns`: return time from that call.
- `call_end_ns - call_start_ns`: CPU-side duration spent in the call, which may include blocking/throttling and must not be interpreted as GPU execution time.
- Difference between successive `call_start_ns`: present-call interval, a useful estimate of application frame pacing.
- Actual display/scan-out time: not measured by this initial implementation.

## Threading and overhead

The hot path uses a monotonic clock, a short native call, and a non-blocking datagram send. The current sender opens a datagram socket for each event for simple failure isolation; this is functional but not optimal for the lowest possible overhead. A later optimization can maintain a per-process socket with thread-safe lifecycle management after profiling. Events are dropped rather than blocking the game when no collector is present.

## Known engineering follow-ups

- Validate Vulkan loader-chain behavior against the target loader and Khronos validation layers.
- Test device/queue dispatch behavior with multiple VkDevices and multiple queues.
- Add explicit teardown and multi-instance stress tests.
- Add optional Vulkan display timing extensions where supported.
- Add DRM/KMS page-flip correlation if the product needs display-completion metrics.
- Test GLX/EGL interposition under Proton/Wine and common SDL/GLFW paths.
- Consider a binary fixed-size event payload if JSON formatting cost is measurable.
