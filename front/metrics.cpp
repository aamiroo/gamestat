#include "metrics.h"
#include <imgui.h>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
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

// ----- Display config -----
// Reads the user's config file (INI, [display] section, same file the Python config
// loader reads). Defaults are "show everything"; a key only changes when the file sets
// it to a valid boolean, exactly like Python's configparser.getboolean fallback.

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

std::string lower(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

// Accepts the same words as configparser: 1/yes/true/on and 0/no/false/off
bool parse_bool(const std::string& value, bool& out) {
    const std::string v = lower(value);
    if (v == "1" || v == "yes" || v == "true"  || v == "on")  { out = true;  return true; }
    if (v == "0" || v == "no"  || v == "false" || v == "off") { out = false; return true; }
    return false;
}

// GAMESTAT_CONFIG overrides the location; default is <project>/config/config.
std::string config_path() {
    const char* env = std::getenv("GAMESTAT_CONFIG");
    if (env && *env) return env;
    const char* home = std::getenv("HOME");
    return std::string(home ? home : "") + "/Project/gamestat/config/config";
}

DisplayConfig load_display_config() {
    DisplayConfig cfg;
    const std::string path = config_path();
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Config: " << path << " not found, showing all metrics" << std::endl;
        return cfg;
    }

    struct Entry { const char* key; bool* field; };
    const Entry entries[] = {
        {"cpu-temp", &cfg.cpu_temp}, {"cpu-use", &cfg.cpu_use}, {"ram-use", &cfg.ram_use},
        {"gpu-temp", &cfg.gpu_temp}, {"gpu-use", &cfg.gpu_use}, {"vram", &cfg.vram},
        {"fps", &cfg.fps},
    };

    std::string section, line;
    while (std::getline(in, line)) {
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#' || t[0] == ';') continue;
        if (t.front() == '[' && t.back() == ']') { section = trim(t.substr(1, t.size() - 2)); continue; }
        if (section != "display") continue;

        const size_t sep = t.find_first_of("=:");
        if (sep == std::string::npos) continue;
        const std::string key = lower(trim(t.substr(0, sep)));
        const std::string val = trim(t.substr(sep + 1));
        for (const Entry& e : entries) {
            bool b = false;
            if (key == e.key && parse_bool(val, b)) *e.field = b;
        }
    }
    std::cerr << "Config: loaded " << path << std::endl;
    return cfg;
}

}  // namespace

MetricsRenderer::MetricsRenderer()
    : window_width_(800), window_height_(600) {
}

MetricsRenderer::~MetricsRenderer() {
}

void MetricsRenderer::init() {
    display_ = load_display_config();
}

void MetricsRenderer::set_window_size(int width, int height) {
    window_width_ = width;
    window_height_ = height;
}

void MetricsRenderer::update(const TelemetryData& telemetry, double age_seconds) {
    data_ = telemetry;
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
    const float col_a_w = std::fmax(text_w("100") + kUnitGap + text_w("%"),       // usage
                                    text_w("0000") + kUnitGap + text_w("fps"));   // fps
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
    const bool show_gpu  = cfg.gpu_use || cfg.gpu_temp;   // GPU power follows the GPU row
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
    begin_group(show_gpu || show_vram);
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
        fmt_int(c, sizeof c, gpu_ok && gpu.power > 0.0f, gpu.power);
        cell(x_c, c, "W");
    }
    if (show_vram) {
        const bool vram_ok = gpu_ok && gpu.vram_total > 0.0f;
        label("VRAM", kVramColor);
        fmt_int(a, sizeof a, vram_ok, gpu.vram_usage);
        cell(x_a, a, vram_ok ? "%" : "");
        fmt_mem(b, sizeof b, vram_ok, gpu.vram_used, gpu.vram_total);
        cell(x_c, b, "GB");
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
