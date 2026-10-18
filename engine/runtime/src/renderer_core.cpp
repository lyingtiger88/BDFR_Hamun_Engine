#define HAMUN_RENDERER_BUILD
#include "renderer_core.hpp"
#include <vulkan/vulkan.h>
#include <cstring>
#include <string>
#include <stdexcept>
#include <vector>
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
    std::vector<VkPhysicalDevice> devices(count);
    result=vkEnumeratePhysicalDevices(instance,&count,devices.data());
    if(result!=VK_SUCCESS){
        lastError="Failed to enumerate GPU";
        vkDestroyInstance(instance,nullptr);return 0;
    }
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(devices[0],&properties);
    out->gpuVendor=properties.vendorID;
    out->vulkanVersion=properties.apiVersion;
    std::memcpy(out->deviceName,properties.deviceName,sizeof(out->deviceName));
    out->deviceName[sizeof(out->deviceName)-1]=0;
    vkDestroyInstance(instance,nullptr);
    return 1;
}
extern "C" const char* hamun_renderer_last_error(){return lastError.c_str();}

extern "C" int hamun_renderer_offscreen_clear(uint32_t width,uint32_t height,uint8_t* rgba,uint64_t byteCount){
    lastError.clear();
    if(!rgba||width<1||height<1||width>4096||height>4096||
       byteCount<uint64_t(width)*height*4){lastError="Invalid output image size/buffer";return 0;}
    VkInstance instance=VK_NULL_HANDLE;
    VkDevice device=VK_NULL_HANDLE;
    VkImage img=VK_NULL_HANDLE;
    VkDeviceMemory memory=VK_NULL_HANDLE;
    VkCommandPool pool=VK_NULL_HANDLE;
    VkCommandBuffer cmd=VK_NULL_HANDLE;
    auto cleanup=[&](){
        if(device){
            vkDeviceWaitIdle(device);
            if(pool)vkDestroyCommandPool(device,pool,nullptr);
            if(img)vkDestroyImage(device,img,nullptr);
            if(memory)vkFreeMemory(device,memory,nullptr);
            vkDestroyDevice(device,nullptr);
        }
        if(instance)vkDestroyInstance(instance,nullptr);
    };
    try{
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName="Hamun Offscreen";app.apiVersion=VK_API_VERSION_1_1;
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ici.pApplicationInfo=&app;
        if(vkCreateInstance(&ici,nullptr,&instance)!=VK_SUCCESS)throw std::runtime_error("Vulkan instance failed");
        uint32_t count=0;
        if(vkEnumeratePhysicalDevices(instance,&count,nullptr)!=VK_SUCCESS||!count)
            throw std::runtime_error("No Vulkan GPU");
        std::vector<VkPhysicalDevice> gpus(count);
        if(vkEnumeratePhysicalDevices(instance,&count,gpus.data())!=VK_SUCCESS)
            throw std::runtime_error("GPU enumeration failed");
        VkPhysicalDevice gpu=VK_NULL_HANDLE;uint32_t family=UINT32_MAX;
        for(auto candidate:gpus){
            VkFormatProperties fmt{};
            vkGetPhysicalDeviceFormatProperties(candidate,VK_FORMAT_R8G8B8A8_UNORM,&fmt);
            if(!(fmt.linearTilingFeatures&VK_FORMAT_FEATURE_TRANSFER_DST_BIT))continue;
            uint32_t families=0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate,&families,nullptr);
            std::vector<VkQueueFamilyProperties> p(families);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate,&families,p.data());
            for(uint32_t i=0;i<families;++i){
                if(p[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){gpu=candidate;family=i;break;}
            }
            if(gpu)break;
        }
        if(!gpu)throw std::runtime_error("No compatible linear RGBA Vulkan GPU");
        float priority=1.0f;
        VkDeviceQueueCreateInfo queueCI{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueCI.queueFamilyIndex=family;queueCI.queueCount=1;queueCI.pQueuePriorities=&priority;
        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&queueCI;
        if(vkCreateDevice(gpu,&dci,nullptr,&device)!=VK_SUCCESS)throw std::runtime_error("Device failed");
        VkQueue queue=VK_NULL_HANDLE;vkGetDeviceQueue(device,family,0,&queue);
        VkImageCreateInfo imgCI{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imgCI.imageType=VK_IMAGE_TYPE_2D;imgCI.format=VK_FORMAT_R8G8B8A8_UNORM;
        imgCI.extent={width,height,1};imgCI.mipLevels=1;imgCI.arrayLayers=1;
        imgCI.samples=VK_SAMPLE_COUNT_1_BIT;imgCI.tiling=VK_IMAGE_TILING_LINEAR;
        imgCI.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT;imgCI.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        if(vkCreateImage(device,&imgCI,nullptr,&img)!=VK_SUCCESS)throw std::runtime_error("Offscreen image failed");
        VkMemoryRequirements req{};vkGetImageMemoryRequirements(device,img,&req);
        VkPhysicalDeviceMemoryProperties props{};vkGetPhysicalDeviceMemoryProperties(gpu,&props);
        uint32_t mt=UINT32_MAX;
        for(uint32_t i=0;i<props.memoryTypeCount;++i)
            if((req.memoryTypeBits&(1u<<i))&&
               (props.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==
               (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){mt=i;break;}
        if(mt==UINT32_MAX)throw std::runtime_error("No host-visible linear image memory");
        VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize=req.size;mai.memoryTypeIndex=mt;
        if(vkAllocateMemory(device,&mai,nullptr,&memory)!=VK_SUCCESS)throw std::runtime_error("Memory alloc failed");
        if(vkBindImageMemory(device,img,memory,0)!=VK_SUCCESS)throw std::runtime_error("Image bind failed");
        VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pci.queueFamilyIndex=family;
        if(vkCreateCommandPool(device,&pci,nullptr,&pool)!=VK_SUCCESS)throw std::runtime_error("Command pool failed");
        VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cai.commandPool=pool;cai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;cai.commandBufferCount=1;
        if(vkAllocateCommandBuffers(device,&cai,&cmd)!=VK_SUCCESS)throw std::runtime_error("Command buffer failed");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        if(vkBeginCommandBuffer(cmd,&begin)!=VK_SUCCESS)throw std::runtime_error("Begin failed");
        VkImageMemoryBarrier before{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        before.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;before.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        before.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        before.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        before.image=img;before.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        before.subresourceRange.levelCount=1;before.subresourceRange.layerCount=1;
        before.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,0,nullptr,0,nullptr,1,&before);
        VkClearColorValue color{{0.035f,0.10f,0.17f,1.0f}};
        VkImageSubresourceRange range{};
        range.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;range.levelCount=1;range.layerCount=1;
        vkCmdClearColorImage(cmd,img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&color,1,&range);
        VkImageMemoryBarrier after=before;
        after.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        after.newLayout=VK_IMAGE_LAYOUT_GENERAL;
        after.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        after.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
                             0,0,nullptr,0,nullptr,1,&after);
        if(vkEndCommandBuffer(cmd)!=VK_SUCCESS)throw std::runtime_error("End failed");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
        if(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE)!=VK_SUCCESS)throw std::runtime_error("Submit failed");
        if(vkQueueWaitIdle(queue)!=VK_SUCCESS)throw std::runtime_error("GPU wait failed");
        VkImageSubresource sub{VK_IMAGE_ASPECT_COLOR_BIT,0,0};
        VkSubresourceLayout layout{};
        vkGetImageSubresourceLayout(device,img,&sub,&layout);
        void* mapped=nullptr;
        if(vkMapMemory(device,memory,0,VK_WHOLE_SIZE,0,&mapped)!=VK_SUCCESS)throw std::runtime_error("Map failed");
        auto* start=reinterpret_cast<const uint8_t*>(mapped)+layout.offset;
        for(uint32_t y=0;y<height;++y)
            std::memcpy(rgba+uint64_t(y)*width*4,start+uint64_t(y)*layout.rowPitch,uint64_t(width)*4);
        vkUnmapMemory(device,memory);
        cleanup();return 1;
    }catch(const std::exception& ex){lastError=ex.what();cleanup();return 0;}
}
