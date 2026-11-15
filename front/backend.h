#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <variant>
#include <csignal>
#include "nlohmann/json.hpp"

// JSON telemetry data structures and backend communicator

// Forward declaration for telemetry data structures
struct CpuTelemetry;
struct RamTelemetry;
struct GpuTelemetry;

// ----- Telemetry Data Structures -----

// CPU metrics
struct CpuTelemetry {
    float temperature = 0.0f;       // in Celsius, 0 = unavailable
    float usage = 0.0f;             // in percent, 0-100
    bool available = false;
};

// RAM metrics
struct RamTelemetry {
    float total = 0.0f;             // in GB
    float used = 0.0f;              // in GB
    float available = 0.0f;         // in GB
    float usage = 0.0f;             // in percent, 0-100
    bool is_available = false;
};

// GPU metrics
struct GpuTelemetry {
    std::string name;
    float temperature = 0.0f;       // in Celsius, 0 = unavailable
    float usage = 0.0f;             // in percent, 0-100
    float vram_total = 0.0f;        // in GB
    float vram_used = 0.0f;         // in GB
    float vram_free = 0.0f;         // in GB
    float vram_usage = 0.0f;        // in percent, 0-100
    float memory_controller = 0.0f; // in percent, 0-100
    float graphics_clock = 0.0f;    // in MHz
    float power = 0.0f;             // in Watts, 0 = unavailable
    bool available = false;
};

// ----- Overall Telemetry Container -----

// Container holding the latest CPU, RAM, and GPU telemetry data

// Container for CPU, RAM, and GPU telemetry data received from the backend
struct TelemetryData {
    CpuTelemetry cpu;
    RamTelemetry ram;
    GpuTelemetry gpu;
    double last_update{0.0};  // timestamp of last successful update
};

// ----- Backend Communication Class -----

// Handles communication with the Python backend process via pipe,

public:
    BackendCommunicator();
    ~BackendCommunicator();

    // Start the backend process and communication thread
    bool start();

    // Stop the backend process and communication thread
    void stop();

    // Check if new telemetry is available
    bool has_new_data() const;

    // Get the latest telemetry data (thread-safe, copies data)
    TelemetryData get_telemetry() const;

    // Get time since last update in seconds
    double get_age() const;

    // Check if any data is available
    bool is_available() const;

private:
    // Internal data structures
    mutable std::mutex mutex_;
    TelemetryData data_;
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    int pipe_fds_[2];  // [0] read, [1] write - used for child process output
    pid_t backend_process_{0};  // PID of the backend process

    // Start the Python backend process
    bool launch_backend();

    // Read and parse telemetry from the process output
    void read_loop();

    // Parse a single JSON telemetry block from output buffer
    bool parse_telemetry(const std::string& json_str);

    // Helper to safely update data from the read thread
    void update_data(const TelemetryData& new_data);
};