#define HAMUN_RENDERER_BUILD
#include "renderer_core.hpp"
#include <vulkan/vulkan.h>
#include <cstring>
#include <string>
static thread_local std::string lastError;
extern "C" int hamun_renderer_probe(HamunRendererInfo* out) {
    lastError.clear();
    if (!out) { lastError="Null output pointer"; return 0; }
    *out = {};
    out->abiVersion=1;
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName="Hamun Renderer";
    app.apiVersion=VK_API_VERSION_1_1;
    VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create.pApplicationInfo=&app;
    VkInstance instance=VK_NULL_HANDLE;
    if(vkCreateInstance(&create,nullptr,&instance)!=VK_SUCCESS){
        lastError="Unable to create Vulkan instance";return 0;
    }
    uint32_t count=0;
    VkResult result=vkEnumeratePhysicalDevices(instance,&count,nullptr);
    if(result!=VK_SUCCESS || count==0){
        lastError="No Vulkan physical device detected";
        vkDestroyInstance(instance,nullptr);return 0;
    }
    VkPhysicalDevice gpu=VK_NULL_HANDLE;
    result=vkEnumeratePhysicalDevices(instance,&count,&gpu);
    if(result!=VK_SUCCESS){
        lastError="Failed to enumerate GPU";
        vkDestroyInstance(instance,nullptr);return 0;
    }
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(gpu,&properties);
    out->gpuVendor=properties.vendorID;
    out->vulkanVersion=properties.apiVersion;
    std::memcpy(out->deviceName,properties.deviceName,sizeof(out->deviceName));
    out->deviceName[sizeof(out->deviceName)-1]=0;
    vkDestroyInstance(instance,nullptr);
    return 1;
}
extern "C" const char* hamun_renderer_last_error(){return lastError.c_str();}
