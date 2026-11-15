# Backend communicator - manages pipe-based communication with Python backend
#include "backend.h"

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <chrono>
#include <csignal>

// Fix: added #include <csignal> above

// Constructor - initializes pipe FDs and sets non-blocking mode
    if (pipe(pipe_fds_) != 0) {
        perror("Failed to create pipe");
    }
    int flags = fcntl(pipe_fds_[0], F_GETFL, 0);
    if (flags >= 0) {
        fcntl(pipe_fds_[0], F_SETFL, flags | O_NONBLOCK);
    }
    if (flags >= 0) {
        fcntl(pipe_fds_[1], F_SETFL, flags | O_NONBLOCK);
    }
    backend_process_ = 0;
}

BackendCommunicator::~BackendCommunicator() {
    stop();
}

// Start - launches the backend process and starts the read thread
    running_ = true;
    if (!launch_backend()) {
        std::cerr << "Failed to launch backend" << std::endl;
        running_ = false;
        return false;
    }
    std::cerr << "Backend launched, process ID: " << backend_process_ << std::endl;
    worker_thread_ = std::thread(&BackendCommunicator::read_loop, this);
    worker_thread_.detach();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cerr << "Checking for new data after 2 seconds..." << std::endl;
    bool data = has_new_data();
    std::cerr << "Has new data: " << data << std::endl;
    if (!data) {
        // Dump the raw pipe data for debugging
        char buffer[4096];
        lseek(pipe_fds_[0], 0, SEEK_SET);
        read(pipe_fds_[0], buffer, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        std::cerr << "Pipe data: " << buffer << std::endl;
    }
    return data;
}

void BackendCommunicator::stop() {
    if (running_) {
        running_ = false;
    }
    if (pipe_fds_[1] >= 0) {
        close(pipe_fds_[1]);
        pipe_fds_[1] = -1;
    }
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    if (backend_process_ > 0) {
        ::kill(backend_process_, SIGTERM);
        backend_process_ = 0;
    }
    if (pipe_fds_[0] >= 0) {
        close(pipe_fds_[0]);
        pipe_fds_[0] = -1;
    }
}

bool BackendCommunicator::has_new_data() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.last_update > 0.0;
}

TelemetryData BackendCommunicator::get_telemetry() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
}

double BackendCommunicator::get_age() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.last_update > 0.0
        ? std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count() - data_.last_update
        : 0.0;
}

bool BackendCommunicator::is_available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    // Check if any data source has available data
    return data_.cpu.available || data_.ram.is_available || data_.gpu.available;
}

void BackendCommunicator::read_loop() {
    char buffer[4096];
    std::string pending;

    while (running_) {
        ssize_t bytes_read = read(pipe_fds_[0], buffer, sizeof(buffer) - 1);

        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            pending.append(buffer);

            while (true) {
                size_t start_pos = pending.find('{');
                if (start_pos == std::string::npos) {
                    pending.clear();
                    break;
                }

                int depth = 0;
                size_t end_pos = std::string::npos;
                for (size_t i = start_pos; i < pending.size(); ++i) {
                    if (pending[i] == '{') depth++;
                    else if (pending[i] == '}') {
                        depth--;
                        if (depth == 0) {
                            end_pos = i + 1;
                            break;
                        }
                    }
                }

                if (end_pos == std::string::npos) {
                    break;
                }

                std::string json_str = pending.substr(start_pos, end_pos - start_pos);
                pending = pending.substr(end_pos);

                if (parse_telemetry(json_str)) {
                    continue;
                }
            }
        } else if (bytes_read == 0) {
            running_ = false;
        } else if (errno == EAGAIN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            break;
        }
    }
}

bool BackendCommunicator::launch_backend() {
    backend_process_ = fork();
    if (backend_process_ < 0) {
        perror("Failed to fork");
        return false;
    }
    if (backend_process_ == 0) {
        dup2(pipe_fds_[1], STDOUT_FILENO);
        close(pipe_fds_[0]);
        close(pipe_fds_[1]);
        setenv("PYTHONUNBUFFERED", "1", 1);

        char* args[] = {
            (char*)"/home/user/Project/gamestat/.venv/bin/python",
            (char*)"-u",
            (char*)"/home/user/Project/gamestat/back/main.py",
            nullptr
        };

        execv(args[0], args);
        perror("execv failed");
        exit(1);
    }
    close(pipe_fds_[1]);
    pipe_fds_[1] = -1;
    return true;
}

