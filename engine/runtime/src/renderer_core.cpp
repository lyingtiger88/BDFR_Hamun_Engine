#define HAMUN_RENDERER_BUILD
#include "renderer_core.hpp"
#include <vulkan/vulkan.h>
#include <cstring>
#include <string>
#include <stdexcept>
#include <cmath>
#include <algorithm>
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

extern "C" int hamun_renderer_mesh_preview(
    uint32_t width,uint32_t height,uint8_t* rgba,uint64_t byteCount,
    const float* xyz,uint32_t vertices,const uint32_t* indices,uint32_t count){
    lastError.clear();
    if(!xyz||!indices||vertices>1000000||count>3000000||count%3!=0){
        lastError="Invalid mesh buffer";return 0;
    }
    if(!hamun_renderer_offscreen_clear(width,height,rgba,byteCount))return 0;
    try {
        // Transitional mesh compositing: real scene triangles, software depth
        // on Vulkan-generated background. Next stage migrates rasterization
        // into Vulkan vertex/index buffers and graphics pipeline.
        struct P {float x,y,z;};
        const float a=0.45f,b=0.32f;
        const float cy=std::cos(a),sy=std::sin(a),cp=std::cos(b),sp=std::sin(b);
        std::vector<P> projected(vertices);
        float radius=0.01f;
        for(uint32_t i=0;i<vertices;++i)
            for(int c=0;c<3;++c) radius=std::max(radius,std::abs(xyz[i*3+c]));
        const float scale=float(std::min(width,height))*.35f/radius;
        for(uint32_t i=0;i<vertices;++i){
            float x=xyz[i*3],y=xyz[i*3+1],z=xyz[i*3+2];
            float rx=cy*x+sy*y,ry=-sy*x+cy*y;
            float rz=cp*z-sp*ry,depth=sp*z+cp*ry;
            projected[i]={width*.5f+rx*scale,height*.52f-rz*scale,depth};
        }
        std::vector<float> zbuf(size_t(width)*height,-1e30f);
        auto edge=[](const P& a,const P& b,float x,float y){
            return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);
        };
        for(uint32_t i=0;i<count;i+=3){
            uint32_t ia=indices[i],ib=indices[i+1],ic=indices[i+2];
            if(ia>=vertices||ib>=vertices||ic>=vertices)continue;
            const P A=projected[ia],B=projected[ib],C=projected[ic];
            float area=edge(A,B,C.x,C.y);
            if(std::abs(area)<1e-5f)continue;
            int x0=std::max(0,int(std::floor(std::min({A.x,B.x,C.x}))));
            int x1=std::min(int(width)-1,int(std::ceil(std::max({A.x,B.x,C.x}))));
            int y0=std::max(0,int(std::floor(std::min({A.y,B.y,C.y}))));
            int y1=std::min(int(height)-1,int(std::ceil(std::max({A.y,B.y,C.y}))));
            float ux=xyz[ib*3]-xyz[ia*3],uy=xyz[ib*3+1]-xyz[ia*3+1],uz=xyz[ib*3+2]-xyz[ia*3+2];
            float vx=xyz[ic*3]-xyz[ia*3],vy=xyz[ic*3+1]-xyz[ia*3+1],vz=xyz[ic*3+2]-xyz[ia*3+2];
            float nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
            float nlen=std::sqrt(nx*nx+ny*ny+nz*nz)+1e-8f;
            float diffuse=std::abs((nx*.35f+ny*.4f+nz*.85f)/nlen);
            float intensity=.23f+.75f*diffuse;
            for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
                float px=x+.5f,py=y+.5f;
                float wa=edge(B,C,px,py)/area,wb=edge(C,A,px,py)/area,wc=1-wa-wb;
                if(wa<0||wb<0||wc<0)continue;
                float depth=wa*A.z+wb*B.z+wc*C.z;
                size_t offset=size_t(y)*width+x;
                if(depth<=zbuf[offset])continue;
                zbuf[offset]=depth;
                uint8_t* pixel=rgba+offset*4;
                pixel[0]=uint8_t(195*intensity);
                pixel[1]=uint8_t(215*intensity);
                pixel[2]=uint8_t(232*intensity);
                pixel[3]=255;
            }
        }
        return 1;
    }catch(const std::exception& ex){lastError=ex.what();return 0;}
}
