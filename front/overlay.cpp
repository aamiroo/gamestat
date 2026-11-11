#include "overlay.h"

#include <backend.h>
#include <metrics.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <thread>

Overlay::Overlay() : window_(nullptr), width_(800), height_(600) {}

Overlay::~Overlay() {
    cleanup();
}

bool Overlay::init(int width, int height, const char* title) {
    width_ = width;
    height_ = height;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);

    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    IMGUI_CHECKVERSION();
    io_ctx_ = ImGui::CreateContext();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    
    // Initialize ImGui OpenGL3 backend
    const char* glsl_version = "#version 130";
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    setup_imgui_style();
    metrics_.init();

    return true;
}

void Overlay::cleanup() {
    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(io_ctx_);
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}

void Overlay::setup_imgui_style() {
    ImGuiIO& io = ImGui::GetIO();

    ImVec4* colors = ImGui::GetStyle().Colors;

    // Dark theme with gaming accent colors
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.15f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.17f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.20f, 0.98f);

    // Accent colors
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.45f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.40f, 0.60f, 1.00f);

    // CPU accent (reddish)
    colors[ImGuiCol_Header] = ImVec4(0.40f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.50f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.60f, 0.40f, 0.40f, 1.00f);

    // RAM accent (bluish)
    colors[ImGuiCol_SliderGrab] = ImVec4(0.20f, 0.50f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.30f, 0.60f, 0.90f, 1.00f);

    // GPU accent (greenish)
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.95f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.40f, 0.40f, 0.50f, 0.75f);
    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.40f, 0.50f);

    // Rounding and spacing
    ImGui::GetStyle().WindowRounding = 8.0f;
    ImGui::GetStyle().ChildRounding = 6.0f;
    ImGui::GetStyle().FrameRounding = 4.0f;
    ImGui::GetStyle().GrabRounding = 4.0f;
    ImGui::GetStyle().FramePadding = ImVec2(8, 6);
    ImGui::GetStyle().WindowPadding = ImVec2(12, 8);
    ImGui::GetStyle().ItemSpacing = ImVec2(8, 4);
}

ImVec2 Overlay::framebuffer_size() const {
    int w, h;
    if (window_) {
        glfwGetFramebufferSize(window_, &w, &h);
        return ImVec2(static_cast<float>(w), static_cast<float>(h));
    }
    return ImVec2(static_cast<float>(width_), static_cast<float>(height_));
}

bool Overlay::run(BackendCommunicator& communicator) {
    if (!window_) {
        std::cerr << "Overlay not initialized" << std::endl;
        return true;
    }

    bool show_demo_window = false;
    bool show_another_window = false;

    double last_time = glfwGetTime();
    int frame_count = 0;
    double overlay_fps = 0.0;

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        auto telemetry = communicator.get_telemetry();
        double age = communicator.get_age();
        metrics_.update(telemetry, age);

        ImVec2 fb_size = framebuffer_size();

        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::SetNextWindowSize(ImVec2(fb_size.x - 20, fb_size.y - 20));

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoBringToFrontOnFocus |
                                        ImGuiWindowFlags_NoNavFocus |
                                        ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        // Use ImGui's internal style color for window background
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.3f));

        ImGui::Begin("GameStat Overlay", nullptr, window_flags);
        metrics_.render();
        ImGui::End();

        // Pop in reverse order
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        // Render ImGui drawing commands
        ImGui::Render();

        // Swap buffers
        glfwSwapBuffers(window_);
    }

    return true;
}