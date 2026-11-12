#include "overlay.h"

#include <backend.h>
#include <metrics.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>

namespace {

// Returns false (and logs) from the enclosing function when a Vulkan call fails.
#define VK_TRY(call, what)                                                         \
    do {                                                                           \
        VkResult vk_result_ = (call);                                              \
        if (vk_result_ != VK_SUCCESS) {                                            \
            std::cerr << "Vulkan error: " << (what) << " (VkResult "               \
                      << static_cast<int>(vk_result_) << ")" << std::endl;         \
            return false;                                                          \
        }                                                                          \
    } while (0)

// Used by the Dear ImGui Vulkan backend to report errors from its own Vulkan calls.
void imgui_check_vk_result(VkResult err) {
    if (err == VK_SUCCESS) return;
    std::cerr << "Vulkan error (imgui backend): VkResult " << static_cast<int>(err) << std::endl;
    if (err < 0) std::abort();
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*types*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*user*/) {
    const char* level = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "error" : "warning";
    std::cerr << "[vulkan validation " << level << "] " << data->pMessage << std::endl;
    return VK_FALSE;
}

// Validation layers are opt-in: GAMESTAT_VULKAN_VALIDATION=1
bool validation_requested() {
    const char* v = std::getenv("GAMESTAT_VULKAN_VALIDATION");
    return v && *v && std::strcmp(v, "0") != 0;
}

bool has_instance_layer(const char* name) {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (const auto& l : layers)
        if (std::strcmp(l.layerName, name) == 0) return true;
    return false;
}

bool has_instance_extension(const char* name) {
    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, exts.data());
    for (const auto& e : exts)
        if (std::strcmp(e.extensionName, name) == 0) return true;
    return false;
}

}  // namespace

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

    if (!glfwVulkanSupported()) {
        std::cerr << "GLFW: Vulkan is not supported on this system (no loader or ICD found)" << std::endl;
        glfwTerminate();
        return false;
    }

    // Vulkan renders through its own swapchain: GLFW must not create an OpenGL context.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);  // used when the platform can composite alpha
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
#ifdef GLFW_MOUSE_PASSTHROUGH
    glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);        // never steal clicks from the game
#endif
    glfwWindowHint(GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwSetWindowPos(window_, 10, 10);  // small margin from the top-left corner, like MangoHud
    glfwShowWindow(window_);
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    if (!create_instance() || !create_surface() || !pick_physical_device() || !create_device() ||
        !create_swapchain() || !create_render_pass() || !create_framebuffers() ||
        !create_commands_and_sync()) {
        std::cerr << "Failed to initialize Vulkan" << std::endl;
        cleanup();
        return false;
    }

    std::cerr << "Vulkan swapchain: "
              << (transparent_ ? "premultiplied alpha (transparent overlay)" : "opaque (compositing not available)")
              << std::endl;

    IMGUI_CHECKVERSION();
    io_ctx_ = ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;  // an overlay should not write imgui.ini

    ImGui_ImplGlfw_InitForVulkan(window_, true);
    imgui_glfw_initialized_ = true;

    // Initialize ImGui Vulkan backend
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_0;
    init_info.Instance = vk_instance_;
    init_info.PhysicalDevice = physical_device_;
    init_info.Device = device_;
    init_info.QueueFamily = queue_family_;
    init_info.Queue = queue_;
    init_info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
    init_info.MinImageCount = min_image_count_;
    init_info.ImageCount = static_cast<uint32_t>(swapchain_images_.size());
    init_info.PipelineInfoMain.RenderPass = render_pass_;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = imgui_check_vk_result;
    if (!ImGui_ImplVulkan_Init(&init_info)) {
        std::cerr << "Failed to initialize ImGui Vulkan backend" << std::endl;
        cleanup();
        return false;
    }
    imgui_vulkan_initialized_ = true;

    setup_imgui_style();
    metrics_.init();

    return true;
}