bool BackendCommunicator::parse_telemetry(const std::string& json_str) {
    try {
        auto j = nlohmann::json::parse(json_str);

        // Parse CPU data
        if (j.contains("cpu") && j["cpu"].is_object()) {
            auto& cpu = j["cpu"];
            if (cpu.contains("temp") && cpu["temp"].is_number()) {
                data_.cpu.temperature = static_cast<float>(cpu["temp"].get<double>());
            }
            if (cpu.contains("usage") && cpu["usage"].is_number()) {
                data_.cpu.usage = static_cast<float>(cpu["usage"].get<double>());
            }
            data_.cpu.available = true;
        }

        // Parse RAM data
        if (j.contains("ram") && j["ram"].is_object()) {
            auto& ram = j["ram"];
            if (ram.contains("total") && ram["total"].is_number()) {
                data_.ram.total = static_cast<float>(ram["total"].get<double>());
            }
            if (ram.contains("used") && ram["used"].is_number()) {
                data_.ram.used = static_cast<float>(ram["used"].get<double>());
            }
            if (ram.contains("available") && ram["available"].is_number()) {
                data_.ram.available = static_cast<float>(ram["available"].get<double>());
            }
            if (ram.contains("percent") && ram["percent"].is_number()) {
                data_.ram.usage = static_cast<float>(ram["percent"].get<double>());
            }
            data_.ram.available = true;
        }

        // Parse GPU data
        if (j.contains("gpu") && j["gpu"].is_object()) {
            auto& gpu = j["gpu"];

            // GPU name
            if (gpu.contains("name") && gpu["name"].is_string()) {
                data_.gpu.name = gpu["name"].get<std::string>();
            }

            // GPU temperature
            if (gpu.contains("temp") && gpu["temp"].is_number()) {
                data_.gpu.temperature = static_cast<float>(gpu["temp"].get<double>());
            }

            // GPU usage
            if (gpu.contains("usage") && gpu["usage"].is_number()) {
                data_.gpu.usage = static_cast<float>(gpu["usage"].get<double>());
            }

            // VRAM data
            if (gpu.contains("vram") && gpu["vram"].is_object()) {
                auto& vram = gpu["vram"];
                if (vram.contains("total") && vram["total"].is_number()) {
                    data_.gpu.vram_total = static_cast<float>(vram["total"].get<double>());
                }
                if (vram.contains("used") && vram["used"].is_number()) {
                    data_.gpu.vram_used = static_cast<float>(vram["used"].get<double>());
                }
                if (vram.contains("free") && vram["free"].is_number()) {
                    data_.gpu.vram_free = static_cast<float>(vram["free"].get<double>());
                }
                if (vram.contains("percent") && vram["percent"].is_number()) {
                    data_.gpu.vram_usage = static_cast<float>(vram["percent"].get<double>());
                }
            }

            // GPU memory controller utilization
            if (gpu.contains("memory_usage") && gpu["memory_usage"].is_number()) {
                data_.gpu.memory_controller = static_cast<float>(gpu["memory_usage"].get<double>());
            }

            // GPU graphics clock
            if (gpu.contains("clock") && gpu["clock"].is_number()) {
                data_.gpu.graphics_clock = static_cast<float>(gpu["clock"].get<double>());
            }

            // GPU power consumption
            if (gpu.contains("power") && gpu["power"].is_number()) {
                data_.gpu.power = static_cast<float>(gpu["power"].get<double>());
            }

            data_.gpu.available = true;
        }

        // Update timestamp
        auto now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        data_.last_update = now;

        return true;

    } catch (const nlohmann::json::parse_error& e) {
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Telemetry parse error: " << e.what() << std::endl;
        return false;
    }
}