#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#ifndef VK_LAYER_EXPORT
#if defined(_WIN32)
#define VK_LAYER_EXPORT __declspec(dllexport)
#else
#define VK_LAYER_EXPORT __attribute__((visibility("default")))
#endif
#endif

#include "gamestat/event_sender.hpp"
#include <mutex>
#include <unordered_map>
#include <cstring>
#include <iterator>

namespace {
using Gipa = PFN_vkGetInstanceProcAddr;
using Gdpa = PFN_vkGetDeviceProcAddr;
std::mutex g_mutex;
Gipa g_global_gipa = nullptr;
std::unordered_map<VkInstance, Gipa> g_instance_gipa;
std::unordered_map<VkPhysicalDevice, VkInstance> g_physical_to_instance;
std::unordered_map<VkDevice, Gdpa> g_device_gdpa;
std::unordered_map<void*, PFN_vkQueuePresentKHR> g_present_by_dispatch;
void* dispatch_key(void* handle) { return handle ? *reinterpret_cast<void**>(handle) : nullptr; }

VKAPI_ATTR VkResult VKAPI_CALL layer_enumerate_physical_devices(VkInstance instance, uint32_t* count, VkPhysicalDevice* devices) {
    Gipa next = nullptr;
    { std::lock_guard<std::mutex> lock(g_mutex); auto it=g_instance_gipa.find(instance); if(it!=g_instance_gipa.end()) next=it->second; }
    auto fn = next ? reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(next(instance, "vkEnumeratePhysicalDevices")) : nullptr;
    if (!fn) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult result = fn(instance, count, devices);
    if ((result == VK_SUCCESS || result == VK_INCOMPLETE) && devices && count) {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (uint32_t i=0; i<*count; ++i) g_physical_to_instance[devices[i]] = instance;
    }
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL layer_queue_present(VkQueue queue, const VkPresentInfoKHR* info) {
    PFN_vkQueuePresentKHR next = nullptr;
    { std::lock_guard<std::mutex> lock(g_mutex); auto it=g_present_by_dispatch.find(dispatch_key(queue)); if(it!=g_present_by_dispatch.end()) next=it->second; }
    if (!next) return VK_ERROR_INITIALIZATION_FAILED;
    const auto start = gamestat::monotonic_raw_ns();
    const VkResult result = next(queue, info);
    const auto end = gamestat::monotonic_raw_ns();
    gamestat::send_frame_event({"vulkan", "queue_present", start, end, static_cast<std::int64_t>(result), info ? info->swapchainCount : 0, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(queue))});
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL layer_create_instance(const VkInstanceCreateInfo* ci, const VkAllocationCallbacks* allocator, VkInstance* instance) {
    auto* chain = reinterpret_cast<VkLayerInstanceCreateInfo*>(const_cast<void*>(ci->pNext));
    while (chain && !(chain->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO && chain->function == VK_LAYER_LINK_INFO))
        chain = reinterpret_cast<VkLayerInstanceCreateInfo*>(const_cast<void*>(chain->pNext));
    if (!chain || !chain->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    auto* link = chain->u.pLayerInfo;
    Gipa next_gipa = link->pfnNextGetInstanceProcAddr;
    chain->u.pLayerInfo = link->pNext;
    auto create = reinterpret_cast<PFN_vkCreateInstance>(next_gipa(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!create) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult result = create(ci, allocator, instance);
    if (result == VK_SUCCESS) { std::lock_guard<std::mutex> lock(g_mutex); g_global_gipa = next_gipa; g_instance_gipa[*instance] = next_gipa; }
    return result;
}

VKAPI_ATTR void VKAPI_CALL layer_destroy_instance(VkInstance instance, const VkAllocationCallbacks* allocator) {
    Gipa next = nullptr;
    { std::lock_guard<std::mutex> lock(g_mutex); auto it=g_instance_gipa.find(instance); if(it!=g_instance_gipa.end()){next=it->second;g_instance_gipa.erase(it);} }
    auto destroy = next ? reinterpret_cast<PFN_vkDestroyInstance>(next(instance, "vkDestroyInstance")) : nullptr;
    if (destroy) destroy(instance, allocator);
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto it=g_physical_to_instance.begin(); it!=g_physical_to_instance.end();) it = (it->second==instance) ? g_physical_to_instance.erase(it) : std::next(it);
}

VKAPI_ATTR VkResult VKAPI_CALL layer_create_device(VkPhysicalDevice physical, const VkDeviceCreateInfo* ci, const VkAllocationCallbacks* allocator, VkDevice* device) {
    auto* chain = reinterpret_cast<VkLayerDeviceCreateInfo*>(const_cast<void*>(ci->pNext));
    while (chain && !(chain->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO && chain->function == VK_LAYER_LINK_INFO))
        chain = reinterpret_cast<VkLayerDeviceCreateInfo*>(const_cast<void*>(chain->pNext));
    if (!chain || !chain->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    auto* link = chain->u.pLayerInfo;
    Gdpa next_gdpa = link->pfnNextGetDeviceProcAddr;
    Gipa next_gipa = link->pfnNextGetInstanceProcAddr;
    chain->u.pLayerInfo = link->pNext;
    VkInstance instance = VK_NULL_HANDLE;
    { std::lock_guard<std::mutex> lock(g_mutex); auto it=g_physical_to_instance.find(physical); if(it!=g_physical_to_instance.end()) instance=it->second; }
    if (instance == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
    auto create = reinterpret_cast<PFN_vkCreateDevice>(next_gipa(instance, "vkCreateDevice"));
    if (!create) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult result = create(physical, ci, allocator, device);
    if (result == VK_SUCCESS) { std::lock_guard<std::mutex> lock(g_mutex); g_device_gdpa[*device]=next_gdpa; }
    return result;
}

VKAPI_ATTR void VKAPI_CALL layer_destroy_device(VkDevice device, const VkAllocationCallbacks* allocator) {
    Gdpa next = nullptr;
    { std::lock_guard<std::mutex> lock(g_mutex); auto it=g_device_gdpa.find(device); if(it!=g_device_gdpa.end()){next=it->second;g_device_gdpa.erase(it);} }
    auto destroy = next ? reinterpret_cast<PFN_vkDestroyDevice>(next(device, "vkDestroyDevice")) : nullptr;
    if (destroy) destroy(device, allocator);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_present_by_dispatch.erase(dispatch_key(device));
}
}

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vkGetInstanceProcAddr(VkInstance instance, const char* name);

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vkGetDeviceProcAddr(VkDevice device, const char* name);

extern "C" VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* version) {
    if (!version || version->loaderLayerInterfaceVersion < 2) return VK_ERROR_INITIALIZATION_FAILED;
    if (version->loaderLayerInterfaceVersion > 2) version->loaderLayerInterfaceVersion = 2;
    version->pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    version->pfnGetDeviceProcAddr = vkGetDeviceProcAddr;
    version->pfnGetPhysicalDeviceProcAddr = nullptr;
    return VK_SUCCESS;
}

extern "C" VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* name) {
    if (!name) return nullptr;
    if (!std::strcmp(name,"vkGetInstanceProcAddr")) return reinterpret_cast<PFN_vkVoidFunction>(vkGetInstanceProcAddr);
    if (!std::strcmp(name,"vkCreateInstance")) return reinterpret_cast<PFN_vkVoidFunction>(layer_create_instance);
    if (!std::strcmp(name,"vkDestroyInstance")) return reinterpret_cast<PFN_vkVoidFunction>(layer_destroy_instance);
    if (!std::strcmp(name,"vkCreateDevice")) return reinterpret_cast<PFN_vkVoidFunction>(layer_create_device);
    if (!std::strcmp(name,"vkEnumeratePhysicalDevices")) return reinterpret_cast<PFN_vkVoidFunction>(layer_enumerate_physical_devices);
    Gipa next=nullptr;
    { std::lock_guard<std::mutex> lock(g_mutex); auto it=g_instance_gipa.find(instance); if(it!=g_instance_gipa.end()) next=it->second; else if (instance == VK_NULL_HANDLE) next=g_global_gipa; }
    return next ? next(instance,name) : nullptr;
}

extern "C" VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char* name) {
    if (!name) return nullptr;
    if (!std::strcmp(name,"vkGetDeviceProcAddr")) return reinterpret_cast<PFN_vkVoidFunction>(vkGetDeviceProcAddr);
    Gdpa next=nullptr;
    { std::lock_guard<std::mutex> lock(g_mutex); auto it=g_device_gdpa.find(device); if(it!=g_device_gdpa.end()) next=it->second; }
    if (!next) return nullptr;
    if (!std::strcmp(name,"vkQueuePresentKHR")) {
        auto fn=reinterpret_cast<PFN_vkQueuePresentKHR>(next(device,name));
        if (fn) { std::lock_guard<std::mutex> lock(g_mutex); g_present_by_dispatch[dispatch_key(device)]=fn; }
        return reinterpret_cast<PFN_vkVoidFunction>(layer_queue_present);
    }
    if (!std::strcmp(name,"vkDestroyDevice")) return reinterpret_cast<PFN_vkVoidFunction>(layer_destroy_device);
    return next(device,name);
}