void Overlay::cleanup() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }

    if (imgui_vulkan_initialized_) {
        ImGui_ImplVulkan_Shutdown();
        imgui_vulkan_initialized_ = false;
    }
    if (imgui_glfw_initialized_) {
        ImGui_ImplGlfw_Shutdown();
        imgui_glfw_initialized_ = false;
    }
    if (io_ctx_) {
        ImGui::DestroyContext(io_ctx_);
        io_ctx_ = nullptr;
    }

    if (device_ != VK_NULL_HANDLE) {
        for (auto& frame : frames_) {
            if (frame.in_flight) vkDestroyFence(device_, frame.in_flight, nullptr);
            if (frame.image_available) vkDestroySemaphore(device_, frame.image_available, nullptr);
            frame = FrameData{};
        }
        if (command_pool_) {  // also frees the command buffers allocated from it
            vkDestroyCommandPool(device_, command_pool_, nullptr);
            command_pool_ = VK_NULL_HANDLE;
        }
        destroy_swapchain_resources(false);
        if (render_pass_) {
            vkDestroyRenderPass(device_, render_pass_, nullptr);
            render_pass_ = VK_NULL_HANDLE;
        }
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
        queue_ = VK_NULL_HANDLE;
    }

    if (vk_instance_ != VK_NULL_HANDLE) {
        if (surface_) {
            vkDestroySurfaceKHR(vk_instance_, surface_, nullptr);
            surface_ = VK_NULL_HANDLE;
        }
        if (debug_messenger_) {
            auto destroy_fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(vk_instance_, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroy_fn) destroy_fn(vk_instance_, debug_messenger_, nullptr);
            debug_messenger_ = VK_NULL_HANDLE;
        }
        vkDestroyInstance(vk_instance_, nullptr);
        vk_instance_ = VK_NULL_HANDLE;
    }
    physical_device_ = VK_NULL_HANDLE;

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}

// ---------------------------------------------------------------------------
// Vulkan setup
// ---------------------------------------------------------------------------

bool Overlay::create_instance() {
    uint32_t glfw_ext_count = 0;
    const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    if (!glfw_exts) {
        std::cerr << "GLFW could not determine the required Vulkan instance extensions" << std::endl;
        return false;
    }
    std::vector<const char*> extensions(glfw_exts, glfw_exts + glfw_ext_count);
    std::vector<const char*> layers;

    bool use_debug_utils = false;
    if (validation_requested()) {
        if (has_instance_layer("VK_LAYER_KHRONOS_validation") &&
            has_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            use_debug_utils = true;
        } else {
            std::cerr << "Vulkan validation requested but VK_LAYER_KHRONOS_validation is not available"
                      << std::endl;
        }
    }

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "GameStat Overlay";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "GameStat";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
    create_info.ppEnabledLayerNames = layers.data();

    VK_TRY(vkCreateInstance(&create_info, nullptr, &vk_instance_), "vkCreateInstance");

    if (use_debug_utils) {
        auto create_fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(vk_instance_, "vkCreateDebugUtilsMessengerEXT"));
        if (create_fn) {
            VkDebugUtilsMessengerCreateInfoEXT dbg = {};
            dbg.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            dbg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            dbg.pfnUserCallback = debug_messenger_callback;
            create_fn(vk_instance_, &dbg, nullptr, &debug_messenger_);
        }
    }
    return true;
}

bool Overlay::create_surface() {
    VK_TRY(glfwCreateWindowSurface(vk_instance_, window_, nullptr, &surface_),
           "glfwCreateWindowSurface");
    return true;
}

bool Overlay::pick_physical_device() {
    uint32_t count = 0;
    VK_TRY(vkEnumeratePhysicalDevices(vk_instance_, &count, nullptr), "vkEnumeratePhysicalDevices");
    if (count == 0) {
        std::cerr << "No Vulkan physical devices found" << std::endl;
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    VK_TRY(vkEnumeratePhysicalDevices(vk_instance_, &count, devices.data()), "vkEnumeratePhysicalDevices");

    int best_score = -1;
    for (VkPhysicalDevice dev : devices) {
        // Needs VK_KHR_swapchain
        uint32_t ext_count = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count, nullptr);
        std::vector<VkExtensionProperties> exts(ext_count);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count, exts.data());
        bool has_swapchain = false;
        for (const auto& e : exts)
            if (std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) has_swapchain = true;
        if (!has_swapchain) continue;

        // Needs a queue family that supports both graphics and presenting to our surface
        uint32_t qf_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(qf_count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, qfs.data());
        int family = -1;
        for (uint32_t i = 0; i < qf_count; ++i) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface_, &present);
            if ((qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
                family = static_cast<int>(i);
                break;
            }
        }
        if (family < 0) continue;

        // Needs at least one surface format and present mode
        uint32_t fmt_count = 0, mode_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface_, &fmt_count, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface_, &mode_count, nullptr);
        if (fmt_count == 0 || mode_count == 0) continue;

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        int score = 1;
        switch (props.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: score = 4; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score = 3; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: score = 2; break;
            default: break;
        }
        if (score > best_score) {
            best_score = score;
            physical_device_ = dev;
            queue_family_ = static_cast<uint32_t>(family);
        }
    }

    if (physical_device_ == VK_NULL_HANDLE) {
        std::cerr << "No Vulkan device supports swapchain presentation to this window" << std::endl;
        return false;
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device_, &props);
    std::cerr << "Vulkan device: " << props.deviceName << std::endl;
    return true;
}

