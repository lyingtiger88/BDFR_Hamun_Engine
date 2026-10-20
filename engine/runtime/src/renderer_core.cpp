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

static int hamun_mesh_render_impl(
    uint32_t width,uint32_t height,uint8_t* rgba,uint64_t byteCount,
    const float* xyz,uint32_t vertices,const uint32_t* indices,uint32_t count,
    const float* mvp,const float* lights,uint32_t lightCount,
    const float* faceNormals,uint32_t requestedShadowSize=256,
    float softness=1.f,float biasScale=1.f){
    lastError.clear();
    if(!xyz||!indices||vertices>1000000||count>3000000||count%3!=0){
        lastError="Invalid mesh buffer";return 0;
    }
    // CPU transitional rasterizer: do not create/destroy a Vulkan device and
    // synchronously read back a clear image every frame. That was the main
    // per-frame stall; the real GPU path must own its color attachment.
    if(!rgba||width==0||height==0||width>4096||height>4096||
       byteCount<uint64_t(width)*height*4){lastError="Invalid output image";return 0;}
    for(size_t i=0,n=size_t(width)*height;i<n;++i){
        rgba[4*i]=9;rgba[4*i+1]=26;rgba[4*i+2]=43;rgba[4*i+3]=255;
    }
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
            if(mvp){
                float cx=mvp[0]*x+mvp[4]*y+mvp[8]*z+mvp[12];
                float cy2=mvp[1]*x+mvp[5]*y+mvp[9]*z+mvp[13];
                float cz=mvp[2]*x+mvp[6]*y+mvp[10]*z+mvp[14];
                float cw=mvp[3]*x+mvp[7]*y+mvp[11]*z+mvp[15];
                if(cw>0.0001f)
                    projected[i]={float(width)*(cx/cw*.5f+.5f),
                                  float(height)*(1.f-(cy2/cw*.5f+.5f)),
                                  -(cz/cw)};
                else projected[i]={-1e8f,-1e8f,-1e8f};
            }else projected[i]={width*.5f+rx*scale,height*.52f-rz*scale,depth};
        }
        // CPU directional shadow map (temporary until Vulkan depth passes).
        // Scene geometry is in world space; the same positions are used by
        // lighting and occlusion, avoiding camera/world-space mismatches.
        struct ShadowMap {
            float ux,uy,uz,vx,vy,vz,dx,dy,dz;
            float minU,minV,step;
            std::vector<float> depth;
        };
        const int shadowSize=int(requestedShadowSize);
        if(shadowSize<64 || shadowSize>4096 || (shadowSize & (shadowSize-1))) {
            lastError="Invalid Sun shadow resolution"; return 0;
        }
        // Reuse the sun shadow atlas when geometry and sun orientation match.
        // Blender redraws unchanged frames frequently; rebuilding the atlas
        // for each such frame is wasteful.
        static thread_local uint64_t cachedHash=0;
        static thread_local std::vector<ShadowMap> cachedShadows;
        uint64_t hash=14695981039346656037ull;
        auto hashBytes=[&](const void* p,size_t size){
            const auto* bytes=static_cast<const unsigned char*>(p);
            for(size_t k=0;k<size;++k){hash^=bytes[k];hash*=1099511628211ull;}
        };
        hashBytes(&shadowSize,sizeof(shadowSize));
        hashBytes(&vertices,sizeof(vertices));
        hashBytes(&count,sizeof(count));
        hashBytes(xyz,size_t(vertices)*3*sizeof(float));
        hashBytes(indices,size_t(count)*sizeof(uint32_t));
        hashBytes(&lightCount,sizeof(lightCount));
        if(lights){
            for(uint32_t li=0;li<lightCount;++li){
                hashBytes(lights+li*12,sizeof(float));
                if(lights[li*12]<0.5f){
                    hashBytes(lights+li*12+1,sizeof(float)); // Sun angular diameter
                    hashBytes(lights+li*12+4,3*sizeof(float));
                    hashBytes(lights+li*12+10,2*sizeof(float)); // energy + cast-shadow flag
                }
            }
        }
        bool rebuild=hash!=cachedHash||cachedShadows.size()!=lightCount;
        std::vector<ShadowMap> shadows;
        if(!rebuild)shadows=cachedShadows;
        else shadows.resize(lightCount);
        if(rebuild)for(uint32_t li=0;li<lightCount;++li){
            const float* l=lights+12*li;
            if(l[0]>=0.5f || l[11]<0.f || l[10]<=0.f)continue; // Sun with shadows enabled
            auto& sm=shadows[li];
            float dx=l[4],dy=l[5],dz=l[6];
            float mag=std::sqrt(dx*dx+dy*dy+dz*dz);
            if(mag<1e-6f)continue;
            sm.dx=dx/mag;sm.dy=dy/mag;sm.dz=dz/mag;
            float ax=std::abs(sm.dz)<0.9f?0.f:0.f;
            float ay=std::abs(sm.dz)<0.9f?0.f:1.f;
            float az=std::abs(sm.dz)<0.9f?1.f:0.f;
            sm.ux=ay*sm.dz-az*sm.dy;
            sm.uy=az*sm.dx-ax*sm.dz;
            sm.uz=ax*sm.dy-ay*sm.dx;
            mag=std::sqrt(sm.ux*sm.ux+sm.uy*sm.uy+sm.uz*sm.uz);
            sm.ux/=mag;sm.uy/=mag;sm.uz/=mag;
            sm.vx=sm.dy*sm.uz-sm.dz*sm.uy;
            sm.vy=sm.dz*sm.ux-sm.dx*sm.uz;
            sm.vz=sm.dx*sm.uy-sm.dy*sm.ux;
            float minU=1e30f,maxU=-1e30f,minV=1e30f,maxV=-1e30f;
            for(uint32_t vi=0;vi<vertices;++vi){
                float x=xyz[3*vi],y=xyz[3*vi+1],z=xyz[3*vi+2];
                float u=x*sm.ux+y*sm.uy+z*sm.uz;
                float v=x*sm.vx+y*sm.vy+z*sm.vz;
                minU=std::min(minU,u);maxU=std::max(maxU,u);
                minV=std::min(minV,v);maxV=std::max(maxV,v);
            }
            float extent=std::max({maxU-minU,maxV-minV,0.01f})*1.02f;
            sm.step=extent/float(shadowSize-1);
            sm.minU=(minU+maxU-extent)*.5f;
            sm.minV=(minV+maxV-extent)*.5f;
            sm.depth.assign(shadowSize*shadowSize,-1e30f);
            struct S {float x,y,z;};
            auto edgeS=[](const S&a,const S&b,float x,float y){
                return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);
            };
            auto project=[&](uint32_t vi){
                float x=xyz[3*vi],y=xyz[3*vi+1],z=xyz[3*vi+2];
                return S{(x*sm.ux+y*sm.uy+z*sm.uz-sm.minU)/sm.step,
                    (x*sm.vx+y*sm.vy+z*sm.vz-sm.minV)/sm.step,
                    -(x*sm.dx+y*sm.dy+z*sm.dz)};
            };
            for(uint32_t ti=0;ti<count;ti+=3){
                uint32_t ia=indices[ti],ib=indices[ti+1],ic=indices[ti+2];
                if(ia>=vertices||ib>=vertices||ic>=vertices)continue;
                S a=project(ia),b=project(ib),c=project(ic);
                float area=edgeS(a,b,c.x,c.y);
                if(std::abs(area)<1e-7f)continue;
                int x0=std::max(0,int(std::floor(std::min({a.x,b.x,c.x}))));
                int x1=std::min(shadowSize-1,int(std::ceil(std::max({a.x,b.x,c.x}))));
                int y0=std::max(0,int(std::floor(std::min({a.y,b.y,c.y}))));
                int y1=std::min(shadowSize-1,int(std::ceil(std::max({a.y,b.y,c.y}))));
                for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
                    float wa=edgeS(b,c,x+.5f,y+.5f)/area;
                    float wb=edgeS(c,a,x+.5f,y+.5f)/area;
                    float wc=1-wa-wb;
                    if(wa<0||wb<0||wc<0)continue;
                    float d=wa*a.z+wb*b.z+wc*c.z;
                    float& pixel=sm.depth[size_t(y)*shadowSize+x];
                    pixel=std::max(pixel,d);
                }
            }
        }
        if(rebuild){cachedShadows=shadows;cachedHash=hash;}
        std::vector<float> zbuf(size_t(width)*height,-1e30f);
        auto edge=[](const P& a,const P& b,float x,float y){
            return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);
        };
        for(uint32_t i=0;i<count;i+=3){
            uint32_t ia=indices[i],ib=indices[i+1],ic=indices[i+2];
            if(ia>=vertices||ib>=vertices||ic>=vertices)continue;
            const P A=projected[ia],B=projected[ib],C=projected[ic];
            // Reject invalid / behind-camera triangles before computing bounds.
            if(!std::isfinite(A.x)||!std::isfinite(A.y)||!std::isfinite(A.z)||
               !std::isfinite(B.x)||!std::isfinite(B.y)||!std::isfinite(B.z)||
               !std::isfinite(C.x)||!std::isfinite(C.y)||!std::isfinite(C.z)||
               A.x < -1e7f || B.x < -1e7f || C.x < -1e7f) continue;
            float area=edge(A,B,C.x,C.y);
            if(std::abs(area)<1e-5f)continue;
            int x0=std::max(0,int(std::floor(std::min({A.x,B.x,C.x}))));
            int x1=std::min(int(width)-1,int(std::ceil(std::max({A.x,B.x,C.x}))));
            int y0=std::max(0,int(std::floor(std::min({A.y,B.y,C.y}))));
            int y1=std::min(int(height)-1,int(std::ceil(std::max({A.y,B.y,C.y}))));
            float ux=xyz[ib*3]-xyz[ia*3],uy=xyz[ib*3+1]-xyz[ia*3+1],uz=xyz[ib*3+2]-xyz[ia*3+2];
            float vx=xyz[ic*3]-xyz[ia*3],vy=xyz[ic*3+1]-xyz[ia*3+1],vz=xyz[ic*3+2]-xyz[ia*3+2];
            float nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
            // Blender supplies inverse-transpose transformed polygon normals.
            // The cross product is retained for legacy callers only.
            if(faceNormals){
                nx=faceNormals[i];ny=faceNormals[i+1];nz=faceNormals[i+2];
            }
            const float nlen=std::sqrt(nx*nx+ny*ny+nz*nz);
            if(!(nlen>1e-8f)||!std::isfinite(nlen))continue;
            nx/=nlen; ny/=nlen; nz/=nlen;
            // Hemisphere environment fill: the unlit side faces must remain
            // discernible. This is a cheap ambient approximation, not GI.
            const float sky=std::clamp(0.5f+0.5f*nz,0.0f,1.0f);
            float red=0.17f+0.12f*sky;
            float green=0.19f+0.13f*sky;
            float blue=0.22f+0.14f*sky;
            // Retain each Sun contribution in linear space. Shadowing must
            // attenuate exactly the same energy that illuminates the face.
            float sunRed[32]={},sunGreen[32]={},sunBlue[32]={};
            if(lights&&lightCount){
                float mx=(xyz[ia*3]+xyz[ib*3]+xyz[ic*3])/3.0f;
                float my=(xyz[ia*3+1]+xyz[ib*3+1]+xyz[ic*3+1])/3.0f;
                float mz=(xyz[ia*3+2]+xyz[ib*3+2]+xyz[ic*3+2])/3.0f;
                for(uint32_t li=0;li<std::min(lightCount,32u);++li){
                    const float* light=lights+12*li;
                    float lx,ly,lz,attenuation=1.0f;
                    if(light[0]<0.5f){
                        lx=-light[4];ly=-light[5];lz=-light[6];
                    }else{
                        lx=light[1]-mx;ly=light[2]-my;lz=light[3]-mz;
                        float d2=std::max(0.05f,lx*lx+ly*ly+lz*lz);
                        attenuation=1.0f/(1.0f+d2*0.15f);
                        if(light[0]>1.5f&&light[0]<2.5f){
                            float inv=1.0f/std::sqrt(d2);
                            float aligned=-(lx*light[4]+ly*light[5]+lz*light[6])*inv;
                            attenuation*=std::max(0.0f,std::min(1.0f,(aligned-0.65f)/0.2f));
                        }
                    }
                    float len=std::sqrt(lx*lx+ly*ly+lz*lz)+1e-8f;
                    const float ndotl=(nx*lx+ny*ly+nz*lz)/len;
                    const bool sun=light[0]<0.5f;
                    // The current preview has no bounce lighting. A small
                    // wrapped diffuse term keeps side faces visible when the
                    // Sun is almost perpendicular, without lighting rear faces.
                    // True Lambert cosine: no artificial light on backfaces.
                    const float diffuse=std::max(0.0f,ndotl);
                    const float energy=std::max(0.0f,light[10]);
                    // Keep Sun 1, 5, 100 and 1000 visually distinguishable.
                    // Energy/exposure are display-mapped in this preview;
                    // physically based exposure is a later pipeline stage.
                    const float strength=(sun
                        ? 0.8f*std::log1p(energy)
                        : 0.012f*energy)*attenuation*diffuse;
                    red+=light[7]*strength;green+=light[8]*strength;blue+=light[9]*strength;
                    if(sun){sunRed[li]=light[7]*strength;sunGreen[li]=light[8]*strength;sunBlue[li]=light[9]*strength;}
                }
            }else{
                float diffuse=std::max(0.0f,nx*.35f+ny*.4f+nz*.85f);
                red+=diffuse*.70f;green+=diffuse*.70f;blue+=diffuse*.70f;
            }
            // Reinhard tonemapping instead of clipping at 1.0: high
            // energy stays distinguishable rather than saturating to one
            // identical pixel value for all intensities >= 1.
            red=red/(1.0f+red);
            green=green/(1.0f+green);
            blue=blue/(1.0f+blue);
            for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
                float px=x+.5f,py=y+.5f;
                float wa=edge(B,C,px,py)/area,wb=edge(C,A,px,py)/area,wc=1-wa-wb;
                if(wa<0||wb<0||wc<0)continue;
                float depth=wa*A.z+wb*B.z+wc*C.z;
                size_t offset=size_t(y)*width+x;
                if(depth<=zbuf[offset])continue;
                zbuf[offset]=depth;
                float shadeR=red,shadeG=green,shadeB=blue;
                // Perspective-correct reconstruction of world position.
                // Shadow filtering is applied before tone mapping.
                if(lights && lightCount){
                    float qa=wa,qb=wb,qc=wc;
                    if(mvp){
                        auto clipW=[&](uint32_t vi){
                            const float* p=xyz+3*vi;
                            return mvp[3]*p[0]+mvp[7]*p[1]+mvp[11]*p[2]+mvp[15];
                        };
                        float aw=clipW(ia),bw=clipW(ib),cw=clipW(ic);
                        if(aw>1e-5f&&bw>1e-5f&&cw>1e-5f){
                            qa/=aw;qb/=bw;qc/=cw;
                            float total=qa+qb+qc;
                            qa/=total;qb/=total;qc/=total;
                        }
                    }
                    float wx=qa*xyz[3*ia]+qb*xyz[3*ib]+qc*xyz[3*ic];
                    float wy=qa*xyz[3*ia+1]+qb*xyz[3*ib+1]+qc*xyz[3*ic+1];
                    float wz=qa*xyz[3*ia+2]+qb*xyz[3*ib+2]+qc*xyz[3*ic+2];
                    float litR=red/(1.f-red),litG=green/(1.f-green),litB=blue/(1.f-blue);
                    for(uint32_t li=0;li<lightCount;++li){
                        const auto& sm=shadows[li];
                        if(sm.depth.empty())continue;
                        float sx=(wx*sm.ux+wy*sm.uy+wz*sm.uz-sm.minU)/sm.step;
                        float sy=(wx*sm.vx+wy*sm.vy+wz*sm.vz-sm.minV)/sm.step;
                        int ix=int(std::floor(sx)),iy=int(std::floor(sy));
                        if(ix<1||iy<1||ix>=shadowSize-1||iy>=shadowSize-1)continue;
                        float d=-(wx*sm.dx+wy*sm.dy+wz*sm.dz);
                        // Bias in light-space depth units, scaled to the shadow texel size.
                        // A two-texel bias can erase shallow contact shadows.
                        const float bias=std::max(0.0001f,sm.step*0.3f)*biasScale;
                        const float* sun=lights+12*li;
                        // Slot 1 stores the Sun angular diameter in radians; its
                        // world-space position must not affect directional shadows.
                        const float angle=std::clamp(sun[1],0.f,3.14159f);
                        const float angularRadius=std::tan(std::min(angle*0.5f,1.45f));
                        const int cx=std::clamp(int(std::round(sx)),0,shadowSize-1);
                        const int cy=std::clamp(int(std::round(sy)),0,shadowSize-1);
                        const float blockerDepth=sm.depth[size_t(cy)*shadowSize+cx];
                        const float separation=std::max(0.f,blockerDepth-d);
                        // Bounded approximate penumbra for the transitional CPU path.
                        const int radius=std::clamp(
                            int(std::ceil(angularRadius*softness*separation/std::max(sm.step,1e-6f))),
                            1,5);
                        float visibility=0.f;
                        int samples=0;
                        for(int oy=-radius;oy<=radius;++oy)
                            for(int ox=-radius;ox<=radius;++ox){
                                const int x=std::clamp(cx+ox,0,shadowSize-1);
                                const int y=std::clamp(cy+oy,0,shadowSize-1);
                                visibility+=(d+bias>=sm.depth[size_t(y)*shadowSize+x])?1.f:0.f;
                                ++samples;
                            }
                        visibility/=float(samples);
                        if(visibility>=1.f)continue;
                        const float* light=lights+12*li;
                        // Only occlude this Sun's own direct contribution;
                        // preserve ambient and other lights in the shadow.
                        litR=std::max(0.f,litR-sunRed[li]*(1.f-visibility));
                        litG=std::max(0.f,litG-sunGreen[li]*(1.f-visibility));
                        litB=std::max(0.f,litB-sunBlue[li]*(1.f-visibility));
                    }
                    shadeR=litR/(1.f+litR);shadeG=litG/(1.f+litG);shadeB=litB/(1.f+litB);
                }
                uint8_t* pixel=rgba+offset*4;
                pixel[0]=uint8_t(195*shadeR);
                pixel[1]=uint8_t(215*shadeG);
                pixel[2]=uint8_t(232*shadeB);
                pixel[3]=255;
            }
        }
        return 1;
    }catch(const std::exception& ex){lastError=ex.what();return 0;}
}

