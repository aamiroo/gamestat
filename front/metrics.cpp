#include "metrics.h"
#include <imgui.h>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <optional>
#include <string>

namespace {

// MangoHud default palette

// Color constants for metric categories
const ImVec4 kGpuColor (0x2E / 255.0f, 0x97 / 255.0f, 0x62 / 255.0f, 1.0f);
const ImVec4 kVramColor(0xAD / 255.0f, 0x64 / 255.0f, 0xC1 / 255.0f, 1.0f);
const ImVec4 kCpuColor (0x2E / 255.0f, 0x97 / 255.0f, 0xCB / 255.0f, 1.0f);
const ImVec4 kRamColor (0xC2 / 255.0f, 0x66 / 255.0f, 0x93 / 255.0f, 1.0f);
const ImVec4 kFpsColor (0xE5 / 255.0f, 0xB4 / 255.0f, 0x3C / 255.0f, 1.0f);
const ImVec4 kValueColor(1.00f, 1.00f, 1.00f, 1.0f);
const ImVec4 kUnitColor (0.78f, 0.78f, 0.78f, 1.0f);   // units are slightly dimmer than values
const ImVec4 kStaleColor(0.50f, 0.50f, 0.50f, 1.0f);   // backend stopped sending data

// Telemetry older than this is shown dimmed

// Threshold after which metrics are considered stale
constexpr double kStaleAfterSeconds = 3.0;

// Space between a value and its unit
constexpr float kUnitGap = 3.0f;

float text_w(const char* s) { return ImGui::CalcTextSize(s).x; }

// Draws "value unit" so the whole thing ends at x_right (window-content coordinates).

// Value is bright, unit dim, like MangoHud.
// The value is bright, the unit dim, like MangoHud. Always stays on the current line.
void cell_right(float x_right, const char* value, const char* unit, const ImVec4& value_color,
                const ImVec4& unit_color) {
    const float gap = (unit && *unit) ? kUnitGap : 0.0f;
    const float w = text_w(value) + gap + (unit ? text_w(unit) : 0.0f);
    ImGui::SameLine(x_right - w, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, value_color);
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();
    if (unit && *unit) {
        ImGui::SameLine(0.0f, gap);
        ImGui::PushStyleColor(ImGuiCol_Text, unit_color);
        ImGui::TextUnformatted(unit);
        ImGui::PopStyleColor();
    }
}

void label(const char* text, const ImVec4& color) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

// "37" for a valid number, "--" when the metric is unavailable
void fmt_int(char* out, size_t n, bool ok, float v) {
    if (ok) std::snprintf(out, n, "%.0f", v);
    else    std::snprintf(out, n, "--");
}

// One decimal for frame times, "--" when the metric is unavailable
void fmt_ms(char* out, size_t n, bool ok, float v) {
    if (ok) std::snprintf(out, n, "%.1f", v);
    else    std::snprintf(out, n, "--");
}

// "used/total" in GB, e.g. "2.0/8.0"
void fmt_mem(char* out, size_t n, bool ok, float used, float total) {
    if (!ok) { std::snprintf(out, n, "--"); return; }
    if (total >= 100.0f) std::snprintf(out, n, "%.0f/%.0f", used, total);
    else                 std::snprintf(out, n, "%.1f/%.1f", used, total);
}

}  // namespace

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

void MetricsRenderer::update(const TelemetryData& telemetry, double age_seconds) {
    data_ = telemetry;
    display_ = telemetry.config;
    age_seconds_ = age_seconds;
    has_data_ = telemetry.last_update > 0.0;
}