bool Overlay::create_device() {
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = queue_family_;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;

    const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = 1;
    create_info.pQueueCreateInfos = &queue_info;
    create_info.enabledExtensionCount = 1;
    create_info.ppEnabledExtensionNames = device_extensions;

    VK_TRY(vkCreateDevice(physical_device_, &create_info, nullptr, &device_), "vkCreateDevice");
    vkGetDeviceQueue(device_, queue_family_, 0, &queue_);
    return true;
}

bool Overlay::create_swapchain() {
    VkSurfaceCapabilitiesKHR caps;
    VK_TRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &caps),
           "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    // Extent: use what the surface dictates, or the framebuffer size if it leaves it to us
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        int w = 0, h = 0;
        glfwGetFramebufferSize(window_, &w, &h);
        extent.width = std::clamp(static_cast<uint32_t>(w), caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(h), caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (extent.width == 0 || extent.height == 0) {
        return false;  // minimized: nothing to render to
    }

    // Format: prefer a UNORM format so ImGui colors are not gamma-converted a second time
    uint32_t fmt_count = 0;
    VK_TRY(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &fmt_count, nullptr),
           "vkGetPhysicalDeviceSurfaceFormatsKHR");
    std::vector<VkSurfaceFormatKHR> formats(fmt_count);
    VK_TRY(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &fmt_count, formats.data()),
           "vkGetPhysicalDeviceSurfaceFormatsKHR");
    VkSurfaceFormatKHR chosen = formats[0];
    const VkFormat preferred[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
    bool found = false;
    for (VkFormat want : preferred) {
        for (const auto& f : formats) {
            if (f.format == want && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosen = f;
                found = true;
                break;
            }
        }
        if (found) break;
    }
    if (chosen.format == VK_FORMAT_UNDEFINED) {
        std::cerr << "Surface reports no usable format" << std::endl;
        return false;
    }
    if (render_pass_ != VK_NULL_HANDLE && chosen.format != swapchain_format_) {
        std::cerr << "Surface format changed after the render pass was created" << std::endl;
        return false;
    }
    swapchain_format_ = chosen.format;
    swapchain_color_space_ = chosen.colorSpace;

    // Present mode: FIFO (vsync) is the only mode guaranteed by the spec
    const VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

    // Image count: one more than the minimum to avoid waiting on the driver
    min_image_count_ = std::max(caps.minImageCount, 2u);
    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) image_count = std::min(image_count, caps.maxImageCount);
    image_count = std::max(image_count, min_image_count_);

    // Prefer a premultiplied-alpha swapchain so the overlay background can be see-through
    // (ImGui blending onto a transparent clear yields premultiplied output). Fall back to opaque.
    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
        composite_alpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    } else if (!(caps.supportedCompositeAlpha & composite_alpha)) {
        const VkCompositeAlphaFlagBitsKHR candidates[] = {
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
        for (auto c : candidates) {
            if (caps.supportedCompositeAlpha & c) {
                composite_alpha = c;
                break;
            }
        }
    }
    transparent_ = (composite_alpha == VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR);

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface_;
    create_info.minImageCount = image_count;
    create_info.imageFormat = chosen.format;
    create_info.imageColorSpace = chosen.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;  // graphics and present share one family
    create_info.preTransform = caps.currentTransform;
    create_info.compositeAlpha = composite_alpha;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = swapchain_;

    VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
    VK_TRY(vkCreateSwapchainKHR(device_, &create_info, nullptr, &new_swapchain), "vkCreateSwapchainKHR");
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    }
    swapchain_ = new_swapchain;
    swapchain_extent_ = extent;

    uint32_t actual_count = 0;
    VK_TRY(vkGetSwapchainImagesKHR(device_, swapchain_, &actual_count, nullptr), "vkGetSwapchainImagesKHR");
    swapchain_images_.resize(actual_count);
    VK_TRY(vkGetSwapchainImagesKHR(device_, swapchain_, &actual_count, swapchain_images_.data()),
           "vkGetSwapchainImagesKHR");

    swapchain_image_views_.assign(actual_count, VK_NULL_HANDLE);
    render_finished_.assign(actual_count, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < actual_count; ++i) {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = swapchain_images_[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = swapchain_format_;
        view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VK_TRY(vkCreateImageView(device_, &view_info, nullptr, &swapchain_image_views_[i]),
               "vkCreateImageView");

        // One "render finished" semaphore per swapchain image: presentation of image i
        // is the only thing that waits on semaphore i.
        VkSemaphoreCreateInfo sem_info = {};
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_TRY(vkCreateSemaphore(device_, &sem_info, nullptr, &render_finished_[i]), "vkCreateSemaphore");
    }
    return true;
}

