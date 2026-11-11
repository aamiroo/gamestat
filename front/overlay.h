#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "backend.h"

// Forward declarations
#include "metrics.h"
struct FrameData;

class Overlay {
public:
    Overlay();
    ~Overlay();

    // Initialize the overlay (window, Vulkan, etc.)
    bool init(int width, int height, const char* title);

    // Clean up resources
    void cleanup();

    // Main loop - returns false to quit
    bool run(BackendCommunicator& communicator);

    // Get whether the overlay should stay open
    bool should_close() const { return glfwWindowShouldClose(window_); }

    // Get the ImGui context
    ImGuiContext* io_ctx() const { return io_ctx_; }

    // Get framebuffer dimensions
    ImVec2 framebuffer_size() const;

private:
    // Window and Vulkan setup
    GLFWwindow* window_ = nullptr;
    VkInstance vk_instance_ = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT debug_callback_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    VkPipelineCache pipeline_cache_ = VK_NULL_HANDLE;

    // ImGui context (pointer - created by CreateContext)
    ImGuiContext* io_ctx_;
    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkFramebuffer* framebuffers_ = nullptr;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;

    // Window properties
    int width_;
    int height_;
    bool fullscreen_ = false;

    // Frame data for Vulkan rendering
    struct FrameData {
        VkCommandBuffer command_buffer;
        VkFramebuffer framebuffer;
        VkImageView backbuffer_view;
    };

    // Metrics renderer
    MetricsRenderer metrics_;

    // Setup ImGui style and bindings
    void setup_imgui_style();

    // Setup Vulkan rendering objects
    void setup_vulkan();

    // Handle window resize
    void on_resize(int width, int height);
};