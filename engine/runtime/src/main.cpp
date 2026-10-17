// SPDX-License-Identifier: GPL-2.0-or-later
// Hamun Runtime 0.1 - native Win32 + Vulkan first triangle.
#include <windows.h>
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
    VkPipelineLayout pipelineLayout{};
    VkPipeline pipeline{};
    VkCommandPool commandPool{};
    std::vector<VkCommandBuffer> cmds;
    VkSemaphore imageReady{}, renderDone{};
    VkFence frameFence{};

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
        hwnd = CreateWindowExW(0, cls.lpszClassName, L"Hamun Runtime - Vulkan 0.1",
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
        recreate();
    }
    void destroySwapchain() {
        if (!device) return;
        if (!cmds.empty()) vkFreeCommandBuffers(device, commandPool, uint32_t(cmds.size()), cmds.data());
        cmds.clear();
        for (auto f : frames) vkDestroyFramebuffer(device, f, nullptr);
        frames.clear();
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
        sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
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
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rp.attachmentCount = 1; rp.pAttachments = &color;
        rp.subpassCount = 1; rp.pSubpasses = &sub;
        rp.dependencyCount = 1; rp.pDependencies = &dependency;
        check(vkCreateRenderPass(device, &rp, nullptr, &renderPass), "Render pass failed");
        auto vert = shader("shaders/triangle.vert.spv");
        auto frag = shader("shaders/triangle.frag.spv");
        VkPipelineShaderStageCreateInfo stages[2]{};
        for (auto& stage : stages) stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vert; stages[0].pName = "main";
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = frag; stages[1].pName = "main";
        VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
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
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        check(vkCreatePipelineLayout(device,&layout,nullptr,&pipelineLayout),"Pipeline layout failed");
        VkGraphicsPipelineCreateInfo pipelineCI{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineCI.stageCount=2; pipelineCI.pStages=stages;
        pipelineCI.pVertexInputState=&vertex;
        pipelineCI.pInputAssemblyState=&assembly;
        pipelineCI.pViewportState=&viewportState;
        pipelineCI.pRasterizationState=&raster;
        pipelineCI.pMultisampleState=&multisample;
        pipelineCI.pColorBlendState=&blend;
        pipelineCI.layout=pipelineLayout;
        pipelineCI.renderPass=renderPass;
        VkResult pipelineResult=vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pipelineCI,nullptr,&pipeline);
        vkDestroyShaderModule(device,vert,nullptr);
        vkDestroyShaderModule(device,frag,nullptr);
        check(pipelineResult,"Graphics pipeline failed");
        for (auto view : views) {
            VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            ci.renderPass=renderPass;
            ci.attachmentCount=1; ci.pAttachments=&view;
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
        for (size_t i=0;i<cmds.size();++i) {
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            check(vkBeginCommandBuffer(cmds[i],&begin),"Command buffer begin failed");
            VkClearValue clear{}; clear.color={{0.015f,0.025f,0.045f,1.0f}};
            VkRenderPassBeginInfo rpBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            rpBegin.renderPass=renderPass; rpBegin.framebuffer=frames[i];
            rpBegin.renderArea.extent=extent; rpBegin.clearValueCount=1; rpBegin.pClearValues=&clear;
            vkCmdBeginRenderPass(cmds[i],&rpBegin,VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cmds[i],VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
            vkCmdDraw(cmds[i],3,1,0,0);
            vkCmdEndRenderPass(cmds[i]);
            check(vkEndCommandBuffer(cmds[i]),"Command buffer end failed");
        }
        resized=false;
    }
    void draw() {
        if (resized) { recreate(); return; }
        if (!swapchain) { recreate(); return; }
        check(vkWaitForFences(device,1,&frameFence,VK_TRUE,UINT64_MAX),"Fence wait failed");
        uint32_t index=0;
        VkResult result=vkAcquireNextImageKHR(device,swapchain,UINT64_MAX,imageReady,VK_NULL_HANDLE,&index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) { recreate(); return; }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            throw std::runtime_error("Acquire image failed");
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
        check(vkQueueWaitIdle(queue),"Queue idle failed");
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resized) recreate();
        else check(result,"Present failed");
    }
    void shutdown() {
        if (device) vkDeviceWaitIdle(device);
        destroySwapchain();
        if (device) {
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