bool Overlay::create_render_pass() {
    VkAttachmentDescription color = {};
    color.format = swapchain_format_;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    // Wait for the acquire semaphore's stage before writing the color attachment
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &color;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    VK_TRY(vkCreateRenderPass(device_, &info, nullptr, &render_pass_), "vkCreateRenderPass");
    return true;
}

bool Overlay::create_framebuffers() {
    framebuffers_.assign(swapchain_image_views_.size(), VK_NULL_HANDLE);
    for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = render_pass_;
        info.attachmentCount = 1;
        info.pAttachments = &swapchain_image_views_[i];
        info.width = swapchain_extent_.width;
        info.height = swapchain_extent_.height;
        info.layers = 1;
        VK_TRY(vkCreateFramebuffer(device_, &info, nullptr, &framebuffers_[i]), "vkCreateFramebuffer");
    }
    return true;
}

bool Overlay::create_commands_and_sync() {
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = queue_family_;
    VK_TRY(vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_), "vkCreateCommandPool");

    VkCommandBuffer buffers[kFramesInFlight];
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool_;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = kFramesInFlight;
    VK_TRY(vkAllocateCommandBuffers(device_, &alloc_info, buffers), "vkAllocateCommandBuffers");

    for (uint32_t i = 0; i < kFramesInFlight; ++i) {
        frames_[i].command_buffer = buffers[i];

        VkSemaphoreCreateInfo sem_info = {};
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_TRY(vkCreateSemaphore(device_, &sem_info, nullptr, &frames_[i].image_available), "vkCreateSemaphore");

        // Start signaled so the very first vkWaitForFences returns immediately
        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_TRY(vkCreateFence(device_, &fence_info, nullptr, &frames_[i].in_flight), "vkCreateFence");
    }
    return true;
}

void Overlay::destroy_swapchain_resources(bool keep_swapchain_handle) {
    for (VkFramebuffer fb : framebuffers_)
        if (fb) vkDestroyFramebuffer(device_, fb, nullptr);
    framebuffers_.clear();
    for (VkImageView view : swapchain_image_views_)
        if (view) vkDestroyImageView(device_, view, nullptr);
    swapchain_image_views_.clear();
    for (VkSemaphore sem : render_finished_)
        if (sem) vkDestroySemaphore(device_, sem, nullptr);
    render_finished_.clear();
    swapchain_images_.clear();

    if (!keep_swapchain_handle && swapchain_) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

bool Overlay::recreate_swapchain() {
    vkDeviceWaitIdle(device_);
    destroy_swapchain_resources(true);  // the old handle is handed to vkCreateSwapchainKHR
    if (!create_swapchain() || !create_framebuffers()) {
        return false;
    }
    ImGui_ImplVulkan_SetMinImageCount(min_image_count_);
    return true;
}

void Overlay::setup_imgui_style() {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    // Clean scalable sans font at a small, MangoHud-like size
    io.Fonts->AddFontDefaultVector();
    style.FontSizeBase = 15.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.008f, 0.008f, 0.008f, 0.50f);  // MangoHud: #020202 @ 50%
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_Separator] = ImVec4(1.00f, 1.00f, 1.00f, 0.12f);    // barely visible divider

    // Flat, compact, square corners
    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowPadding = ImVec2(8, 6);
    style.ItemSpacing = ImVec2(0, 2);
    style.SeparatorTextBorderSize = 1.0f;
}

ImVec2 Overlay::framebuffer_size() const {
    int w, h;
    if (window_) {
        glfwGetFramebufferSize(window_, &w, &h);
        return ImVec2(static_cast<float>(w), static_cast<float>(h));
    }
    return ImVec2(static_cast<float>(width_), static_cast<float>(height_));
}