void MetricsRenderer::render() {
    const bool stale = !has_data_ || age_seconds_ > kStaleAfterSeconds;
    const ImVec4 value_color = stale ? kStaleColor : kValueColor;
    const ImVec4 unit_color  = stale ? kStaleColor : kUnitColor;

    const CpuTelemetry& cpu = data_.cpu;
    const RamTelemetry& ram = data_.ram;
    const GpuTelemetry& gpu = data_.gpu;
    const FpsTelemetry& fps = data_.fps;

    // The backend parser overwrites RamTelemetry::available with a flag, so a populated
    // 'total' is the reliable sign that RAM data arrived.
    const bool cpu_ok = cpu.available;
    const bool ram_ok = ram.total > 0.0f;
    const bool gpu_ok = gpu.available;

    // --- Column layout, derived from the widest expected text so it never jiggles ---
    const float em  = ImGui::GetFontSize();
    const float gap = std::floor(em * 0.6f);
    const float label_w = std::fmax(text_w("VRAM"), text_w("0.1%L"));
    // Columns also have to fit the FPS rows ("0000 fps", "000.0 ms")
    const float col_a_w = std::fmax(std::fmax(text_w("100") + kUnitGap + text_w("%"),       // usage
                                              text_w("0000") + kUnitGap + text_w("fps")),   // fps
                                    text_w("0000") + kUnitGap + text_w("MHz"));             // gpu clock
    const float col_b_w = std::fmax(text_w("100") + kUnitGap + text_w("°C"),      // temperature
                                    text_w("000.0") + kUnitGap + text_w("ms"));   // frame time
    const float col_c_w = text_w("000") + kUnitGap + text_w("W");     // power
    const float mem_w   = text_w("00.0/00.0") + kUnitGap + text_w("GB");

    const float x_a = label_w + gap + col_a_w;      // right edge of column A
    const float x_b = x_a + gap + col_b_w;          // right edge of column B
    float x_c = x_b + gap + col_c_w;                // right edge of column C
    // memory text spans B and C: widen C if needed
    if (x_c - (x_a + gap) < mem_w) x_c = x_a + gap + mem_w;
    // Which rows/cells the user wants (config). A row is drawn only if at least one of its
    // cells is enabled. Cells keep their fixed column positions, so hiding one never shifts
    // the others.
    const DisplayConfig& cfg = display_;
    const bool show_gpu  = cfg.gpu_use || cfg.gpu_temp || cfg.gpu_power;
    const bool show_vram = cfg.vram;
    const bool show_cpu  = cfg.cpu_use || cfg.cpu_temp;
    const bool show_ram  = cfg.ram_use;
    const bool show_fps  = cfg.fps;

    float max_x = 0.0f;       // right edge of the widest cell actually drawn
    bool any_drawn = false;
    auto cell = [&](float x_right, const char* value, const char* unit) {
        cell_right(x_right, value, unit, value_color, unit_color);
        if (x_right > max_x) max_x = x_right;
    };
    // Separator line between visible groups only
    auto begin_group = [&](bool visible) {
        if (!visible) return;
        if (any_drawn) ImGui::Separator();
        any_drawn = true;
    };

    char a[32], b[32], c[32];

    // ----- GPU + VRAM -----
    begin_group(show_gpu || show_vram || cfg.gpu_clock || cfg.gpu_mem_ctrl);
    if (show_gpu) {
        label("GPU", kGpuColor);
        if (cfg.gpu_use) {
            fmt_int(a, sizeof a, gpu_ok, gpu.usage);
            cell(x_a, a, gpu_ok ? "%" : "");
        }
        if (cfg.gpu_temp) {
            fmt_int(b, sizeof b, gpu_ok && gpu.temperature > 0.0f, gpu.temperature);
            cell(x_b, b, "°C");
        }
        if (cfg.gpu_power) {
            fmt_int(c, sizeof c, gpu_ok && gpu.power > 0.0f, gpu.power);
            cell(x_c, c, "W");
        }
    }
    if (show_vram) {
        const bool vram_ok = gpu_ok && gpu.vram_total > 0.0f;
        label("VRAM", kVramColor);
        fmt_int(a, sizeof a, vram_ok, gpu.vram_usage);
        cell(x_a, a, vram_ok ? "%" : "");
        fmt_mem(b, sizeof b, vram_ok, gpu.vram_used, gpu.vram_total);
        cell(x_c, b, "GB");
    }
    if (cfg.gpu_clock) {
        const bool clock_ok = gpu_ok && gpu.graphics_clock > 0.0f;
        label("CLK", kGpuColor);
        fmt_int(a, sizeof a, clock_ok, gpu.graphics_clock);
        cell(x_a, a, clock_ok ? "MHz" : "");
    }
    if (cfg.gpu_mem_ctrl) {
        label("MCU", kVramColor);   // GPU memory controller utilization
        fmt_int(a, sizeof a, gpu_ok, gpu.memory_controller);
        cell(x_a, a, gpu_ok ? "%" : "");
    }

    // ----- CPU + RAM -----
    begin_group(show_cpu || show_ram);
    if (show_cpu) {
        label("CPU", kCpuColor);
        if (cfg.cpu_use) {
            fmt_int(a, sizeof a, cpu_ok, cpu.usage);
            cell(x_a, a, cpu_ok ? "%" : "");
        }
        if (cfg.cpu_temp) {
            fmt_int(b, sizeof b, cpu_ok && cpu.temperature > 0.0f, cpu.temperature);
            cell(x_b, b, "°C");
        }
    }
    if (show_ram) {
        label("RAM", kRamColor);
        fmt_int(a, sizeof a, ram_ok, ram.usage);
        cell(x_a, a, ram_ok ? "%" : "");
        fmt_mem(b, sizeof b, ram_ok, ram.used, ram.total);
        cell(x_c, b, "GB");
    }

    // ----- FPS group -----
    begin_group(show_fps);
    if (show_fps) {
        // A metric is shown only when the backend marked FPS available and sent a number
        const bool fps_ok = fps.available;
        auto ok = [fps_ok](const std::optional<float>& v) { return fps_ok && v.has_value(); };

        // FPS: current fps + frame time
        label("FPS", kFpsColor);
        fmt_int(a, sizeof a, ok(fps.fps), fps.fps.value_or(0.0f));
        cell(x_a, a, ok(fps.fps) ? "fps" : "");
        fmt_ms(b, sizeof b, ok(fps.frame_time_ms), fps.frame_time_ms.value_or(0.0f));
        cell(x_b, b, ok(fps.frame_time_ms) ? "ms" : "");

        // AVG: average fps
        label("AVG", kFpsColor);
        fmt_int(a, sizeof a, ok(fps.average_fps), fps.average_fps.value_or(0.0f));
        cell(x_a, a, ok(fps.average_fps) ? "fps" : "");

        // 1% low: fps + p99 frame time
        label("1%L", kFpsColor);
        fmt_int(a, sizeof a, ok(fps.fps_1_low), fps.fps_1_low.value_or(0.0f));
        cell(x_a, a, ok(fps.fps_1_low) ? "fps" : "");
        fmt_ms(b, sizeof b, ok(fps.p99_frame_time_ms), fps.p99_frame_time_ms.value_or(0.0f));
        cell(x_b, b, ok(fps.p99_frame_time_ms) ? "ms" : "");

        // 0.1% low: fps + p99.9 frame time
        label("0.1%L", kFpsColor);
        fmt_int(a, sizeof a, ok(fps.fps_01_low), fps.fps_01_low.value_or(0.0f));
        cell(x_a, a, ok(fps.fps_01_low) ? "fps" : "");
        fmt_ms(b, sizeof b, ok(fps.p999_frame_time_ms), fps.p999_frame_time_ms.value_or(0.0f));
        cell(x_b, b, ok(fps.p999_frame_time_ms) ? "ms" : "");
    }

    // Everything switched off in the config: draw nothing
    if (!any_drawn) {
        content_size_ = ImVec2(0.0f, 0.0f);
        return;
    }

    // Bottom edge of the last row, relative to the window's content origin
    const float bottom = ImGui::GetItemRectMax().y - ImGui::GetWindowPos().y - ImGui::GetStyle().WindowPadding.y;
    content_size_ = ImVec2(max_x, bottom);
}
