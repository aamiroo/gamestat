# GameStat

<p align="center">
  <strong>Linux Gaming Performance Monitor</strong>
  <br />
  Monitor your hardware. Understand your performance.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Linux-blue?style=flat-square" alt="Linux" />
  <img src="https://img.shields.io/badge/Backend-Python-yellow?style=flat-square" alt="Python" />
  <img src="https://img.shields.io/badge/Frontend-C%2B%2B-blue?style=flat-square" alt="C++" />
  <img src="https://img.shields.io/badge/Rendering-Vulkan-red?style=flat-square" alt="Vulkan" />
  <img src="https://img.shields.io/badge/License-MIT-green?style=flat-square" alt="GPL-3.0 License" />
</p>

---

## Overview

**GameStat** is an open-source performance monitoring overlay designed specifically for Linux gaming.

It provides real-time hardware telemetry through a compact, gaming-oriented interface, allowing players to monitor system performance without leaving their games.

Built with a modular architecture, GameStat combines a Python-based monitoring backend with a native C++ rendering frontend and a dedicated frame capture module.

The project focuses on lightweight operation, responsive updates, and extensibility across Linux gaming environments.

## Features

### Hardware Monitoring

- Real-time CPU temperature and utilization.
- RAM usage, available memory, and total memory.
- NVIDIA GPU temperature and utilization.
- VRAM usage, available memory, and total memory.
- GPU memory controller utilization.
- GPU graphics clock monitoring.
- GPU power consumption monitoring.

### Overlay

- Compact, gaming-oriented monitoring interface.
- Vulkan-based rendering.
- Automatic overlay sizing based on displayed content.
- Color-coded hardware metrics.
- Dark interface designed to minimize screen obstruction.
- Asynchronous telemetry updates.
- Stale-data detection when backend updates stop.

### System Architecture

- Modular Python and C++ architecture.
- Automatic backend startup.
- Asynchronous frontend/backend communication.
- Dedicated frame capture and analysis components.
- Designed for low resource consumption.

## Hardware Metrics

| Component  | Monitored Metrics                                                |
| ---------- | ---------------------------------------------------------------- |
| CPU        | Temperature, utilization                                         |
| RAM        | Total, used, available, usage percentage                         |
| NVIDIA GPU | Temperature, utilization                                         |
| VRAM       | Total, used, free, usage percentage                              |
| GPU        | Memory controller utilization, graphics clock, power consumption |

> FPS capture and monitoring are under development. Full in-game FPS display is not yet considered a stable feature.

## Architecture

GameStat consists of three main components.

### 1. Python Backend

Responsible for collecting hardware telemetry and providing structured monitoring data.

**Technologies:**

- Python 3.13+
- psutil
- NVIDIA Management Library (NVML)
- nvidia-ml-py

### 2. C++ Frontend

Responsible for rendering the overlay and displaying real-time system statistics.

**Technologies:**

- C++17
- Vulkan
- GLFW
- Dear ImGui
- CMake

### 3. Frame Capture Module

A dedicated component for collecting and analyzing frame-related data.

It includes native capture components and a Python interface for frame collection and analysis.

FPS monitoring is currently under development.

## Requirements

### Operating System

- Linux
- X11 is the primary target for the current implementation.
- Window manager and compositor behavior may affect overlay compatibility.

### Backend

- Python 3.13+
- uv

NVIDIA GPU telemetry requires a compatible NVIDIA GPU, driver, and NVML support.

### Frontend

- C++17 compatible compiler
- CMake
- pkg-config
- Vulkan development libraries
- GLFW 3.4+
- Dear ImGui source code with GLFW and Vulkan backends

The existing `imgui/` directory must contain the Dear ImGui sources and matching Vulkan backend implementation.

### Graphics

- Vulkan-compatible graphics driver
- Vulkan loader and development libraries

## Installation

### Build from Source

The following instructions describe the current development installation workflow.

#### 1. Clone the repository

```bash
git clone https://github.com/aamiroo/gamestat.git
cd gamestat
```

#### 2. Install system dependencies

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

#### 3. Install uv

Install [uv](https://docs.astral.sh/uv/) if it is not already available.

#### 4. Install backend dependencies

```bash
cd back
uv sync
cd ..
```

The Python backend and its dependencies are managed through uv.

#### 5. Build the frontend

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

Launch the application from the project root:

```bash
./front/build/game_stat_overlay
```

The frontend automatically starts the Python backend and receives hardware telemetry.

> The current development version relies on project-relative resources and its configured Python environment. Run it from the project root.

## NVIDIA GPU Support

GameStat uses NVIDIA Management Library (NVML) to retrieve GPU telemetry.

Verify that the NVIDIA driver is working:

```bash
nvidia-smi
```

The backend can retrieve:

- GPU temperature
- GPU utilization
- VRAM statistics
- Memory controller utilization
- Graphics clock
- Power consumption

On hybrid graphics systems, Vulkan device selection and NVIDIA PRIME offloading depend on the installed driver and application configuration.

If NVIDIA telemetry is unavailable, check the driver installation and NVML availability.

## Overlay

GameStat provides a compact monitoring interface designed for gaming environments.

Current interface characteristics:

- Automatically sized window.
- Color-coded hardware labels.
- Compact spacing and typography.
- Dark background.
- Live metric updates.
- Stale-data indication when telemetry stops updating.

### Compatibility

Overlay behavior depends on the desktop environment, window manager, compositor, graphics configuration, and game display mode.

X11 is the primary target for the current implementation.

Transparency, click-through behavior, always-on-top behavior, and exclusive fullscreen compatibility depend on the environment and require individual testing.

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
| Frame capture module | Implemented |
| FPS monitoring | Implemented |
| FPS display in overlay | Implemented |
| In-game overlay display | Tested and working |
| Extended game compatibility | Requires further testing |
| Additional hardware support | Planned |

## Roadmap

- [x] CPU monitoring
- [x] RAM monitoring
- [x] NVIDIA GPU monitoring
- [x] VRAM monitoring
- [x] Python-to-C++ communication
- [x] Vulkan frontend
- [x] Compact overlay interface
- [x] Live metric rendering
- [ ] Complete FPS monitoring
- [ ] Extended game compatibility testing
- [ ] Improved transparency and window behavior
- [ ] Additional GPU and hardware support
- [ ] Configuration and customization
- [ ] Installation packages for Linux distributions
- [ ] Expanded documentation

## Contributing

Contributions, bug reports, and suggestions are welcome.

Whether you want to report a bug, improve compatibility, contribute code, or suggest a feature, your participation is appreciated.

Feel free to open an issue or submit a pull request.

## License

GameStat is licensed under the **MIT License**.

See the [LICENSE](LICENSE) file for details.

---

\<p align="center">   Made for Linux gaming. \</p>
