# Overlay window - Vulkan-based ImGui overlay with X11 stay-on-top support
#pragma once

#include <vector>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "backend.h"

// Forward declarations
#include "metrics.h"

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
    // Number of frames the CPU may record while the GPU is still working
    static constexpr uint32_t kFramesInFlight = 2;

    // Per-frame (in flight) resources
    struct FrameData {
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        VkSemaphore image_available = VK_NULL_HANDLE;  // signaled by vkAcquireNextImageKHR
        VkFence in_flight = VK_NULL_HANDLE;            // signaled when the submission finished
    };

    // Window
    GLFWwindow* window_ = nullptr;

    // Core Vulkan objects
    VkInstance vk_instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t queue_family_ = 0;  // graphics + present
    VkQueue queue_ = VK_NULL_HANDLE;

    // Swapchain and everything that depends on it
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchain_format_ = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR swapchain_color_space_ = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D swapchain_extent_ = {0, 0};
    uint32_t min_image_count_ = 2;
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_image_views_;
    std::vector<VkFramebuffer> framebuffers_;
    std::vector<VkSemaphore> render_finished_;  // one per swapchain image
    bool swapchain_rebuild_ = false;
    bool transparent_ = false;  // swapchain composites premultiplied alpha

    // Render pass, command buffers and per-frame synchronization
    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    FrameData frames_[kFramesInFlight];
    uint32_t frame_index_ = 0;

    // ImGui context (pointer - created by CreateContext)
    ImGuiContext* io_ctx_ = nullptr;
    bool imgui_glfw_initialized_ = false;
    bool imgui_vulkan_initialized_ = false;

    // Window properties
    int width_;
    int height_;
    bool fullscreen_ = false;

    // Metrics renderer
    MetricsRenderer metrics_;

    // Setup ImGui style and bindings
    void setup_imgui_style();

    // Vulkan setup steps (each returns false and logs on failure)
    bool create_instance();
    bool create_surface();
    bool pick_physical_device();
    bool create_device();
    bool create_swapchain();
    bool create_render_pass();
    bool create_framebuffers();
    bool create_commands_and_sync();

    // Swapchain lifetime
    void destroy_swapchain_resources(bool keep_swapchain_handle);
    bool recreate_swapchain();

    // Record, submit and present one frame
    void draw_frame(ImDrawData* draw_data);
};
