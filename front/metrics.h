#pragma once

#include "backend.h"
#include <imgui.h>
#include <string>

struct MetricCard {
    std::string label;
    std::string value;
    std::string unit;
    ImVec2 size;
    ImVec4 accent_color;
    bool show_unit;
};

class MetricsRenderer {
public:
    MetricsRenderer();
    ~MetricsRenderer();

    void init();
    void set_window_size(int width, int height);

    void update(const TelemetryData& telemetry, double age_seconds);
    void render();

private:
    MetricCard cpu_card_;
    MetricCard ram_card_;
    MetricCard gpu_card_;

    int window_width_;
    int window_height_;

    std::string format_value(float value, const char* unit, bool available) const;
    void update_cpu_card(const CpuTelemetry& cpu);
    void update_ram_card(const RamTelemetry& ram);
    void update_gpu_card(const GpuTelemetry& gpu);
};