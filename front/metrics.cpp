#include "metrics.h"
#include <imgui.h>
#include <sstream>
#include <cmath>

MetricsRenderer::MetricsRenderer()
    : window_width_(800), window_height_(600) {
}

MetricsRenderer::~MetricsRenderer() {
}

void MetricsRenderer::init() {
}

void MetricsRenderer::set_window_size(int width, int height) {
    window_width_ = width;
    window_height_ = height;
}

std::string MetricsRenderer::format_value(float value, const char* unit, bool available) const {
    if (!available || value == 0.0f) {
        return "N/A";
    }
    std::ostringstream oss;
    oss.precision(1);
    oss << std::fixed << value;
    if (unit && *unit) {
        oss << " " << unit;
    }
    return oss.str();
}

void MetricsRenderer::update_cpu_card(const CpuTelemetry& cpu) {
    cpu_card_.label = "CPU";
    if (cpu.available) {
        cpu_card_.value = format_value(cpu.temperature, "°C", cpu.temperature > 0.0f);
        cpu_card_.accent_color = ImVec4(0.6f, 0.3f, 0.3f, 1.0f);
    } else {
        cpu_card_.value = "N/A";
        cpu_card_.accent_color = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    }
    cpu_card_.unit = "%";
    cpu_card_.show_unit = true;
}

void MetricsRenderer::update_ram_card(const RamTelemetry& ram) {
    ram_card_.label = "RAM";
    if (ram.available) {
        ram_card_.value = format_value(ram.used, "GB", true);
        ram_card_.accent_color = ImVec4(0.3f, 0.4f, 0.8f, 1.0f);
    } else {
        ram_card_.value = "N/A";
        ram_card_.accent_color = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    }
    ram_card_.unit = "%";
    ram_card_.show_unit = true;
}

void MetricsRenderer::update_gpu_card(const GpuTelemetry& gpu) {
    gpu_card_.label = "GPU";
    if (gpu.available) {
        gpu_card_.value = format_value(gpu.temperature, "°C", gpu.temperature > 0.0f);
        gpu_card_.accent_color = ImVec4(0.3f, 0.6f, 0.3f, 1.0f);
    } else {
        gpu_card_.value = "N/A";
        gpu_card_.accent_color = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    }
    gpu_card_.unit = "%";
    gpu_card_.show_unit = true;
}

void MetricsRenderer::update(const TelemetryData& telemetry, double age_seconds) {
    update_cpu_card(telemetry.cpu);
    update_ram_card(telemetry.ram);
    update_gpu_card(telemetry.gpu);
    (void)age_seconds;
}

void MetricsRenderer::render() {
    // CPU metric card
    ImGui::SetCursorPos(ImVec2(15, 15));
    ImGui::TextUnformatted(cpu_card_.label.c_str());
    ImGui::SameLine();
    if (cpu_card_.show_unit && strcmp(cpu_card_.value.c_str(), "N/A") != 0) {
        ImGui::TextUnformatted(cpu_card_.unit.c_str());
    }

    // RAM metric card
    ImGui::SameLine(180);
    ImGui::TextUnformatted(ram_card_.label.c_str());
    ImGui::SameLine();
    if (ram_card_.show_unit && strcmp(ram_card_.value.c_str(), "N/A") != 0) {
        ImGui::TextUnformatted(ram_card_.unit.c_str());
    }

    // GPU metric card
    ImGui::SameLine(360);
    ImGui::TextUnformatted(gpu_card_.label.c_str());
    ImGui::SameLine();
    if (gpu_card_.show_unit && strcmp(gpu_card_.value.c_str(), "N/A") != 0) {
        ImGui::TextUnformatted(gpu_card_.unit.c_str());
    }
}