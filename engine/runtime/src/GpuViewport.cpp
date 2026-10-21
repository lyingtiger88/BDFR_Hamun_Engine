#define HAMUN_RENDERER_BUILD
#include "renderer_core.hpp"
#include <vulkan/vulkan.h>
#include <windows.h>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <filesystem>
#include <cstddef>
#include <array>

namespace {
struct Vertex {float p[3],n[3];};
struct Params {float mvp[16],dir[4],color[4];};
void require(VkResult r,const char* s){if(r!=VK_SUCCESS)throw std::runtime_error(s);}
uint32_t memoryType(VkPhysicalDevice gpu,uint32_t bits,VkMemoryPropertyFlags flags){
    VkPhysicalDeviceMemoryProperties m{};
    vkGetPhysicalDeviceMemoryProperties(gpu,&m);
    for(uint32_t i=0;i<m.memoryTypeCount;++i)
        if((bits&(1u<<i))&&(m.memoryTypes[i].propertyFlags&flags)==flags)return i;
    throw std::runtime_error("No suitable Vulkan memory type");
}
std::vector<char> binary(const wchar_t* filename){
    HMODULE module=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&binary),&module);
    wchar_t path[MAX_PATH]={};
    GetModuleFileNameW(module,path,MAX_PATH);
    auto name=std::filesystem::path(path).parent_path()/L"shaders"/filename;
    std::ifstream f(name,std::ios::binary|std::ios::ate);
    if(!f)throw std::runtime_error("Viewport SPIR-V missing beside HamunRenderer.dll");
    auto size=f.tellg();
    if(size<=0||size%4)throw std::runtime_error("Invalid SPIR-V file");
    std::vector<char> bytes(static_cast<size_t>(size));
    f.seekg(0);f.read(bytes.data(),size);
    if(!f)throw std::runtime_error("Unable to read SPIR-V");
    return bytes;
}
struct PersistentVkContext {
    VkInstance instance=VK_NULL_HANDLE;
    VkPhysicalDevice gpu=VK_NULL_HANDLE;
    VkDevice device=VK_NULL_HANDLE;
    VkQueue queue=VK_NULL_HANDLE;
    VkPipelineCache pipelineCache=VK_NULL_HANDLE;
    VkRenderPass mainPass=VK_NULL_HANDLE,depthPass=VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout=VK_NULL_HANDLE;
    VkPipelineLayout mainLayout=VK_NULL_HANDLE,sunLayout=VK_NULL_HANDLE;
    VkPipeline mainPipeline=VK_NULL_HANDLE,sunPipeline=VK_NULL_HANDLE;
    uint32_t pipelineW=0,pipelineH=0,pipelineShadowSize=0;
    VkBuffer meshBuffer=VK_NULL_HANDLE;
    VkDeviceMemory meshMemory=VK_NULL_HANDLE;
    uint64_t meshKey=0;
    VkShaderModule mainVert=VK_NULL_HANDLE,mainFrag=VK_NULL_HANDLE,sunDepthVert=VK_NULL_HANDLE;
    uint32_t family=UINT32_MAX;
    PersistentVkContext(){
        try{
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.apiVersion=VK_API_VERSION_1_1;app.pApplicationName="Hamun GPU Viewport";
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ici.pApplicationInfo=&app;
        require(vkCreateInstance(&ici,nullptr,&instance),"Create instance failed");
        uint32_t gpuCount=0;
        require(vkEnumeratePhysicalDevices(instance,&gpuCount,nullptr),"Enumerate GPUs failed");
        if(!gpuCount)throw std::runtime_error("No Vulkan GPU");
        std::vector<VkPhysicalDevice> gpus(gpuCount);
        require(vkEnumeratePhysicalDevices(instance,&gpuCount,gpus.data()),"Enumerate GPU list failed");
        gpu=VK_NULL_HANDLE;family=UINT32_MAX;
        for(auto candidate:gpus){
            uint32_t count=0;vkGetPhysicalDeviceQueueFamilyProperties(candidate,&count,nullptr);
            std::vector<VkQueueFamilyProperties> props(count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate,&count,props.data());
            VkFormatProperties cf{},df{};
            vkGetPhysicalDeviceFormatProperties(candidate,VK_FORMAT_R8G8B8A8_UNORM,&cf);
            vkGetPhysicalDeviceFormatProperties(candidate,VK_FORMAT_D32_SFLOAT,&df);
            if(!(cf.optimalTilingFeatures&VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)||
               !(df.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))continue;
            for(uint32_t q=0;q<count;++q)if(props[q].queueFlags&VK_QUEUE_GRAPHICS_BIT){
                gpu=candidate;family=q;break;
            }
            if(gpu)break;
        }
        if(!gpu)throw std::runtime_error("No compatible Vulkan graphics queue");
        float priority=1.f;
        VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;
        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qi;
        require(vkCreateDevice(gpu,&dci,nullptr,&device),"Create GPU device failed");
        vkGetDeviceQueue(device,family,0,&queue);
        // Retain compiled graphics pipeline data for this Vulkan device.
        VkPipelineCacheCreateInfo cacheInfo{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
        require(vkCreatePipelineCache(device,&cacheInfo,nullptr,&pipelineCache),
                "Create persistent pipeline cache failed");

        }catch(...){
            if(mainPipeline&&device)vkDestroyPipeline(device,mainPipeline,nullptr);
            if(sunPipeline&&device)vkDestroyPipeline(device,sunPipeline,nullptr);
            if(mainLayout&&device)vkDestroyPipelineLayout(device,mainLayout,nullptr);
            if(sunLayout&&device)vkDestroyPipelineLayout(device,sunLayout,nullptr);
            if(descriptorLayout&&device)vkDestroyDescriptorSetLayout(device,descriptorLayout,nullptr);
            if(mainPass&&device)vkDestroyRenderPass(device,mainPass,nullptr);
            if(depthPass&&device)vkDestroyRenderPass(device,depthPass,nullptr);
            if(mainVert&&device)vkDestroyShaderModule(device,mainVert,nullptr);
            if(mainFrag&&device)vkDestroyShaderModule(device,mainFrag,nullptr);
            if(sunDepthVert&&device)vkDestroyShaderModule(device,sunDepthVert,nullptr);
            if(meshBuffer&&device)vkDestroyBuffer(device,meshBuffer,nullptr);
            if(meshMemory&&device)vkFreeMemory(device,meshMemory,nullptr);
            if(pipelineCache&&device)vkDestroyPipelineCache(device,pipelineCache,nullptr);
            if(device)vkDestroyDevice(device,nullptr);
            if(instance)vkDestroyInstance(instance,nullptr);
            throw;
        }
    }
    ~PersistentVkContext(){
        if(device){
            vkDeviceWaitIdle(device);
            if(mainPipeline)vkDestroyPipeline(device,mainPipeline,nullptr);
            if(sunPipeline)vkDestroyPipeline(device,sunPipeline,nullptr);
            if(mainLayout)vkDestroyPipelineLayout(device,mainLayout,nullptr);
            if(sunLayout)vkDestroyPipelineLayout(device,sunLayout,nullptr);
            if(descriptorLayout)vkDestroyDescriptorSetLayout(device,descriptorLayout,nullptr);
            if(mainPass)vkDestroyRenderPass(device,mainPass,nullptr);
            if(depthPass)vkDestroyRenderPass(device,depthPass,nullptr);
            if(mainVert)vkDestroyShaderModule(device,mainVert,nullptr);
            if(mainFrag)vkDestroyShaderModule(device,mainFrag,nullptr);
            if(sunDepthVert)vkDestroyShaderModule(device,sunDepthVert,nullptr);
            if(meshBuffer)vkDestroyBuffer(device,meshBuffer,nullptr);
            if(meshMemory)vkFreeMemory(device,meshMemory,nullptr);
            if(pipelineCache)vkDestroyPipelineCache(device,pipelineCache,nullptr);
            vkDestroyDevice(device,nullptr);
        }
        if(instance)vkDestroyInstance(instance,nullptr);
    }
    PersistentVkContext(const PersistentVkContext&)=delete;
    PersistentVkContext& operator=(const PersistentVkContext&)=delete;
};
}
static int gpuViewportImpl(
    uint32_t w,uint32_t h,uint8_t* pixels,uint64_t capacity,
    const float* positions,uint32_t vertexCount,const uint32_t* indices,uint32_t indexCount,
    const float* mvp,const float* lights,uint32_t lightCount,const float* faceNormals,
    char* error,uint32_t errorCapacity,uint32_t requestedShadowSize,float shadowSoftness,float shadowBias) {
    auto fail=[&](const char* why) {
        if(error&&errorCapacity){
            std::strncpy(error,why,errorCapacity-1);
            error[errorCapacity-1]=0;
        }
        return 0;
    };
    if(!pixels||!positions||!indices||!mvp||!faceNormals||
       w==0||h==0||w>4096||h>4096||vertexCount==0||
       indexCount==0||indexCount%3||vertexCount>1000000||indexCount>3000000||
       capacity<uint64_t(w)*h*4||(lightCount&&!lights)||lightCount>32)
        return fail("Invalid Vulkan viewport parameters");
    // Skip full Vulkan setup for identical redraws. The bridge invokes this
    // function continuously even if neither mesh, camera nor light changed.
    static thread_local uint64_t cachedKey=0;
    static thread_local uint32_t cachedW=0,cachedH=0;
    static thread_local std::vector<uint8_t> cachedImage;
    uint64_t key=14695981039346656037ull;
    auto mix=[&](const void* ptr,size_t n){
        const auto* b=static_cast<const unsigned char*>(ptr);
        for(size_t i=0;i<n;++i){key^=b[i];key*=1099511628211ull;}
    };
    // Geometry identity excludes render target size and shadow/camera settings.
    mix(&vertexCount,sizeof(vertexCount));mix(&indexCount,sizeof(indexCount));
    mix(positions,size_t(vertexCount)*3*sizeof(float));
    mix(indices,size_t(indexCount)*sizeof(uint32_t));
    mix(faceNormals,size_t(indexCount)*sizeof(float));
    const uint64_t currentMeshKey=key;
    mix(&w,sizeof(w));mix(&h,sizeof(h));
    mix(&requestedShadowSize,sizeof(requestedShadowSize));
    mix(&shadowSoftness,sizeof(shadowSoftness));mix(&shadowBias,sizeof(shadowBias));
    mix(mvp,16*sizeof(float));mix(&lightCount,sizeof(lightCount));
    if(lightCount)mix(lights,size_t(lightCount)*12*sizeof(float));
    if(key==cachedKey&&cachedW==w&&cachedH==h&&cachedImage.size()==size_t(w)*h*4){
        std::memcpy(pixels,cachedImage.data(),cachedImage.size());
        return 1;
    }
    VkDevice device=VK_NULL_HANDLE;
    VkCommandPool pool=VK_NULL_HANDLE;
    VkRenderPass pass=VK_NULL_HANDLE,shadowPass=VK_NULL_HANDLE;
    VkPipelineLayout layout=VK_NULL_HANDLE,shadowLayout=VK_NULL_HANDLE;
    VkPipeline pipeline=VK_NULL_HANDLE,shadowPipeline=VK_NULL_HANDLE;
    VkShaderModule vert=VK_NULL_HANDLE,frag=VK_NULL_HANDLE,shadowVert=VK_NULL_HANDLE;
    VkImage color=VK_NULL_HANDLE,depth=VK_NULL_HANDLE,shadowImage=VK_NULL_HANDLE;
    VkDeviceMemory colorMemory=VK_NULL_HANDLE,depthMemory=VK_NULL_HANDLE,shadowMemory=VK_NULL_HANDLE;
    VkImageView colorView=VK_NULL_HANDLE,depthView=VK_NULL_HANDLE,shadowView=VK_NULL_HANDLE;
    VkFramebuffer framebuffer=VK_NULL_HANDLE,shadowFramebuffer=VK_NULL_HANDLE;
    VkSampler shadowSampler=VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout=VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool=VK_NULL_HANDLE;
    VkBuffer lightUniform=VK_NULL_HANDLE;
    VkDeviceMemory lightUniformMemory=VK_NULL_HANDLE;
    VkBuffer shadowUniform=VK_NULL_HANDLE;
    VkDeviceMemory shadowUniformMemory=VK_NULL_HANDLE;
    VkBuffer vertices=VK_NULL_HANDLE,readback=VK_NULL_HANDLE;
    VkDeviceMemory readbackMemory=VK_NULL_HANDLE;
    auto cleanup=[&]{
        if(device){
            vkDeviceWaitIdle(device);
            if(framebuffer)vkDestroyFramebuffer(device,framebuffer,nullptr);
            if(shadowFramebuffer)vkDestroyFramebuffer(device,shadowFramebuffer,nullptr);
            /* persistent pipeline resource */
            /* persistent pipeline resource */
            // Depth render pass retained by PersistentVkContext.

            if(shadowSampler)vkDestroySampler(device,shadowSampler,nullptr);
            if(descriptorPool)vkDestroyDescriptorPool(device,descriptorPool,nullptr);
            /* persistent pipeline resource */
            if(lightUniform)vkDestroyBuffer(device,lightUniform,nullptr);
            if(lightUniformMemory)vkFreeMemory(device,lightUniformMemory,nullptr);
            if(shadowUniform)vkDestroyBuffer(device,shadowUniform,nullptr);
            if(shadowUniformMemory)vkFreeMemory(device,shadowUniformMemory,nullptr);
            if(shadowView)vkDestroyImageView(device,shadowView,nullptr);
            if(shadowImage)vkDestroyImage(device,shadowImage,nullptr);
            if(shadowMemory)vkFreeMemory(device,shadowMemory,nullptr);
            /* persistent pipeline resource */
            /* persistent pipeline resource */
            // Main render pass retained by PersistentVkContext.
            // Persistent shader modules are owned by PersistentVkContext.
            if(colorView)vkDestroyImageView(device,colorView,nullptr);
            if(depthView)vkDestroyImageView(device,depthView,nullptr);
            if(color)vkDestroyImage(device,color,nullptr);
            if(depth)vkDestroyImage(device,depth,nullptr);
            if(colorMemory)vkFreeMemory(device,colorMemory,nullptr);
            if(depthMemory)vkFreeMemory(device,depthMemory,nullptr);
            if(readback)vkDestroyBuffer(device,readback,nullptr);
            if(readbackMemory)vkFreeMemory(device,readbackMemory,nullptr);
            if(pool)vkDestroyCommandPool(device,pool,nullptr);
        }
    };
    try {
        static thread_local PersistentVkContext persistent;
        device=persistent.device;
        VkPhysicalDevice gpu=persistent.gpu;
        VkQueue queue=persistent.queue;
        uint32_t family=persistent.family;
        auto createBuffer=[&](VkDeviceSize size,VkBufferUsageFlags usage,VkBuffer& handle,VkDeviceMemory& memory){
            VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bi.size=size;bi.usage=usage;bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
            require(vkCreateBuffer(device,&bi,nullptr,&handle),"Create buffer failed");
            VkMemoryRequirements req{};vkGetBufferMemoryRequirements(device,handle,&req);
            VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            alloc.allocationSize=req.size;
            alloc.memoryTypeIndex=memoryType(gpu,req.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            require(vkAllocateMemory(device,&alloc,nullptr,&memory),"Allocate buffer failed");
            require(vkBindBufferMemory(device,handle,memory,0),"Bind buffer failed");
        };
        auto createImage=[&](VkFormat format,VkImageUsageFlags usage,VkImage& handle,VkDeviceMemory& memory,VkImageView& view,VkImageAspectFlags aspect,uint32_t iw,uint32_t ih){
            VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            ci.imageType=VK_IMAGE_TYPE_2D;ci.format=format;ci.extent={iw,ih,1};
            ci.mipLevels=1;ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;
            ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=usage;
            ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;ci.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
            require(vkCreateImage(device,&ci,nullptr,&handle),"Create render image failed");
            VkMemoryRequirements req{};vkGetImageMemoryRequirements(device,handle,&req);
            VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            alloc.allocationSize=req.size;alloc.memoryTypeIndex=memoryType(gpu,req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            require(vkAllocateMemory(device,&alloc,nullptr,&memory),"Allocate render image failed");
            require(vkBindImageMemory(device,handle,memory,0),"Bind image failed");
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            vi.image=handle;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=format;
            vi.subresourceRange.aspectMask=aspect;vi.subresourceRange.levelCount=1;
            vi.subresourceRange.layerCount=1;
            require(vkCreateImageView(device,&vi,nullptr,&view),"Create image view failed");
        };
        createImage(VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            color,colorMemory,colorView,VK_IMAGE_ASPECT_COLOR_BIT,w,h);
        createImage(VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            depth,depthMemory,depthView,VK_IMAGE_ASPECT_DEPTH_BIT,w,h);
        // For a Point source render six 90-degree faces into one 3x2 atlas.
        int pointCandidate=-1;
        bool anySunShadow=false,anySpotShadow=false,anyAreaShadow=false;
        for(uint32_t li=0;li<lightCount;++li){
            const float* l=lights+12*li;
            if(l[10]<=0.f || l[11]<0.f)continue;
            int kind=static_cast<int>(l[0]+0.5f);
            if(kind==0)anySunShadow=true;
            if(kind==2)anySpotShadow=true;
            if(kind==3)anyAreaShadow=true;
            if(kind==1 && pointCandidate<0)pointCandidate=static_cast<int>(li);
        }
        const bool usePointAtlas=pointCandidate>=0 &&
            !anySunShadow && !anySpotShadow && !anyAreaShadow;
        const uint32_t shadowSize=std::clamp(requestedShadowSize,64u,
                                            usePointAtlas?1024u:4096u);
        const uint32_t shadowWidth=usePointAtlas?shadowSize*3:shadowSize;
        const uint32_t shadowHeight=usePointAtlas?shadowSize*2:shadowSize;
        VkFormatProperties shadowFormat{};
        vkGetPhysicalDeviceFormatProperties(gpu,VK_FORMAT_D32_SFLOAT,&shadowFormat);
        if(!(shadowFormat.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
            throw std::runtime_error("D32 shadow sampling unsupported");
        createImage(VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
            shadowImage,shadowMemory,shadowView,VK_IMAGE_ASPECT_DEPTH_BIT,shadowWidth,shadowHeight);
        // One shadow map: prefer enabled Sun, otherwise use an enabled Area.
        // Area uses an orthographic approximation along its emission direction.
        // A future implementation will use sampled emitters for true penumbrae.
        int shadowLightIndex=-1;
        float sunDir[3]={0.f,0.f,-1.f};
        if(lights){
            for(uint32_t li=0;li<lightCount;++li){
                const float* l=lights+12*li;
                if(l[0]<0.5f && l[10]>0.f && l[11]>=0.f){
                    shadowLightIndex=static_cast<int>(li);
                    break;
                }
            }
            // Single-map fallback priority: Spot, Area, then Point.
            // Point is initially a scene-facing directional approximation;
            // complete omnidirectional shadows require six cube faces.
            if(shadowLightIndex<0)for(int kind=2;kind<=3;++kind){
                for(uint32_t li=0;li<lightCount;++li){
                    const float* l=lights+12*li;
                    if(l[0]>=kind-0.5f && l[0]<kind+0.5f &&
                       l[10]>0.f && l[11]>=0.f){
                        shadowLightIndex=static_cast<int>(li);break;
                    }
                }
                if(shadowLightIndex>=0)break;
            }
            if(shadowLightIndex<0)for(uint32_t li=0;li<lightCount;++li){
                const float* l=lights+12*li;
                if(l[0]>=0.5f && l[0]<1.5f && l[10]>0.f && l[11]>=0.f){
                    shadowLightIndex=static_cast<int>(li);break;
                }
            }
            if(shadowLightIndex>=0){
                const float* chosen=lights+12*shadowLightIndex;
                if(chosen[0]>=0.5f && chosen[0]<1.5f){
                    // Aim the point shadow frustum at the scene bounding box,
                    // not the vertex average (which is tessellation-dependent).
                    float lo[3]={1e30f,1e30f,1e30f};
                    float hi[3]={-1e30f,-1e30f,-1e30f};
                    for(uint32_t i=0;i<vertexCount;++i)for(int j=0;j<3;++j){
                        lo[j]=std::min(lo[j],positions[3*i+j]);
                        hi[j]=std::max(hi[j],positions[3*i+j]);
                    }
                    for(int j=0;j<3;++j)sunDir[j]=0.5f*(lo[j]+hi[j])-chosen[1+j];
                }else{
                    std::memcpy(sunDir,chosen+4,sizeof(sunDir));
                }
            }
        }
        auto normalize=[](std::array<float,3> a){
            const float len=std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);
            if(len<1e-6f)return std::array<float,3>{0.f,0.f,-1.f};
            for(auto& v:a)v/=len;return a;
        };
        auto cross=[](const std::array<float,3>& a,const std::array<float,3>& b){
            return std::array<float,3>{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
        };
        auto dot=[](const std::array<float,3>& a,const std::array<float,3>& b){
            return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
        };
        auto d=normalize(std::array<float,3>{sunDir[0],sunDir[1],sunDir[2]});
        auto up=std::fabs(d[2])>0.95f
            ? std::array<float,3>{0.f,1.f,0.f}
            : std::array<float,3>{0.f,0.f,1.f};
        auto u=normalize(cross(up,d));
        auto v=cross(d,u);
        float minima[3]={1e30f,1e30f,1e30f},maxima[3]={-1e30f,-1e30f,-1e30f};
        for(uint32_t i=0;i<vertexCount;++i){
            std::array<float,3> p{positions[3*i],positions[3*i+1],positions[3*i+2]};
            float coords[3]={dot(u,p),dot(v,p),dot(d,p)};
            for(int j=0;j<3;++j){
                minima[j]=std::min(minima[j],coords[j]);
                maxima[j]=std::max(maxima[j],coords[j]);
            }
        }
        float lightMatrix[16]={};
        const bool pointShadow=shadowLightIndex>=0 &&
            lights[12*shadowLightIndex]>=0.5f && lights[12*shadowLightIndex]<1.5f;
        const bool spotShadow=shadowLightIndex>=0 &&
            lights[12*shadowLightIndex]>=1.5f && lights[12*shadowLightIndex]<2.5f;
        const bool areaShadow=shadowLightIndex>=0 &&
            lights[12*shadowLightIndex]>=2.5f && lights[12*shadowLightIndex]<3.5f;
        if(areaShadow){
            // A finite Area emitter is not a directional Sun. Use its world
            // position as the projection origin and its local -Z as the axis.
            // Fit the perspective frustum to scene geometry in front of it.
            // Keep the existing PCF softness unchanged.
            const float* source=lights+12*shadowLightIndex;
            const std::array<float,3> origin{source[1],source[2],source[3]};
            float farPlane=0.1f;
            float maxSlope=0.0f;
            bool hasForwardGeometry=false;
            for(uint32_t i=0;i<vertexCount;++i){
                const std::array<float,3> p{positions[3*i]-origin[0],
                                            positions[3*i+1]-origin[1],
                                            positions[3*i+2]-origin[2]};
                const float z=dot(d,p);
                if(z<=0.005f)continue;
                hasForwardGeometry=true;
                farPlane=std::max(farPlane,std::sqrt(dot(p,p)));
                maxSlope=std::max(maxSlope,
                    std::max(std::fabs(dot(u,p)),std::fabs(dot(v,p)))/z);
            }
            if(hasForwardGeometry){
                // Avoid a near-180-degree frustum from geometry grazing the
                // emitter plane. Those locations are outside usable light.
                const float xyScale=1.f/std::clamp(maxSlope*1.05f,0.15f,10.f);
                farPlane=farPlane*1.1f+0.01f;
                const float nearPlane=std::min(0.05f,farPlane*0.01f);
                const float zScale=farPlane/(farPlane-nearPlane);
                for(int j=0;j<3;++j){
                    lightMatrix[j*4+0]=u[j]*xyScale;
                    lightMatrix[j*4+1]=v[j]*xyScale;
                    lightMatrix[j*4+2]=d[j]*zScale;
                    lightMatrix[j*4+3]=d[j];
                }
                lightMatrix[12]=-dot(u,origin)*xyScale;
                lightMatrix[13]=-dot(v,origin)*xyScale;
                lightMatrix[14]=-dot(d,origin)*zScale-nearPlane*zScale;
                lightMatrix[15]=-dot(d,origin);
            }
        }else if(spotShadow){
            // Perspective projection must originate at the Spot emitter and
            // match the exact lighting cone. An orthographic Sun projection
            // clips nearby occluders and produces impossible receiver shadows.
            const float* source=lights+12*shadowLightIndex;
            const std::array<float,3> origin{source[1],source[2],source[3]};
            const float angle=source[11]>1.f
                ? std::clamp(source[11]-1.f,0.05f,3.10f)
                : 1.0681415f; // old ABI fallback: ~61.2 degrees
            const float xyScale=1.f/std::tan(angle*0.5f);
            float farPlane=0.1f;
            for(uint32_t i=0;i<vertexCount;++i){
                const std::array<float,3> p{positions[3*i]-origin[0],
                                            positions[3*i+1]-origin[1],
                                            positions[3*i+2]-origin[2]};
                const float distance=std::sqrt(dot(p,p));
                farPlane=std::max(farPlane,distance);
            }
            farPlane=farPlane*1.10f+0.01f;
            const float nearPlane=std::min(0.05f,farPlane*0.01f);
            const float zScale=farPlane/(farPlane-nearPlane);
            for(int j=0;j<3;++j){
                lightMatrix[j*4+0]=u[j]*xyScale;
                lightMatrix[j*4+1]=v[j]*xyScale;
                lightMatrix[j*4+2]=d[j]*zScale;
                lightMatrix[j*4+3]=d[j];
            }
            lightMatrix[12]=-dot(u,origin)*xyScale;
            lightMatrix[13]=-dot(v,origin)*xyScale;
            lightMatrix[14]=-dot(d,origin)*zScale-nearPlane*zScale;
            lightMatrix[15]=-dot(d,origin);
        }else if(pointShadow){
            // Perspective depth projection from the Point emitter: unlike an
            // orthographic Sun map, objects nearer the emitter cast correctly
            // expanding shadows onto receivers behind them.
            const float* emitter=lights+12*shadowLightIndex+1;
            float nearest=1e30f,farthest=0.f,maxSlope=0.f;
            for(uint32_t i=0;i<vertexCount;++i){
                const std::array<float,3> p{
                    positions[3*i]-emitter[0],
                    positions[3*i+1]-emitter[1],
                    positions[3*i+2]-emitter[2]};
                const float z=dot(d,p);
                if(z<=0.001f)continue;
                nearest=std::min(nearest,z);
                farthest=std::max(farthest,z);
                maxSlope=std::max(maxSlope,std::max(std::fabs(dot(u,p)),std::fabs(dot(v,p)))/z);
            }
            if(farthest>0.f){
                const float nearPlane=std::max(0.005f,std::min(nearest*0.5f,farthest*0.1f));
                const float farPlane=std::max(nearPlane+0.1f,farthest*1.05f);
                const float scale=1.f/std::max(0.1f,maxSlope*1.1f);
                for(int j=0;j<3;++j){
                    lightMatrix[j*4+0]=u[j]*scale;
                    lightMatrix[j*4+1]=v[j]*scale;
                    lightMatrix[j*4+2]=d[j]*farPlane/(farPlane-nearPlane);
                    lightMatrix[j*4+3]=d[j];
                }
                lightMatrix[12]=-dot(u,std::array<float,3>{emitter[0],emitter[1],emitter[2]})*scale;
                lightMatrix[13]=-dot(v,std::array<float,3>{emitter[0],emitter[1],emitter[2]})*scale;
                lightMatrix[14]=-dot(d,std::array<float,3>{emitter[0],emitter[1],emitter[2]})*
                    farPlane/(farPlane-nearPlane)-nearPlane*farPlane/(farPlane-nearPlane);
                lightMatrix[15]=-dot(d,std::array<float,3>{emitter[0],emitter[1],emitter[2]});
            }
        }else{
            const std::array<float,3> axes[3]={u,v,d};
            for(int row=0;row<3;++row){
                const float range=std::max(maxima[row]-minima[row],0.1f)*1.06f;
                const float scale=row==2?1.f/range:2.f/range;
                for(int col=0;col<3;++col)lightMatrix[col*4+row]=axes[row][col]*scale;
                lightMatrix[12+row]=row==2
                    ? -(minima[row]-0.03f*range)*scale
                    : -(minima[row]+maxima[row])*0.5f*scale;
            }
            lightMatrix[15]=1.f;
        }
        struct ShadowUniform {
            float matrix[16];
            float settings[4];
            float faces[6][16];
            float pointPosition[4];
        } shadowData{};
        if(usePointAtlas && shadowLightIndex>=0){
            const float* l=lights+12*shadowLightIndex;
            const std::array<float,3> emitter{l[1],l[2],l[3]};
            shadowData.pointPosition[0]=l[1];
            shadowData.pointPosition[1]=l[2];
            shadowData.pointPosition[2]=l[3];
            shadowData.pointPosition[3]=1.f;
            const std::array<std::array<float,3>,6> directions{{
                {{1.f,0.f,0.f}},{{-1.f,0.f,0.f}},
                {{0.f,1.f,0.f}},{{0.f,-1.f,0.f}},
                {{0.f,0.f,1.f}},{{0.f,0.f,-1.f}}
            }};
            float farPlane=0.1f;
            for(uint32_t i=0;i<vertexCount;++i){
                const float dx=positions[3*i]-l[1],dy=positions[3*i+1]-l[2],
                            dz=positions[3*i+2]-l[3];
                farPlane=std::max(farPlane,std::sqrt(dx*dx+dy*dy+dz*dz));
            }
            farPlane=farPlane*1.10f+0.01f;
            const float nearPlane=std::min(0.05f,farPlane*0.01f);
            const float zScale=farPlane/(farPlane-nearPlane);
            for(int face=0;face<6;++face){
                const auto fd=directions[face];
                const auto refUp=std::fabs(fd[2])>0.95f
                    ? std::array<float,3>{0.f,1.f,0.f}
                    : std::array<float,3>{0.f,0.f,1.f};
                const auto fu=normalize(cross(refUp,fd));
                const auto fv=cross(fd,fu);
                float* m=shadowData.faces[face];
                for(int j=0;j<3;++j){
                    m[j*4]=fu[j];
                    m[j*4+1]=fv[j];
                    m[j*4+2]=fd[j]*zScale;
                    m[j*4+3]=fd[j];
                }
                m[12]=-dot(fu,emitter);
                m[13]=-dot(fv,emitter);
                m[14]=-dot(fd,emitter)*zScale-nearPlane*zScale;
                m[15]=-dot(fd,emitter);
            }
        }
        std::memcpy(shadowData.matrix,lightMatrix,sizeof(lightMatrix));
        shadowData.settings[0]=shadowLightIndex>=0?1.f:0.f;
        shadowData.settings[3]=static_cast<float>(shadowLightIndex+1);
        shadowData.settings[1]=std::max(0.00001f,shadowBias*0.0005f);
        shadowData.settings[2]=std::max(0.f,shadowSoftness);
        // Optional shadow diagnostics: set HAMUN_SHADOW_DEBUG=1 to visualize
        // selected atlas faces and unclamped light-space coordinates.
        char shadowDebug[8]={};
        if(GetEnvironmentVariableA("HAMUN_SHADOW_DEBUG",shadowDebug,sizeof(shadowDebug))>0)
            shadowData.pointPosition[3]=2.f;
        createBuffer(sizeof(shadowData),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     shadowUniform,shadowUniformMemory);
        void* shadowMapped=nullptr;
        require(vkMapMemory(device,shadowUniformMemory,0,sizeof(shadowData),0,&shadowMapped),
                "Map shadow uniforms failed");
        std::memcpy(shadowMapped,&shadowData,sizeof(shadowData));
        vkUnmapMemory(device,shadowUniformMemory);
        if(persistent.meshBuffer==VK_NULL_HANDLE || persistent.meshKey!=currentMeshKey){
            if(persistent.meshBuffer){
                vkDestroyBuffer(device,persistent.meshBuffer,nullptr);
                persistent.meshBuffer=VK_NULL_HANDLE;
            }
            if(persistent.meshMemory){
                vkFreeMemory(device,persistent.meshMemory,nullptr);
                persistent.meshMemory=VK_NULL_HANDLE;
            }
            persistent.meshKey=0;
            std::vector<Vertex> mesh;
            mesh.reserve(indexCount);
            for(uint32_t i=0;i<indexCount;i+=3){
                for(uint32_t k=0;k<3;++k){
                    uint32_t index=indices[i+k];
                    if(index>=vertexCount)throw std::runtime_error("Mesh index out of bounds");
                    Vertex v{};
                    std::memcpy(v.p,positions+3*index,sizeof(v.p));
                    std::memcpy(v.n,faceNormals+i,sizeof(v.n));
                    mesh.push_back(v);
                }
            }
            createBuffer(mesh.size()*sizeof(Vertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,persistent.meshBuffer,persistent.meshMemory);
            void* uploadMapped=nullptr;
            require(vkMapMemory(device,persistent.meshMemory,0,VK_WHOLE_SIZE,0,&uploadMapped),"Map vertices failed");
            std::memcpy(uploadMapped,mesh.data(),mesh.size()*sizeof(Vertex));
            vkUnmapMemory(device,persistent.meshMemory);
            persistent.meshKey=currentMeshKey;
        }
        vertices=persistent.meshBuffer;
        void* mapped=nullptr;
        createBuffer(VkDeviceSize(w)*h*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT,readback,readbackMemory);
        VkAttachmentDescription shadowAttachment{};
        shadowAttachment.format=VK_FORMAT_D32_SFLOAT;
        shadowAttachment.samples=VK_SAMPLE_COUNT_1_BIT;
        shadowAttachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        shadowAttachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        shadowAttachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        shadowAttachment.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        VkAttachmentReference shadowRef{0,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription shadowSubpass{};
        shadowSubpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
        shadowSubpass.pDepthStencilAttachment=&shadowRef;
        VkSubpassDependency shadowDependency{};
        shadowDependency.srcSubpass=0;shadowDependency.dstSubpass=VK_SUBPASS_EXTERNAL;
        shadowDependency.srcStageMask=VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        shadowDependency.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        shadowDependency.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        shadowDependency.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        VkRenderPassCreateInfo shadowPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        shadowPassInfo.attachmentCount=1;shadowPassInfo.pAttachments=&shadowAttachment;
        shadowPassInfo.subpassCount=1;shadowPassInfo.pSubpasses=&shadowSubpass;
        shadowPassInfo.dependencyCount=1;shadowPassInfo.pDependencies=&shadowDependency;
        if(!persistent.depthPass)
            require(vkCreateRenderPass(device,&shadowPassInfo,nullptr,&persistent.depthPass),"Create Sun shadow pass failed");
        shadowPass=persistent.depthPass;
        VkFramebufferCreateInfo shadowFb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        shadowFb.renderPass=shadowPass;shadowFb.attachmentCount=1;
        shadowFb.pAttachments=&shadowView;shadowFb.width=shadowWidth;
        shadowFb.height=shadowHeight;shadowFb.layers=1;
        require(vkCreateFramebuffer(device,&shadowFb,nullptr,&shadowFramebuffer),"Create Sun depth framebuffer failed");
        VkSamplerCreateInfo shadowSamplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        shadowSamplerInfo.magFilter=VK_FILTER_NEAREST;
        shadowSamplerInfo.minFilter=VK_FILTER_NEAREST;
        shadowSamplerInfo.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        shadowSamplerInfo.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        shadowSamplerInfo.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        shadowSamplerInfo.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        shadowSamplerInfo.compareEnable=VK_TRUE;
        shadowSamplerInfo.compareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
        shadowSamplerInfo.minLod=0;shadowSamplerInfo.maxLod=0;
        require(vkCreateSampler(device,&shadowSamplerInfo,nullptr,&shadowSampler),"Create shadow sampler failed");
        VkDescriptorSetLayoutBinding bindings[3]{};
        bindings[0].binding=0;bindings[0].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount=1;bindings[0].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].binding=1;bindings[1].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[1].descriptorCount=1;bindings[1].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[2].binding=2;bindings[2].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[2].descriptorCount=1;bindings[2].stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo dl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dl.bindingCount=3;dl.pBindings=bindings;
        if(!persistent.descriptorLayout)
            require(vkCreateDescriptorSetLayout(device,&dl,nullptr,&persistent.descriptorLayout),"Create shadow descriptor layout failed");
        setLayout=persistent.descriptorLayout;
        VkDescriptorPoolSize poolSizes[2]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},
                                           {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,2}};
        VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        dp.maxSets=1;dp.poolSizeCount=2;dp.pPoolSizes=poolSizes;
        require(vkCreateDescriptorPool(device,&dp,nullptr,&descriptorPool),"Create shadow descriptor pool failed");
        VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        da.descriptorPool=descriptorPool;da.descriptorSetCount=1;da.pSetLayouts=&setLayout;
        VkDescriptorSet descriptorSet=VK_NULL_HANDLE;
        require(vkAllocateDescriptorSets(device,&da,&descriptorSet),"Allocate shadow descriptor failed");
        VkDescriptorImageInfo depthBinding{};
        depthBinding.sampler=shadowSampler;depthBinding.imageView=shadowView;
        depthBinding.imageLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        VkDescriptorBufferInfo uniformBinding{shadowUniform,0,sizeof(shadowData)};
        // std140 layout: four vec4 per light; supports all 32 ABI lights.
        struct GpuLight {float positionType[4],directionRange[4],colorEnergy[4],settings[4];};
        struct LightBlock {GpuLight entries[32];int count;int padding[3];} lightBlock{};
        lightBlock.count=static_cast<int>(lightCount);
        for(uint32_t i=0;i<lightCount;++i){
            const float* l=lights+12*i;
            auto& o=lightBlock.entries[i];
            o.positionType[0]=l[1];o.positionType[1]=l[2];o.positionType[2]=l[3];o.positionType[3]=l[0];
            o.directionRange[0]=l[4];o.directionRange[1]=l[5];o.directionRange[2]=l[6];
            o.colorEnergy[0]=l[7];o.colorEnergy[1]=l[8];o.colorEnergy[2]=l[9];o.colorEnergy[3]=std::max(0.f,l[10]);
            o.settings[0]=l[11];
            o.settings[1]=l[0]>=1.5f && l[0]<2.5f && l[11]>1.f
                ?std::clamp(l[11]-1.f,0.05f,3.10f):1.0681415f;
            // Point radius / Area half-size, encoded by the Blender bridge.
            // Keep disabled lights at zero; Sun and Spot use other parameters.
            o.settings[2]=(l[0]>=0.5f && l[0]<1.5f) ||
                          (l[0]>=2.5f && l[0]<3.5f)
                          ?std::max(0.f,l[11]-1.f):0.f;
        }
        createBuffer(sizeof(lightBlock),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,lightUniform,lightUniformMemory);
        void* lightMapped=nullptr;
        require(vkMapMemory(device,lightUniformMemory,0,sizeof(lightBlock),0,&lightMapped),"Map light uniforms failed");
        std::memcpy(lightMapped,&lightBlock,sizeof(lightBlock));
        vkUnmapMemory(device,lightUniformMemory);
        VkDescriptorBufferInfo lightsBinding{lightUniform,0,sizeof(lightBlock)};
        VkWriteDescriptorSet writes[3]{};
        for(auto& write:writes)write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet=descriptorSet;writes[0].dstBinding=0;
        writes[0].descriptorCount=1;writes[0].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo=&depthBinding;
        writes[1].dstSet=descriptorSet;writes[1].dstBinding=1;
        writes[1].descriptorCount=1;writes[1].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[1].pBufferInfo=&uniformBinding;
        writes[2].dstSet=descriptorSet;writes[2].dstBinding=2;
        writes[2].descriptorCount=1;writes[2].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[2].pBufferInfo=&lightsBinding;
        vkUpdateDescriptorSets(device,3,writes,0,nullptr);
        VkAttachmentDescription attachments[2]{};
        attachments[0].format=VK_FORMAT_R8G8B8A8_UNORM;
        attachments[0].samples=VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        attachments[1].format=VK_FORMAT_D32_SFLOAT;
        attachments[1].samples=VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference cr{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference dr{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};
        sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount=1;sub.pColorAttachments=&cr;sub.pDepthStencilAttachment=&dr;
        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rp.attachmentCount=2;rp.pAttachments=attachments;rp.subpassCount=1;rp.pSubpasses=&sub;
        if(!persistent.mainPass)
            require(vkCreateRenderPass(device,&rp,nullptr,&persistent.mainPass),"Create render pass failed");
        pass=persistent.mainPass;
        auto module=[&](const wchar_t* filename){
            auto bytes=binary(filename);
            VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            ci.codeSize=bytes.size();ci.pCode=reinterpret_cast<const uint32_t*>(bytes.data());
            VkShaderModule sm{};
            require(vkCreateShaderModule(device,&ci,nullptr,&sm),"Create shader module failed");
            return sm;
        };
        if(!persistent.mainVert)persistent.mainVert=module(L"viewport_shadowed.vert.spv");
        if(!persistent.mainFrag)persistent.mainFrag=module(L"viewport_shadowed.frag.spv");
        if(!persistent.sunDepthVert)persistent.sunDepthVert=module(L"viewport_sun_depth.vert.spv");
        vert=persistent.mainVert;frag=persistent.mainFrag;shadowVert=persistent.sunDepthVert;
        VkPipelineShaderStageCreateInfo stages[2]{};
        for(auto& x:stages){x.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;x.pName="main";}
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vert;
        stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=frag;
        VkVertexInputBindingDescription binding{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attrs[2]{
            {0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,p)},
            {1,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,n)}
        };
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vi.vertexBindingDescriptionCount=1;vi.pVertexBindingDescriptions=&binding;
        vi.vertexAttributeDescriptionCount=2;vi.pVertexAttributeDescriptions=attrs;
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport{0,0,float(w),float(h),0,1};
        VkRect2D scissor{{0,0},{w,h}};
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vp.viewportCount=1;vp.pViewports=&viewport;vp.scissorCount=1;vp.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode=VK_POLYGON_MODE_FILL;rs.cullMode=VK_CULL_MODE_NONE;
        rs.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;rs.lineWidth=1.f;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask=0xf;
        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cb.attachmentCount=1;cb.pAttachments=&blendAttachment;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        ds.depthTestEnable=VK_TRUE;ds.depthWriteEnable=VK_TRUE;ds.depthCompareOp=VK_COMPARE_OP_LESS;
        VkPushConstantRange pr{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(Params)};
        VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pli.pushConstantRangeCount=1;pli.pPushConstantRanges=&pr;
        pli.setLayoutCount=1;pli.pSetLayouts=&setLayout;
        if(!persistent.mainLayout)
            require(vkCreatePipelineLayout(device,&pli,nullptr,&persistent.mainLayout),"Create pipeline layout failed");
        layout=persistent.mainLayout;
        VkGraphicsPipelineCreateInfo pipe{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipe.stageCount=2;pipe.pStages=stages;pipe.pVertexInputState=&vi;
        pipe.pInputAssemblyState=&ia;pipe.pViewportState=&vp;
        pipe.pRasterizationState=&rs;pipe.pMultisampleState=&ms;
        pipe.pColorBlendState=&cb;pipe.pDepthStencilState=&ds;
        pipe.layout=layout;pipe.renderPass=pass;
        const bool pipelineResize=(persistent.pipelineW!=w || persistent.pipelineH!=h ||
                                   persistent.pipelineShadowSize!=shadowSize);
        if(pipelineResize){
            if(persistent.mainPipeline){vkDestroyPipeline(device,persistent.mainPipeline,nullptr);persistent.mainPipeline=VK_NULL_HANDLE;}
            if(persistent.sunPipeline){vkDestroyPipeline(device,persistent.sunPipeline,nullptr);persistent.sunPipeline=VK_NULL_HANDLE;}
            persistent.pipelineW=0; persistent.pipelineH=0; persistent.pipelineShadowSize=0;
        }
        if(!persistent.mainPipeline)
            require(vkCreateGraphicsPipelines(device,persistent.pipelineCache,1,&pipe,nullptr,&persistent.mainPipeline),"Create graphics pipeline failed");
        pipeline=persistent.mainPipeline;
        VkPushConstantRange shadowPush{VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(lightMatrix)};
        VkPipelineLayoutCreateInfo shadowPli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        shadowPli.pushConstantRangeCount=1;shadowPli.pPushConstantRanges=&shadowPush;
        if(!persistent.sunLayout)
            require(vkCreatePipelineLayout(device,&shadowPli,nullptr,&persistent.sunLayout),
                    "Create Sun shadow pipeline layout failed");
        shadowLayout=persistent.sunLayout;
        VkPipelineShaderStageCreateInfo depthStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        depthStage.stage=VK_SHADER_STAGE_VERTEX_BIT;
        depthStage.module=shadowVert;depthStage.pName="main";
        VkViewport shadowViewport{0,0,float(shadowSize),float(shadowSize),0,1};
        VkRect2D shadowScissor{{0,0},{shadowSize,shadowSize}};
        VkPipelineViewportStateCreateInfo shadowVp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        shadowVp.viewportCount=1;shadowVp.pViewports=&shadowViewport;
        shadowVp.scissorCount=1;shadowVp.pScissors=&shadowScissor;
        VkPipelineColorBlendStateCreateInfo shadowBlend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        VkDynamicState shadowStates[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo shadowDynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        shadowDynamic.dynamicStateCount=2;shadowDynamic.pDynamicStates=shadowStates;
        VkGraphicsPipelineCreateInfo depthPipe=pipe;
        depthPipe.pDynamicState=&shadowDynamic;
        depthPipe.stageCount=1;depthPipe.pStages=&depthStage;
        depthPipe.pViewportState=&shadowVp;depthPipe.pColorBlendState=&shadowBlend;
        depthPipe.layout=shadowLayout;depthPipe.renderPass=shadowPass;
        if(!persistent.sunPipeline)
            require(vkCreateGraphicsPipelines(device,persistent.pipelineCache,1,&depthPipe,nullptr,
                                              &persistent.sunPipeline),"Create Sun depth graphics pipeline failed");
        shadowPipeline=persistent.sunPipeline;
        persistent.pipelineW=w; persistent.pipelineH=h;
        persistent.pipelineShadowSize=shadowSize;
        VkImageView attachmentViews[2]={colorView,depthView};
        VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fi.renderPass=pass;fi.attachmentCount=2;fi.pAttachments=attachmentViews;
        fi.width=w;fi.height=h;fi.layers=1;
        require(vkCreateFramebuffer(device,&fi,nullptr,&framebuffer),"Create framebuffer failed");
        VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pci.queueFamilyIndex=family;
        require(vkCreateCommandPool(device,&pci,nullptr,&pool),"Create command pool failed");
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool=pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;
        VkCommandBuffer cmd{};
        require(vkAllocateCommandBuffers(device,&ai,&cmd),"Allocate command buffer failed");
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        require(vkBeginCommandBuffer(cmd,&bi),"Begin GPU commands failed");
        VkClearValue clears[2]{};clears[0].color={{0.035f,0.10f,0.17f,1.f}};clears[1].depthStencil={1.f,0};
        VkRenderPassBeginInfo rb{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rb.renderPass=pass;rb.framebuffer=framebuffer;rb.renderArea.extent={w,h};
        rb.clearValueCount=2;rb.pClearValues=clears;
        VkDeviceSize offset=0;
        VkClearValue shadowClear{};
        shadowClear.depthStencil={1.f,0};
        VkRenderPassBeginInfo shadowBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        shadowBegin.renderPass=shadowPass;shadowBegin.framebuffer=shadowFramebuffer;
        shadowBegin.renderArea.extent={shadowWidth,shadowHeight};
        shadowBegin.clearValueCount=1;shadowBegin.pClearValues=&shadowClear;
        vkCmdBeginRenderPass(cmd,&shadowBegin,VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,shadowPipeline);
        vkCmdBindVertexBuffers(cmd,0,1,&vertices,&offset);
        if(usePointAtlas){
            for(int face=0;face<6;++face){
                VkViewport tile{float((face%3)*shadowSize),
                                float((face/3)*shadowSize),
                                float(shadowSize),float(shadowSize),0.f,1.f};
                VkRect2D tileScissor{{int32_t((face%3)*shadowSize),
                                      int32_t((face/3)*shadowSize)},{shadowSize,shadowSize}};
                vkCmdSetViewport(cmd,0,1,&tile);
                vkCmdSetScissor(cmd,0,1,&tileScissor);
                vkCmdPushConstants(cmd,shadowLayout,VK_SHADER_STAGE_VERTEX_BIT,0,
                                   sizeof(lightMatrix),shadowData.faces[face]);
                vkCmdDraw(cmd,indexCount,1,0,0);
            }
        }else{
            VkViewport tile{0.f,0.f,float(shadowSize),float(shadowSize),0.f,1.f};
            VkRect2D tileScissor{{0,0},{shadowSize,shadowSize}};
            vkCmdSetViewport(cmd,0,1,&tile);
            vkCmdSetScissor(cmd,0,1,&tileScissor);
            vkCmdPushConstants(cmd,shadowLayout,VK_SHADER_STAGE_VERTEX_BIT,0,
                               sizeof(lightMatrix),lightMatrix);
            vkCmdDraw(cmd,indexCount,1,0,0);
        }
        vkCmdEndRenderPass(cmd);
        vkCmdBeginRenderPass(cmd,&rb,VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        vkCmdBindVertexBuffers(cmd,0,1,&vertices,&offset);
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,1,
                                &descriptorSet,0,nullptr);
        Params params{};
        std::memcpy(params.mvp,mvp,sizeof(params.mvp));
        params.dir[2]=-1.f;params.color[0]=params.color[1]=params.color[2]=1.f;
        params.color[3]=1.f;
        if(lights){
            for(uint32_t i=0;i<lightCount;++i)if(lights[12*i]<0.5f){
                std::memcpy(params.dir,lights+12*i+4,3*sizeof(float));
                std::memcpy(params.color,lights+12*i+7,3*sizeof(float));
                params.color[3]=0.8f*std::log1p(std::max(0.f,lights[12*i+10]));
                break;
            }
        }
        vkCmdPushConstants(cmd,layout,pr.stageFlags,0,sizeof(params),&params);
        vkCmdDraw(cmd,indexCount,1,0,0);
        vkCmdEndRenderPass(cmd);
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount=1;region.imageExtent={w,h,1};
        vkCmdCopyImageToBuffer(cmd,color,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback,1,&region);
        VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer=readback;barrier.size=VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,
            0,0,nullptr,1,&barrier,0,nullptr);
        require(vkEndCommandBuffer(cmd),"End GPU commands failed");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
        require(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"Submit GPU commands failed");
        require(vkQueueWaitIdle(queue),"GPU completion failed");
        require(vkMapMemory(device,readbackMemory,0,VK_WHOLE_SIZE,0,&mapped),"Map GPU readback failed");
        std::memcpy(pixels,mapped,size_t(w)*h*4);
        vkUnmapMemory(device,readbackMemory);
        cachedImage.assign(pixels,pixels+size_t(w)*h*4);
        cachedKey=key;cachedW=w;cachedH=h;
        cleanup();return 1;
    }catch(const std::exception& ex){std::string msg=ex.what();cleanup();return fail(msg.c_str());}
}

extern "C" int hamun_gpu_viewport_render(
    uint32_t w,uint32_t h,uint8_t* pixels,uint64_t capacity,
    const float* positions,uint32_t vertexCount,const uint32_t* indices,uint32_t indexCount,
    const float* mvp,const float* lights,uint32_t lightCount,const float* faceNormals,
    char* error,uint32_t errorCapacity){
    return gpuViewportImpl(w,h,pixels,capacity,positions,vertexCount,indices,indexCount,
                           mvp,lights,lightCount,faceNormals,error,errorCapacity,1024,1.f,1.f);
}
extern "C" int hamun_gpu_viewport_render_shadow(
    uint32_t w,uint32_t h,uint8_t* pixels,uint64_t capacity,
    const float* positions,uint32_t vertexCount,const uint32_t* indices,uint32_t indexCount,
    const float* mvp,const float* lights,uint32_t lightCount,const float* faceNormals,
    char* error,uint32_t errorCapacity,uint32_t shadowSize,float softness,float bias){
    if(shadowSize<64||shadowSize>4096||(shadowSize&(shadowSize-1))!=0||
       !std::isfinite(softness)||softness<0.f||softness>4.f||
       !std::isfinite(bias)||bias<0.f||bias>4.f)return 0;
    return gpuViewportImpl(w,h,pixels,capacity,positions,vertexCount,indices,indexCount,
                           mvp,lights,lightCount,faceNormals,error,errorCapacity,
                           shadowSize,softness,bias);
}