extern "C" int hamun_renderer_mesh_preview(
    uint32_t w,uint32_t h,uint8_t* rgba,uint64_t n,const float* xyz,
    uint32_t nv,const uint32_t* idx,uint32_t ni){
    return hamun_mesh_render_impl(w,h,rgba,n,xyz,nv,idx,ni,nullptr,nullptr,0,nullptr);
}
extern "C" int hamun_renderer_mesh_camera(
    uint32_t w,uint32_t h,uint8_t* rgba,uint64_t n,const float* xyz,
    uint32_t nv,const uint32_t* idx,uint32_t ni,const float* mvp){
    if(!mvp){lastError="Missing camera matrix";return 0;}
    return hamun_mesh_render_impl(w,h,rgba,n,xyz,nv,idx,ni,mvp,nullptr,0,nullptr);
}

extern "C" int hamun_renderer_mesh_lit(uint32_t w,uint32_t h,uint8_t* rgba,uint64_t n,
    const float* xyz,uint32_t nv,const uint32_t* idx,uint32_t ni,
    const float* mvp,const float* lights,uint32_t lightCount) {
    if(!mvp){lastError="Missing camera matrix";return 0;}
    if(lightCount>32|| (lightCount>0&&!lights)){lastError="Invalid light array";return 0;}
    return hamun_mesh_render_impl(w,h,rgba,n,xyz,nv,idx,ni,mvp,lights,lightCount,nullptr);
}

