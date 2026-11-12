# GameStat

<p align="center">
  <strong>A lightweight hardware monitoring overlay for Linux gaming.</strong>
  <br />
  Monitor your CPU, GPU, RAM, and system performance while playing games.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Linux-blue?style=flat-square" alt="Linux" />
  <img src="https://img.shields.io/badge/Backend-Python-yellow?style=flat-square" alt="Python" />
  <img src="https://img.shields.io/badge/Frontend-C%2B%2B-blue?style=flat-square" alt="C++" />
  <img src="https://img.shields.io/badge/Graphics-Vulkan-red?style=flat-square" alt="Vulkan" />
  <img src="https://img.shields.io/badge/License-MIT-green?style=flat-square" alt="MIT License" />
</p>

---

## Overview

**GameStat** is a lightweight hardware monitoring application designed to display real-time system statistics through a compact, gaming-oriented overlay.

It allows Linux gamers to monitor important hardware metrics without constantly switching away from their games.

The project consists of two main components:

- **Python Backend:** Collects hardware telemetry and provides structured monitoring data.
- **C++ Frontend:** Receives telemetry and renders the overlay using Vulkan and Dear ImGui.

The interface follows a compact, MangoHud-inspired visual style, designed to minimize screen obstruction during gameplay.

## Features

- Real-time CPU temperature and utilization monitoring.
- RAM usage, available memory, and total memory monitoring.
- NVIDIA GPU temperature and utilization monitoring.
- VRAM usage, available memory, and total memory monitoring.
- GPU memory controller utilization.
- GPU graphics clock monitoring.
- GPU power consumption monitoring.
- Compact MangoHud-inspired overlay interface.
- Automatic backend startup and communication.
- Asynchronous telemetry updates.
- Automatic overlay sizing based on displayed content.
- Stale-data detection when backend updates stop.
- Vulkan-based rendering.
- Designed for low resource consumption.

## Hardware Metrics

| Component | Metrics |
|---|---|
| CPU | Temperature, utilization |
| RAM | Total, used, available, usage percentage |
| NVIDIA GPU | Temperature, utilization |
| VRAM | Total, used, free, usage percentage |
| GPU | Memory controller utilization, graphics clock, power consumption |

> FPS monitoring is not included in the current implementation.

## Architecture

GameStat uses a modular architecture consisting of a Python backend and a C++ frontend.

### Backend — Python

The backend is responsible for collecting hardware telemetry and providing structured monitoring data.

**Technologies:**

- Python 3.13+
- psutil
- NVIDIA Management Library (NVML)
- nvidia-ml-py

### Frontend — C++

The frontend is responsible for rendering the monitoring interface and displaying live telemetry.

**Technologies:**

- C++17
- Vulkan
- GLFW
- Dear ImGui
- CMake

### Communication

The C++ frontend launches the Python backend and receives telemetry through the existing process communication mechanism.

Incoming JSON data is parsed and processed asynchronously, allowing the interface to update without blocking the rendering loop.

## Requirements

### Operating System

- Linux
- X11 is the primary target for the current implementation.
- Desktop compositor and window manager behavior may affect overlay transparency and click-through functionality.

### Backend

- Python 3.13+
- uv
- NVIDIA GPU and compatible NVIDIA driver for NVIDIA telemetry.

### Frontend

- C++17 compatible compiler
- CMake
- pkg-config
- Vulkan development libraries
- GLFW 3.4+
- Dear ImGui source code with GLFW and Vulkan backends.

The existing `imgui/` directory must contain the Dear ImGui sources and matching Vulkan backend implementation.

## Installation

### 1. Clone the repository

```bash
git clone https://github.com/aamiroo/gamestat.git
cd gamestat
```

### 2. Install system dependencies

On Debian-based distributions:

```bash
sudo apt update

sudo apt install \
    build-essential \
    cmake \
    pkg-config \
    libvulkan-dev \
    vulkan-tools \
    libglfw3-dev
```

Verify Vulkan availability:

```bash
vulkaninfo --summary
```

### 3. Install backend dependencies

Install [uv](https://docs.astral.sh/uv/) if it is not already installed.

Then install the Python dependencies:

```bash
cd back
uv sync
cd ..
```

The Python environment and dependencies are managed through uv.

### 4. Build the frontend

From the project root:

```bash
cmake -S front -B front/build -DCMAKE_BUILD_TYPE=Release

cmake --build front/build -j"$(nproc)"
```

The frontend executable is generated at:

```text
front/build/game_stat_overlay
```

## Running GameStat

Launch GameStat from the project root:

```bash
./front/build/game_stat_overlay
```

The frontend automatically starts the Python backend and receives hardware telemetry.

> Run the application from the project root so the backend can resolve its project-relative paths correctly.

## NVIDIA GPU Support

NVIDIA GPU metrics are collected through NVML.

Verify that the NVIDIA driver is working:

```bash
nvidia-smi
```

The backend uses NVIDIA Management Library to retrieve:

- GPU temperature
- GPU utilization
- VRAM statistics
- Memory controller utilization
- Graphics clock
- Power consumption

To inspect available Vulkan devices:

```bash
vulkaninfo --summary
```

On hybrid graphics systems, Vulkan device selection and NVIDIA PRIME offloading depend on the installed driver and application configuration.

If NVIDIA telemetry is unavailable, check the driver installation and NVML availability.

## Overlay

GameStat provides a compact, MangoHud-inspired monitoring interface.

The overlay displays CPU, RAM, GPU, and VRAM statistics in a minimal layout designed for gameplay.

Current interface characteristics:

- Compact, automatically sized window.
- Color-coded hardware labels.
- Minimal spacing and typography.
- Dark background.
- Live metric updates.
- Stale-data indication when telemetry stops updating.

### Compatibility

Overlay behavior depends on the desktop environment, window manager, compositor, graphics configuration, and game display mode.

X11 is the primary target for the current implementation.

**Transparency, click-through behavior, always-on-top behavior, and exclusive fullscreen compatibility depend on the environment and require individual testing.** A genuinely transparent Vulkan window is not guaranteed on X11.

## Development Status

| Component | Status |
|---|---|
| CPU monitoring | Implemented |
| RAM monitoring | Implemented |
| NVIDIA GPU monitoring | Implemented |
| VRAM monitoring | Implemented |
| Backend integration | Implemented |
| C++ frontend | Implemented |
| Vulkan rendering | Implemented |
| Compact monitoring overlay | Implemented |
| Live metric display | Implemented |
| FPS monitoring | Planned |
| Overlay transparency and game compatibility | Requires further testing |

## Roadmap

- [x] CPU monitoring
- [x] RAM monitoring
- [x] NVIDIA GPU monitoring
- [x] VRAM monitoring
- [x] Python-to-C++ communication
- [x] Vulkan frontend
- [x] Compact overlay interface
- [x] Live metric rendering
- [ ] FPS monitoring
- [ ] Extended game compatibility testing
- [ ] Improved transparency and window behavior
- [ ] Additional hardware support
- [ ] Configuration and customization

## Contributing

Contributions, bug reports, and suggestions are welcome.

If you find an issue or have an idea for improvement, feel free to open an issue or submit a pull request.

## License

GameStat is licensed under the **MIT License**.

See the [LICENSE](LICENSE) file for details.

---

<p align="center">
  Made for Linux gaming.
</p>