// SPDX-License-Identifier: GPL-2.0-or-later
// Hamun Runtime 0.1 - native Win32 + Vulkan first triangle.
#include <windows.h>
#include "mesh.hpp"
#include "diagnostics.hpp"
#include <chrono>
#include <cstddef>
#include <cstring>
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

static void check(VkResult result, const char *message) {
    if (result != VK_SUCCESS) throw std::runtime_error(message);
}
static std::vector<char> readFile(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error(std::string("Cannot read shader: ") + path);
    auto size = file.tellg();
    std::vector<char> bytes(static_cast<size_t>(size));
    file.seekg(0);
    file.read(bytes.data(), size);
    return bytes;
}
struct Runtime {
    HINSTANCE hinstance{};
    HWND hwnd{};
    bool running = true, resized = false;
    VkInstance instance{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice physical{};
    VkDevice device{};
    uint32_t family = UINT32_MAX;
    VkQueue queue{};
    VkSwapchainKHR swapchain{};
    VkFormat format{};
    VkExtent2D extent{};
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> frames;
    VkRenderPass renderPass{};
    VkImage depthImage{};
    VkDeviceMemory depthMemory{};
    VkImageView depthView{};
    float yaw=0.15f, pitch=0.15f, distance=10.0f;
    VkPipelineLayout pipelineLayout{};
    VkPipeline pipeline{};
    VkCommandPool commandPool{};
    std::vector<VkCommandBuffer> cmds;
    VkSemaphore imageReady{}, renderDone{};
    VkFence frameFence{};
    HamunDiagnostics diagnostics;
    std::wstring presentModeName=L"FIFO";
    HamunMesh mesh=hamunMakeSphere();
    VkBuffer vertexBuffer{}, indexBuffer{};
    VkDeviceMemory vertexMemory{}, indexMemory{};
    struct Push { float camera[4]; float object[4]; float material[4]; };
    void uploadBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
        VkBuffer& buffer, VkDeviceMemory& memory) {
        VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        ci.size=size; ci.usage=usage; ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device,&ci,nullptr,&buffer),"Create mesh buffer failed");
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device,buffer,&req);
        VkPhysicalDeviceMemoryProperties props{};
        vkGetPhysicalDeviceMemoryProperties(physical,&props);
        uint32_t type=UINT32_MAX;
        for(uint32_t i=0;i<props.memoryTypeCount;++i)
            if((req.memoryTypeBits&(1u<<i)) &&
               (props.memoryTypes[i].propertyFlags&
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
                {type=i;break;}
        if(type==UINT32_MAX) throw std::runtime_error("No coherent host-visible GPU memory");
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize=req.size; ai.memoryTypeIndex=type;
        check(vkAllocateMemory(device,&ai,nullptr,&memory),"Allocate mesh buffer failed");
        check(vkBindBufferMemory(device,buffer,memory,0),"Bind mesh buffer failed");
        void* mapped=nullptr;
        check(vkMapMemory(device,memory,0,size,0,&mapped),"Map mesh memory failed");
        std::memcpy(mapped,data,static_cast<size_t>(size));
        vkUnmapMemory(device,memory);
    }


    void window(HINSTANCE app) {
        hinstance = app;
        WNDCLASSW cls{};
        cls.lpfnWndProc = [](HWND window, UINT msg, WPARAM w, LPARAM l) -> LRESULT {
            Runtime* self = reinterpret_cast<Runtime*>(GetWindowLongPtrW(window, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return TRUE;
            }
            if (msg == WM_SIZE && self) self->resized = true;
            if (msg == WM_CLOSE) { DestroyWindow(window); return 0; }
            if (msg == WM_DESTROY) {
                if (self) self->running = false;
                PostQuitMessage(0);
                return 0;
            }
            return DefWindowProcW(window, msg, w, l);
        };
        cls.hInstance = app;
        cls.lpszClassName = L"HamunRuntimeWindow";
        cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&cls);
        hwnd = CreateWindowExW(0, cls.lpszClassName, L"Hamun Runtime - Vulkan 0.5 | Material Laboratory",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 1100, 720,
            nullptr, nullptr, app, this);
        if (!hwnd) throw std::runtime_error("Win32 window creation failed");
    }
    void initialize() {
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "Hamun Runtime";
        app.apiVersion = VK_API_VERSION_1_1;
        const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ici.pApplicationInfo = &app;
        ici.enabledExtensionCount = 2;
        ici.ppEnabledExtensionNames = extensions;
        check(vkCreateInstance(&ici, nullptr, &instance), "Vulkan instance creation failed. Check driver.");
        VkWin32SurfaceCreateInfoKHR sci{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        sci.hinstance = hinstance;
        sci.hwnd = hwnd;
        check(vkCreateWin32SurfaceKHR(instance, &sci, nullptr, &surface), "Vulkan Win32 surface failed");
        uint32_t count = 0;
        check(vkEnumeratePhysicalDevices(instance, &count, nullptr), "Enumerating GPUs failed");
        if (!count) throw std::runtime_error("No Vulkan GPU detected. Update your AMD driver.");
        std::vector<VkPhysicalDevice> devices(count);
        check(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "GPU enumeration failed");
        for (auto gpu : devices) {
            uint32_t n = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n, nullptr);
            std::vector<VkQueueFamilyProperties> props(n);
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n, props.data());
            for (uint32_t i = 0; i < n; ++i) {
                VkBool32 present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &present);
                if (!(props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !present) continue;
                uint32_t extCount = 0;
                vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extCount, nullptr);
                std::vector<VkExtensionProperties> exts(extCount);
                vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extCount, exts.data());
                bool hasSwapchain = std::any_of(exts.begin(), exts.end(),
                    [](const auto& e){ return std::string(e.extensionName) == VK_KHR_SWAPCHAIN_EXTENSION_NAME; });
                if (hasSwapchain) { physical = gpu; family = i; break; }
            }
            if (physical) break;
        }
        if (!physical) throw std::runtime_error("No graphics/present queue with VK_KHR_swapchain found");
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physical, &properties);
        std::string title = std::string("Hamun Runtime | Vulkan | ") + properties.deviceName;
        SetWindowTextA(hwnd, title.c_str());
        float priority = 1.0f;
        VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qci.queueFamilyIndex = family;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;
        const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        dci.queueCreateInfoCount = 1;
        dci.pQueueCreateInfos = &qci;
        dci.enabledExtensionCount = 1;
        dci.ppEnabledExtensionNames = deviceExtensions;
        check(vkCreateDevice(physical, &dci, nullptr, &device), "Vulkan logical device failed");
        vkGetDeviceQueue(device, family, 0, &queue);
        VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pci.queueFamilyIndex = family;
        pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        check(vkCreateCommandPool(device, &pci, nullptr, &commandPool), "Command pool failed");
        VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        check(vkCreateSemaphore(device, &sem, nullptr, &imageReady), "Image semaphore failed");
        check(vkCreateSemaphore(device, &sem, nullptr, &renderDone), "Render semaphore failed");
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check(vkCreateFence(device, &fci, nullptr, &frameFence), "Frame fence failed");
        uploadBuffer(mesh.vertices.data(),mesh.vertices.size()*sizeof(HamunVertex),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,vertexBuffer,vertexMemory);
        uploadBuffer(mesh.indices.data(),mesh.indices.size()*sizeof(uint32_t),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,indexBuffer,indexMemory);
        recreate();
        diagnostics.create(hwnd,physical);
    }
    void destroySwapchain() {
        if (!device) return;
        if (!cmds.empty()) vkFreeCommandBuffers(device, commandPool, uint32_t(cmds.size()), cmds.data());
        cmds.clear();
        for (auto f : frames) vkDestroyFramebuffer(device, f, nullptr);
        frames.clear();
        if (depthView) vkDestroyImageView(device, depthView, nullptr);
        depthView=VK_NULL_HANDLE;
        if (depthImage) vkDestroyImage(device, depthImage, nullptr);
        depthImage=VK_NULL_HANDLE;
        if (depthMemory) vkFreeMemory(device, depthMemory, nullptr);
        depthMemory=VK_NULL_HANDLE;
        if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
        if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
        if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
        for (auto v : views) vkDestroyImageView(device, v, nullptr);
        views.clear();
        images.clear();
        if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
    VkShaderModule shader(const char* name) {
        auto bytes = readFile(name);
        if (bytes.empty() || bytes.size() % 4) throw std::runtime_error("Invalid SPIR-V shader");
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        ci.codeSize = bytes.size();
        ci.pCode = reinterpret_cast<const uint32_t*>(bytes.data());
        VkShaderModule result{};
        check(vkCreateShaderModule(device, &ci, nullptr, &result), "Shader module failed");
        return result;
    }
    void recreate() {
        RECT rect{};
        GetClientRect(hwnd, &rect);
        if (rect.right == 0 || rect.bottom == 0) return;
        vkDeviceWaitIdle(device);
        destroySwapchain();
        VkSurfaceCapabilitiesKHR caps{};
        check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps), "Surface caps failed");
        uint32_t n = 0;
        check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &n, nullptr), "Formats failed");
        if (!n) throw std::runtime_error("Surface has no formats");
        std::vector<VkSurfaceFormatKHR> formats(n);
        check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &n, formats.data()), "Formats failed");
        VkSurfaceFormatKHR selected = formats[0];
        for (auto candidate : formats) {
            if (candidate.format == VK_FORMAT_B8G8R8A8_SRGB && candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                selected = candidate; break;
            }
        }
        format = selected.format;
        extent = caps.currentExtent;
        if (extent.width == UINT32_MAX) {
            extent.width = std::clamp(uint32_t(rect.right), caps.minImageExtent.width, caps.maxImageExtent.width);
            extent.height = std::clamp(uint32_t(rect.bottom), caps.minImageExtent.height, caps.maxImageExtent.height);
        }
        uint32_t imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;
        VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        sci.surface = surface;
        sci.minImageCount = imageCount;
        sci.imageFormat = format;
        sci.imageColorSpace = selected.colorSpace;
        sci.imageExtent = extent;
        sci.imageArrayLayers = 1;
        sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        sci.preTransform = caps.currentTransform;
        sci.compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ?
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        // Prefer non-vsync present modes for an uncapped rendering benchmark.
        // MAILBOX may still synchronize presentation to refresh; IMMEDIATE can tear.
        uint32_t modeCount=0;
        check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&modeCount,nullptr),
              "Present mode query failed");
        std::vector<VkPresentModeKHR> available(modeCount);
        check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&modeCount,available.data()),
              "Present modes query failed");
        auto supports=[&](VkPresentModeKHR mode){
            return std::find(available.begin(),available.end(),mode)!=available.end();
        };
        VkPresentModeKHR selectedMode=VK_PRESENT_MODE_FIFO_KHR;
        if(supports(VK_PRESENT_MODE_IMMEDIATE_KHR)) selectedMode=VK_PRESENT_MODE_IMMEDIATE_KHR;
        else if(supports(VK_PRESENT_MODE_MAILBOX_KHR)) selectedMode=VK_PRESENT_MODE_MAILBOX_KHR;
        sci.presentMode=selectedMode;
        presentModeName=(selectedMode==VK_PRESENT_MODE_IMMEDIATE_KHR)?L"IMMEDIATE (uncapped / tearing possible)":
          (selectedMode==VK_PRESENT_MODE_MAILBOX_KHR)?L"MAILBOX (refresh-paced presentation)":
          L"FIFO (VSync fallback)";
        diagnostics.setPresentMode(presentModeName);
        sci.clipped = VK_TRUE;
        check(vkCreateSwapchainKHR(device, &sci, nullptr, &swapchain), "Swapchain failed");
        check(vkGetSwapchainImagesKHR(device, swapchain, &n, nullptr), "Swapchain images failed");
        images.resize(n);
        check(vkGetSwapchainImagesKHR(device, swapchain, &n, images.data()), "Swapchain images failed");
        for (auto im : images) {
            VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            ci.image = im; ci.viewType = VK_IMAGE_VIEW_TYPE_2D; ci.format = format;
            ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ci.subresourceRange.levelCount = 1; ci.subresourceRange.layerCount = 1;
            VkImageView view{};
            check(vkCreateImageView(device, &ci, nullptr, &view), "Image view failed");
            views.push_back(view);
        }
        VkImageCreateInfo di{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        di.imageType=VK_IMAGE_TYPE_2D; di.format=VK_FORMAT_D32_SFLOAT;
        di.extent={extent.width,extent.height,1}; di.mipLevels=1; di.arrayLayers=1;
        di.samples=VK_SAMPLE_COUNT_1_BIT; di.tiling=VK_IMAGE_TILING_OPTIMAL;
        di.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        check(vkCreateImage(device,&di,nullptr,&depthImage),"Depth image failed");
        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(device,depthImage,&req);
        VkPhysicalDeviceMemoryProperties mem{};
        vkGetPhysicalDeviceMemoryProperties(physical,&mem);
        uint32_t type=UINT32_MAX;
        for(uint32_t i=0;i<mem.memoryTypeCount;++i)
          if((req.memoryTypeBits&(1u<<i)) &&
             (mem.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
             {type=i;break;}
        if(type==UINT32_MAX) throw std::runtime_error("No local depth memory");
        VkMemoryAllocateInfo allocMem{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocMem.allocationSize=req.size; allocMem.memoryTypeIndex=type;
        check(vkAllocateMemory(device,&allocMem,nullptr,&depthMemory),"Depth alloc failed");
        check(vkBindImageMemory(device,depthImage,depthMemory,0),"Depth bind failed");
        VkImageViewCreateInfo dvi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        dvi.image=depthImage; dvi.viewType=VK_IMAGE_VIEW_TYPE_2D;
        dvi.format=VK_FORMAT_D32_SFLOAT; dvi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
        dvi.subresourceRange.levelCount=1; dvi.subresourceRange.layerCount=1;
        check(vkCreateImageView(device,&dvi,nullptr,&depthView),"Depth view failed");
        VkAttachmentDescription depth{};
        depth.format=VK_FORMAT_D32_SFLOAT; depth.samples=VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference depthRef{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkAttachmentDescription color{};
        color.format = format; color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference cref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};
        sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &cref;
        sub.pDepthStencilAttachment=&depthRef;
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        VkAttachmentDescription attachments[2]={color,depth};
        rp.attachmentCount=2; rp.pAttachments=attachments;
        rp.subpassCount = 1; rp.pSubpasses = &sub;
        rp.dependencyCount = 1; rp.pDependencies = &dependency;
        check(vkCreateRenderPass(device, &rp, nullptr, &renderPass), "Render pass failed");
        auto vert = shader("shaders/triangle.vert.spv");
        auto frag = shader("shaders/triangle.frag.spv");
        VkPipelineShaderStageCreateInfo stages[2]{};
        for (auto& stage : stages) stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vert; stages[0].pName = "main";
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = frag; stages[1].pName = "main";
        VkVertexInputBindingDescription binding{0,sizeof(HamunVertex),VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attrs[2]{
            {0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(HamunVertex,x)},
            {1,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(HamunVertex,r)}
        };
        VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertex.vertexBindingDescriptionCount=1; vertex.pVertexBindingDescriptions=&binding;
        vertex.vertexAttributeDescriptionCount=2; vertex.pVertexAttributeDescriptions=attrs;
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport{0, 0, float(extent.width), float(extent.height), 0, 1};
        VkRect2D scissor{{0,0},extent};
        VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount=1; viewportState.pViewports=&viewport;
        viewportState.scissorCount=1; viewportState.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode=VK_POLYGON_MODE_FILL; raster.lineWidth=1;
        raster.cullMode=VK_CULL_MODE_NONE; raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;
        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount=1; blend.pAttachments=&blendAttachment;
        VkPushConstantRange cameraRange{};
        cameraRange.stageFlags=VK_SHADER_STAGE_VERTEX_BIT;
        cameraRange.size=sizeof(Push);
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout.pushConstantRangeCount=1;
        layout.pPushConstantRanges=&cameraRange;
        check(vkCreatePipelineLayout(device,&layout,nullptr,&pipelineLayout),"Pipeline layout failed");
        VkGraphicsPipelineCreateInfo pipelineCI{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineCI.stageCount=2; pipelineCI.pStages=stages;
        pipelineCI.pVertexInputState=&vertex;
        pipelineCI.pInputAssemblyState=&assembly;
        pipelineCI.pViewportState=&viewportState;
        pipelineCI.pRasterizationState=&raster;
        pipelineCI.pMultisampleState=&multisample;
        pipelineCI.pColorBlendState=&blend;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        ds.depthTestEnable=VK_TRUE; ds.depthWriteEnable=VK_TRUE;
        ds.depthCompareOp=VK_COMPARE_OP_LESS;
        pipelineCI.pDepthStencilState=&ds;
        pipelineCI.layout=pipelineLayout;
        pipelineCI.renderPass=renderPass;
        VkResult pipelineResult=vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pipelineCI,nullptr,&pipeline);
        vkDestroyShaderModule(device,vert,nullptr);
        vkDestroyShaderModule(device,frag,nullptr);
        check(pipelineResult,"Graphics pipeline failed");
        for (auto view : views) {
            VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            ci.renderPass=renderPass;
            VkImageView attachmentsForFrame[2]={view,depthView};
            ci.attachmentCount=2; ci.pAttachments=attachmentsForFrame;
            ci.width=extent.width; ci.height=extent.height; ci.layers=1;
            VkFramebuffer framebuffer{};
            check(vkCreateFramebuffer(device,&ci,nullptr,&framebuffer),"Framebuffer failed");
            frames.push_back(framebuffer);
        }
        cmds.resize(frames.size());
        VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc.commandPool=commandPool; alloc.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount=uint32_t(cmds.size());
        check(vkAllocateCommandBuffers(device,&alloc,cmds.data()),"Command buffer allocation failed");
        resized=false;
    }
    void draw() {
        const auto frameStart=std::chrono::steady_clock::now();
        if (GetAsyncKeyState(VK_LEFT)&0x8000) yaw-=0.015f;
        if (GetAsyncKeyState(VK_RIGHT)&0x8000) yaw+=0.015f;
        if (GetAsyncKeyState(VK_UP)&0x8000) pitch=std::min(pitch+0.015f,1.3f);
        if (GetAsyncKeyState(VK_DOWN)&0x8000) pitch=std::max(pitch-0.015f,-1.3f);
        if (GetAsyncKeyState('W')&0x8000) distance=std::max(2.8f,distance-0.04f);
        if (GetAsyncKeyState('S')&0x8000) distance=std::min(18.0f,distance+0.04f);
        if (resized) { recreate(); return; }
        if (!swapchain) { recreate(); return; }
        check(vkWaitForFences(device,1,&frameFence,VK_TRUE,UINT64_MAX),"Fence wait failed");
        uint32_t index=0;
        VkResult result=vkAcquireNextImageKHR(device,swapchain,UINT64_MAX,imageReady,VK_NULL_HANDLE,&index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) { recreate(); return; }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            throw std::runtime_error("Acquire image failed");
        check(vkResetCommandBuffer(cmds[index],0),"Command reset failed");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        check(vkBeginCommandBuffer(cmds[index],&begin),"Command begin failed");
        VkClearValue clears[2]{};
        clears[0].color={{0.015f,0.025f,0.045f,1.0f}};
        clears[1].depthStencil={1.0f,0};
        VkRenderPassBeginInfo rb{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rb.renderPass=renderPass; rb.framebuffer=frames[index];
        rb.renderArea.extent=extent; rb.clearValueCount=2; rb.pClearValues=clears;
        vkCmdBeginRenderPass(cmds[index],&rb,VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmds[index],VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        VkDeviceSize offset=0;
        vkCmdBindVertexBuffers(cmds[index],0,1,&vertexBuffer,&offset);
        vkCmdBindIndexBuffer(cmds[index],indexBuffer,0,VK_INDEX_TYPE_UINT32);
        Push params{};
        params.camera[0]=yaw; params.camera[1]=pitch;
        params.camera[2]=distance; params.camera[3]=float(extent.width)/float(extent.height);
        const float objects[4][4]={{-3.6f,0,0,0.87f},{-1.2f,0,0,0.87f},{1.2f,0,0,0.87f},{3.6f,0,0,0.87f}};
        static const auto animationStart=std::chrono::steady_clock::now();
        const float seconds=std::chrono::duration<float>(std::chrono::steady_clock::now()-animationStart).count();
        for(int material=0;material<4;++material){
            const auto& obj=objects[material];
            std::memcpy(params.object,obj,sizeof(obj));
            params.material[0]=float(material);
            params.material[1]=seconds;
            vkCmdPushConstants(cmds[index],pipelineLayout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(Push),&params);
            vkCmdDrawIndexed(cmds[index],static_cast<uint32_t>(mesh.indices.size()),1,0,0,0);
        }
        vkCmdEndRenderPass(cmds[index]);
        check(vkEndCommandBuffer(cmds[index]),"Command end failed");
        check(vkResetFences(device,1,&frameFence),"Fence reset failed");
        VkPipelineStageFlags stages[]={VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount=1; submit.pWaitSemaphores=&imageReady;
        submit.pWaitDstStageMask=stages;
        submit.commandBufferCount=1; submit.pCommandBuffers=&cmds[index];
        submit.signalSemaphoreCount=1; submit.pSignalSemaphores=&renderDone;
        check(vkQueueSubmit(queue,1,&submit,frameFence),"Queue submit failed");
        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount=1; present.pWaitSemaphores=&renderDone;
        present.swapchainCount=1; present.pSwapchains=&swapchain; present.pImageIndices=&index;
        result=vkQueuePresentKHR(queue,&present);
        // Single frame in flight: wait before reusing renderDone semaphore.
        const auto waitStart=std::chrono::steady_clock::now();
        check(vkQueueWaitIdle(queue),"Queue idle failed");
        const auto waitEnd=std::chrono::steady_clock::now();
        const double cpuMs=std::chrono::duration<double,std::milli>(waitEnd-frameStart).count();
        const double waitMs=std::chrono::duration<double,std::milli>(waitEnd-waitStart).count();
        diagnostics.frame(cpuMs,waitMs);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resized) recreate();
        else check(result,"Present failed");
    }
    void shutdown() {
        diagnostics.shutdown();
        if (device) vkDeviceWaitIdle(device);
        destroySwapchain();
        if (device) {
            if(vertexBuffer) vkDestroyBuffer(device,vertexBuffer,nullptr);
            if(vertexMemory) vkFreeMemory(device,vertexMemory,nullptr);
            if(indexBuffer) vkDestroyBuffer(device,indexBuffer,nullptr);
            if(indexMemory) vkFreeMemory(device,indexMemory,nullptr);
            if (frameFence) vkDestroyFence(device,frameFence,nullptr);
            if (imageReady) vkDestroySemaphore(device,imageReady,nullptr);
            if (renderDone) vkDestroySemaphore(device,renderDone,nullptr);
            if (commandPool) vkDestroyCommandPool(device,commandPool,nullptr);
            vkDestroyDevice(device,nullptr);
            device=VK_NULL_HANDLE;
        }
        if (surface && instance) vkDestroySurfaceKHR(instance,surface,nullptr);
        if (instance) vkDestroyInstance(instance,nullptr);
        surface=VK_NULL_HANDLE; instance=VK_NULL_HANDLE;
        if (hwnd && IsWindow(hwnd)) DestroyWindow(hwnd);
        hwnd=nullptr;
    }
};
int WINAPI wWinMain(HINSTANCE app,HINSTANCE,LPWSTR,int) {
    Runtime runtime;
    try {
        // Shader paths are resolved relative to the executable, not current CMD folder.
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(nullptr,path,MAX_PATH);
        std::wstring executable(path);
        auto slash=executable.find_last_of(L"\\/");
        if(slash!=std::wstring::npos) SetCurrentDirectoryW(executable.substr(0,slash).c_str());
        runtime.window(app);
        runtime.initialize();
        MSG msg{};
        while(runtime.running) {
            while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
                if(msg.message == WM_QUIT) runtime.running=false;
                TranslateMessage(&msg); DispatchMessageW(&msg);
            }
            if(!runtime.running) break;
            if(IsIconic(runtime.hwnd)) { WaitMessage(); continue; }
            runtime.draw();
        }
        runtime.shutdown();
        return 0;
    } catch(const std::exception& ex) {
        runtime.shutdown();
        MessageBoxA(nullptr,ex.what(),"Hamun Vulkan Runtime Error",MB_OK|MB_ICONERROR);
        return 1;
    }
}