// Optional explicit world-space normal for each indexed triangle (3 floats).
// This entry point preserves the original ABI while fixing mirrored transforms
// and Blender's evaluated mesh orientation in the live viewport.
extern "C" int hamun_renderer_mesh_lit_normals(uint32_t w,uint32_t h,uint8_t* rgba,uint64_t n,
    const float* xyz,uint32_t nv,const uint32_t* idx,uint32_t ni,
    const float* mvp,const float* lights,uint32_t lightCount,const float* normals) {
    if(!mvp){lastError="Missing camera matrix";return 0;}
    if(lightCount>32||(lightCount>0&&!lights)){lastError="Invalid light array";return 0;}
    if(!normals){lastError="Missing world-space face normals";return 0;}
    return hamun_mesh_render_impl(w,h,rgba,n,xyz,nv,idx,ni,mvp,lights,lightCount,normals);
}

// Optional extended CPU shadow API. Previous DLL exports remain unchanged.
extern "C" int hamun_renderer_mesh_lit_normals_quality(uint32_t w,uint32_t h,uint8_t* rgba,uint64_t n,
    const float* xyz,uint32_t nv,const uint32_t* idx,uint32_t ni,
    const float* mvp,const float* lights,uint32_t lightCount,const float* normals,
    uint32_t shadowResolution) {
    if(!mvp){lastError="Missing camera matrix";return 0;}
    if(lightCount>32||(lightCount>0&&!lights)){lastError="Invalid light array";return 0;}
    if(!normals){lastError="Missing world-space face normals";return 0;}
    if(shadowResolution<64||shadowResolution>4096||
       (shadowResolution&(shadowResolution-1))!=0){
        lastError="Invalid shadow map resolution";return 0;
    }
    return hamun_mesh_render_impl(w,h,rgba,n,xyz,nv,idx,ni,mvp,lights,lightCount,normals,shadowResolution);
}

extern "C" int hamun_renderer_mesh_lit_normals_settings(uint32_t w,uint32_t h,uint8_t* rgba,uint64_t n,
    const float* xyz,uint32_t nv,const uint32_t* idx,uint32_t ni,
    const float* mvp,const float* lights,uint32_t lightCount,const float* normals,
    uint32_t shadowResolution,float softness,float biasScale) {
    if(!mvp||!normals){lastError="Missing camera matrix or normals";return 0;}
    if(lightCount>32||(lightCount>0&&!lights)){lastError="Invalid light array";return 0;}
    if(shadowResolution<64||shadowResolution>4096||(shadowResolution&(shadowResolution-1))!=0 ||
       !std::isfinite(softness)||softness<0.f||softness>4.f ||
       !std::isfinite(biasScale)||biasScale<0.f||biasScale>4.f){lastError="Invalid shadow settings";return 0;}
    return hamun_mesh_render_impl(w,h,rgba,n,xyz,nv,idx,ni,mvp,lights,lightCount,normals,
                                  shadowResolution,softness,biasScale);
}
