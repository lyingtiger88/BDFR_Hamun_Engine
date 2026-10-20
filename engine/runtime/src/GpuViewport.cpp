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
    mix(&w,sizeof(w));mix(&h,sizeof(h));
    mix(&requestedShadowSize,sizeof(requestedShadowSize));
    mix(&shadowSoftness,sizeof(shadowSoftness));mix(&shadowBias,sizeof(shadowBias));
    mix(&vertexCount,sizeof(vertexCount));mix(&indexCount,sizeof(indexCount));
    mix(positions,size_t(vertexCount)*3*sizeof(float));
    mix(indices,size_t(indexCount)*sizeof(uint32_t));
    mix(faceNormals,size_t(indexCount)*sizeof(float));
    mix(mvp,16*sizeof(float));mix(&lightCount,sizeof(lightCount));
    if(lightCount)mix(lights,size_t(lightCount)*12*sizeof(float));
    if(key==cachedKey&&cachedW==w&&cachedH==h&&cachedImage.size()==size_t(w)*h*4){
        std::memcpy(pixels,cachedImage.data(),cachedImage.size());
        return 1;
    }
    VkInstance instance=VK_NULL_HANDLE;
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
    VkBuffer shadowUniform=VK_NULL_HANDLE;
    VkDeviceMemory shadowUniformMemory=VK_NULL_HANDLE;
    VkBuffer vertices=VK_NULL_HANDLE,readback=VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory=VK_NULL_HANDLE,readbackMemory=VK_NULL_HANDLE;
    auto cleanup=[&]{
        if(device){
            vkDeviceWaitIdle(device);
            if(framebuffer)vkDestroyFramebuffer(device,framebuffer,nullptr);
            if(shadowFramebuffer)vkDestroyFramebuffer(device,shadowFramebuffer,nullptr);
            if(shadowPipeline)vkDestroyPipeline(device,shadowPipeline,nullptr);
            if(shadowLayout)vkDestroyPipelineLayout(device,shadowLayout,nullptr);
            if(shadowPass)vkDestroyRenderPass(device,shadowPass,nullptr);
            if(shadowVert)vkDestroyShaderModule(device,shadowVert,nullptr);
            if(shadowSampler)vkDestroySampler(device,shadowSampler,nullptr);
            if(descriptorPool)vkDestroyDescriptorPool(device,descriptorPool,nullptr);
            if(setLayout)vkDestroyDescriptorSetLayout(device,setLayout,nullptr);
            if(shadowUniform)vkDestroyBuffer(device,shadowUniform,nullptr);
            if(shadowUniformMemory)vkFreeMemory(device,shadowUniformMemory,nullptr);
            if(shadowView)vkDestroyImageView(device,shadowView,nullptr);
            if(shadowImage)vkDestroyImage(device,shadowImage,nullptr);
            if(shadowMemory)vkFreeMemory(device,shadowMemory,nullptr);
            if(pipeline)vkDestroyPipeline(device,pipeline,nullptr);
            if(layout)vkDestroyPipelineLayout(device,layout,nullptr);
            if(pass)vkDestroyRenderPass(device,pass,nullptr);
            if(vert)vkDestroyShaderModule(device,vert,nullptr);
            if(frag)vkDestroyShaderModule(device,frag,nullptr);
            if(colorView)vkDestroyImageView(device,colorView,nullptr);
            if(depthView)vkDestroyImageView(device,depthView,nullptr);
            if(color)vkDestroyImage(device,color,nullptr);
            if(depth)vkDestroyImage(device,depth,nullptr);
            if(colorMemory)vkFreeMemory(device,colorMemory,nullptr);
            if(depthMemory)vkFreeMemory(device,depthMemory,nullptr);
            if(vertices)vkDestroyBuffer(device,vertices,nullptr);
            if(readback)vkDestroyBuffer(device,readback,nullptr);
            if(vertexMemory)vkFreeMemory(device,vertexMemory,nullptr);
            if(readbackMemory)vkFreeMemory(device,readbackMemory,nullptr);
            if(pool)vkDestroyCommandPool(device,pool,nullptr);
            vkDestroyDevice(device,nullptr);
        }
        if(instance)vkDestroyInstance(instance,nullptr);
    };
    try {
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
        VkPhysicalDevice gpu=VK_NULL_HANDLE;uint32_t family=UINT32_MAX;
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
        VkQueue queue{};vkGetDeviceQueue(device,family,0,&queue);
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
        createBuffer(mesh.size()*sizeof(Vertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,vertices,vertexMemory);
        void* mapped=nullptr;
        require(vkMapMemory(device,vertexMemory,0,VK_WHOLE_SIZE,0,&mapped),"Map vertices failed");
        std::memcpy(mapped,mesh.data(),mesh.size()*sizeof(Vertex));vkUnmapMemory(device,vertexMemory);
        createBuffer(VkDeviceSize(w)*h*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT,readback,readbackMemory);
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
        require(vkCreateRenderPass(device,&rp,nullptr,&pass),"Create render pass failed");
        auto module=[&](const wchar_t* filename){
            auto bytes=binary(filename);
            VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            ci.codeSize=bytes.size();ci.pCode=reinterpret_cast<const uint32_t*>(bytes.data());
            VkShaderModule sm{};
            require(vkCreateShaderModule(device,&ci,nullptr,&sm),"Create shader module failed");
            return sm;
        };
        vert=module(L"viewport_direct.vert.spv");frag=module(L"viewport_direct.frag.spv");
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
        require(vkCreatePipelineLayout(device,&pli,nullptr,&layout),"Create pipeline layout failed");
        VkGraphicsPipelineCreateInfo pipe{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipe.stageCount=2;pipe.pStages=stages;pipe.pVertexInputState=&vi;
        pipe.pInputAssemblyState=&ia;pipe.pViewportState=&vp;
        pipe.pRasterizationState=&rs;pipe.pMultisampleState=&ms;
        pipe.pColorBlendState=&cb;pipe.pDepthStencilState=&ds;
        pipe.layout=layout;pipe.renderPass=pass;
        require(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pipe,nullptr,&pipeline),"Create graphics pipeline failed");
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
        vkCmdBeginRenderPass(cmd,&rb,VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        VkDeviceSize offset=0;vkCmdBindVertexBuffers(cmd,0,1,&vertices,&offset);
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