// ---------------------------------------------------------------------------
// Frame loop
// ---------------------------------------------------------------------------

void Overlay::draw_frame(ImDrawData* draw_data) {
    FrameData& frame = frames_[frame_index_];

    // Wait until the GPU is done with the resources of this frame slot
    if (vkWaitForFences(device_, 1, &frame.in_flight, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        return;
    }

    uint32_t image_index = 0;
    VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, frame.image_available,
                                            VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchain_rebuild_ = true;  // fence stays signaled, nothing was submitted
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "Vulkan error: vkAcquireNextImageKHR (VkResult " << static_cast<int>(result) << ")"
                  << std::endl;
        return;
    }
    if (result == VK_SUBOPTIMAL_KHR) swapchain_rebuild_ = true;  // still usable this frame

    vkResetFences(device_, 1, &frame.in_flight);
    vkResetCommandBuffer(frame.command_buffer, 0);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(frame.command_buffer, &begin_info);

    // Fully transparent when the swapchain composites alpha, plain black otherwise
    VkClearValue clear_value = {};
    clear_value.color = {{0.0f, 0.0f, 0.0f, transparent_ ? 0.0f : 1.0f}};

    VkRenderPassBeginInfo rp_begin = {};
    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.renderPass = render_pass_;
    rp_begin.framebuffer = framebuffers_[image_index];
    rp_begin.renderArea.offset = {0, 0};
    rp_begin.renderArea.extent = swapchain_extent_;
    rp_begin.clearValueCount = 1;
    rp_begin.pClearValues = &clear_value;
    vkCmdBeginRenderPass(frame.command_buffer, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    ImGui_ImplVulkan_RenderDrawData(draw_data, frame.command_buffer);

    vkCmdEndRenderPass(frame.command_buffer);
    vkEndCommandBuffer(frame.command_buffer);

    const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &frame.image_available;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &frame.command_buffer;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &render_finished_[image_index];
    if (vkQueueSubmit(queue_, 1, &submit, frame.in_flight) != VK_SUCCESS) {
        // The fence was already reset and will never signal: this frame slot is unusable.
        std::cerr << "Vulkan error: vkQueueSubmit failed, closing overlay" << std::endl;
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
        return;
    }

    VkPresentInfoKHR present = {};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &render_finished_[image_index];
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain_;
    present.pImageIndices = &image_index;
    result = vkQueuePresentKHR(queue_, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        swapchain_rebuild_ = true;
    } else if (result != VK_SUCCESS) {
        std::cerr << "Vulkan error: vkQueuePresentKHR (VkResult " << static_cast<int>(result) << ")"
                  << std::endl;
    }

    frame_index_ = (frame_index_ + 1) % kFramesInFlight;
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

        // Nothing can be presented to a zero-sized (minimized) window
        int fb_w = 0, fb_h = 0;
        glfwGetFramebufferSize(window_, &fb_w, &fb_h);
        if (fb_w == 0 || fb_h == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (swapchain_rebuild_) {
            if (!recreate_swapchain()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            swapchain_rebuild_ = false;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        auto telemetry = communicator.get_telemetry();
        double age = communicator.get_age();
        metrics_.update(telemetry, age);

        ImVec2 fb_size = framebuffer_size();


        // The ImGui window fills the (small) overlay window; the overlay window is resized
        // to fit the content below.
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(fb_size);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoScrollbar |
                                        ImGuiWindowFlags_NoScrollWithMouse |
                                        ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoBringToFrontOnFocus |
                                        ImGuiWindowFlags_NoNavFocus ;

        ImGui::Begin("GameStat Overlay", nullptr, window_flags);
        metrics_.render();
        const ImVec2 pad = ImGui::GetStyle().WindowPadding;
        ImGui::End();

        // Fit the OS window to the content (applies from the next frame)
        const int want_w = static_cast<int>(std::ceil(metrics_.content_size().x + pad.x * 2.0f));
        const int want_h = static_cast<int>(std::ceil(metrics_.content_size().y + pad.y * 2.0f));
        int cur_w = 0, cur_h = 0;
        glfwGetWindowSize(window_, &cur_w, &cur_h);
        if (want_w > 0 && want_h > 0 && (cur_w != want_w || cur_h != want_h)) {
            glfwSetWindowSize(window_, want_w, want_h);
        }

        // Render ImGui drawing commands, then submit and present the frame
        ImGui::Render();
        draw_frame(ImGui::GetDrawData());
    }

    vkDeviceWaitIdle(device_);
    return true;
}
