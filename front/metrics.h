#pragma once

#include "backend.h"
#include <imgui.h>
#include <string>

// Screen corner the overlay is anchored to.
enum class OverlayPosition { TopLeft, TopRight, BottomRight, BottomLeft };

// Which metrics are shown. Loaded from the user's config file ([display] section);
// everything is on by default.
struct DisplayConfig {
    bool cpu_temp = true;
    bool cpu_use  = true;
    bool ram_use  = true;
    bool gpu_temp = true;
    bool gpu_use  = true;
    bool vram     = true;
    bool fps      = true;
    OverlayPosition position = OverlayPosition::TopLeft;
};

// MangoHud-style stacked readout:
//   GPU   37%  61°C  95W
//   VRAM  25%  2.0/8.0 GB
//   CPU    1%   --°C
//   RAM    7%  0.6/7.8 GB
//   FPS   60 fps  16.7 ms   (+ AVG, 1%L, 0.1%L rows)
class MetricsRenderer {
public:
    MetricsRenderer();
    ~MetricsRenderer();

    void init();
    void set_window_size(int width, int height);

    void update(const TelemetryData& telemetry, double age_seconds);

    // Must be called inside an ImGui window.
    void render();

    // Corner of the screen the overlay should be placed in (from the config).
    OverlayPosition position() const { return display_.position; }

    // Size of the content drawn by the last render() call (excludes window padding).
    ImVec2 content_size() const { return content_size_; }

private:
    TelemetryData data_;
    DisplayConfig display_;
    double age_seconds_ = 0.0;
    bool has_data_ = false;

    ImVec2 content_size_ = ImVec2(0.0f, 0.0f);

    int window_width_;
    int window_height_;
};
