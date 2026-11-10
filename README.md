# GameStat

\<p align="center">
&#x20; \<strong>A lightweight gaming hardware monitoring overlay for Linux.\</strong>
&#x20; \<br />
&#x20; Monitor your CPU, GPU, RAM, and system performance while playing games.
\</p>

\<p align="center">
&#x20; \<img src="[https://img.shields.io/badge/Platform-Linux-blue?style=flat-square](https://img.shields.io/badge/Platform-Linux-blue?style=flat-square)" alt="Linux" />
&#x20; \<img src="[https://img.shields.io/badge/Backend-Python-yellow?style=flat-square](https://img.shields.io/badge/Backend-Python-yellow?style=flat-square)" alt="Python" />
&#x20; \<img src="[https://img.shields.io/badge/Frontend-C%2B%2B-blue?style=flat-square](https://img.shields.io/badge/Frontend-C%2B%2B-blue?style=flat-square)" alt="C++" />
&#x20; \<img src="[https://img.shields.io/badge/Graphics-Vulkan-red?style=flat-square](https://img.shields.io/badge/Graphics-Vulkan-red?style=flat-square)" alt="Vulkan" />
&#x20; \<img src="[https://img.shields.io/badge/License-MIT-green?style=flat-square](https://img.shields.io/badge/License-MIT-green?style=flat-square)" alt="MIT License" />
\</p>

---

## Overview

**GameStat** is a hardware monitoring application designed to display real-time system statistics through a lightweight, visually polished overlay.

It allows Linux gamers to monitor important hardware metrics without constantly switching away from their games.

The project uses a Python backend to collect hardware information and a C++ frontend to render the monitoring interface.

## Features

- Real-time CPU temperature and utilization monitoring.
- RAM usage and memory availability monitoring.
- NVIDIA GPU temperature and utilization monitoring.
- VRAM usage and availability monitoring.
- GPU memory controller utilization.
- GPU graphics clock monitoring.
- GPU power consumption monitoring.
- Compact gaming-oriented overlay interface.
- Automatic backend startup and communication.
- Designed for low resource consumption.

## Hardware Metrics

| Component  | Metrics                                                          |
| ---------- | ---------------------------------------------------------------- |
| CPU        | Temperature, utilization                                         |
| RAM        | Total, used, available, usage percentage                         |
| NVIDIA GPU | Temperature, utilization                                         |
| VRAM       | Total, used, free, usage percentage                              |
| GPU        | Memory controller utilization, graphics clock, power consumption |

> FPS monitoring is not included in the current development phase.

## Architecture

GameStat consists of two main components:

### Backend — Python

The backend is responsible for collecting hardware telemetry.

Technologies:

- Python
- psutil
- NVIDIA Management Library (NVML)
- nvidia-ml-py

### Frontend — C++

The frontend is responsible for displaying telemetry through a graphical overlay.

Technologies:

- C++
- Vulkan
- GLFW
- Dear ImGui
- CMake

### Communication

The C++ application launches the existing Python backend and receives its JSON telemetry through a process communication mechanism.

The frontend parses incoming data asynchronously and updates the interface without blocking the rendering thread.

## Requirements

### Operating System

- Linux
- X11 recommended for the current overlay implementation.

### Backend

- Python 3.13+
- uv
- NVIDIA GPU with a compatible NVIDIA driver for NVIDIA telemetry.

### Frontend

- C++ compiler with C++17 or newer support.
- CMake
- Vulkan SDK or development libraries.
- GLFW development libraries.
- Existing Dear ImGui source code.

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

Make sure Vulkan is available:

```bash
vulkaninfo --summary
```

### 3. Install backend dependencies

```bash
cd back
uv sync
cd ..
```

The Python environment and dependencies are managed through uv.

### 4. Build the frontend

```bash
cmake -S front -B front/build -DCMAKE_BUILD_TYPE=Release
cmake --build front/build -j"$(nproc)"
```

## Running GameStat

The application launch instructions will be provided once the frontend implementation is complete.

> The exact executable name depends on the CMake target defined by the frontend implementation.

## NVIDIA GPU Support

NVIDIA GPU metrics are collected through NVML.

Make sure the NVIDIA driver is installed and working:

```bash
nvidia-smi
```

The backend uses the NVIDIA Management Library to retrieve:

- GPU temperature
- GPU utilization
- VRAM statistics
- Memory controller utilization
- Graphics clock
- Power consumption

If NVIDIA telemetry is unavailable, check the driver installation and NVML availability.

## Overlay Compatibility

GameStat is designed to work as an on-screen gaming overlay.

Window behavior depends on the desktop environment, window manager, graphics configuration, and game display mode.

X11 is the primary target for the current implementation.

Exclusive fullscreen compatibility may vary between games and must be tested individually.

## Development Status

GameStat is under active development.

| Component             | Status      |
| --------------------- | ----------- |
| CPU monitoring        | Implemented |
| RAM monitoring        | Implemented |
| NVIDIA GPU monitoring | Implemented |
| Backend integration   | Implemented |
| C++ frontend          | In progress |
| Gaming overlay        | In progress |
| FPS monitoring        | Planned     |

## Roadmap

- [x] CPU monitoring
- [x] RAM monitoring
- [x] NVIDIA GPU monitoring
- [x] Python-to-C++ communication
- [ ] C++ frontend implementation
- [ ] Gaming overlay integration
- [ ] FPS monitoring
- [ ] Additional hardware support
- [ ] Configuration and customization

## Contributing

Contributions, bug reports, and suggestions are welcome.

If you find an issue or have an idea for improvement, feel free to open an issue or submit a pull request.

## License

GameStat is licensed under the **MIT License**.

See the [LICENSE](LICENSE) file for details.

---

\<p align="center">
&#x20; Made for Linux gaming.
\</p>
