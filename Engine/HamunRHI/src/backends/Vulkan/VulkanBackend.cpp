#include <Hamun/Core/Log.hpp>
#include <Hamun/RHI/RHI.hpp>

#if defined(HAMUN_ENABLE_VULKAN)
#include "VulkanTextureResources.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#endif

namespace Hamun::RHI {

class VulkanBackend final : public IBackend {
public:
    std::string_view Name() const noexcept override { return "Vulkan"; }
    BackendType Type() const noexcept override { return BackendType::Vulkan; }
    const Capabilities& Caps() const noexcept override { return caps_; }
    const AdapterInfo& Adapter() const noexcept override { return adapter_; }

    bool Initialize(const BackendCreateInfo&) override {
#if defined(HAMUN_ENABLE_VULKAN)
        if (device_ != VK_NULL_HANDLE)
            return true;

        VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        application.pApplicationName = "Hamun Engine";
        application.apiVersion = VK_API_VERSION_1_1;

        VkInstanceCreateInfo instanceInfo{
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instanceInfo.pApplicationInfo = &application;
        if (vkCreateInstance(&instanceInfo, nullptr, &instance_) != VK_SUCCESS) {
            Core::Log(Core::LogLevel::Error, "Vulkan instance creation failed.");
            return false;
        }

        std::uint32_t deviceCount = 0;
        if (vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr)
                != VK_SUCCESS || deviceCount == 0) {
            Shutdown();
            return false;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        if (vkEnumeratePhysicalDevices(instance_, &deviceCount,
                                        devices.data()) != VK_SUCCESS) {
            Shutdown();
            return false;
        }

        for (VkPhysicalDevice candidate : devices) {
            std::uint32_t count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count,
                                                     families.data());
            for (std::uint32_t i = 0; i < count; ++i) {
                if (families[i].queueCount &&
                    (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                    physicalDevice_ = candidate;
                    queueFamily_ = i;
                    break;
                }
            }
            if (physicalDevice_) break;
        }
        if (!physicalDevice_) {
            Shutdown();
            return false;
        }

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
        adapter_.name = properties.deviceName;
        adapter_.vendorId = properties.vendorID;
        adapter_.deviceId = properties.deviceID;

        constexpr float priority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo{
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex = queueFamily_;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;

        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        if (vkCreateDevice(physicalDevice_, &deviceInfo, nullptr,
                           &device_) != VK_SUCCESS) {
            Shutdown();
            return false;
        }

        vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);
        VkCommandPoolCreateInfo poolInfo{
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.queueFamilyIndex = queueFamily_;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                         VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(device_, &poolInfo, nullptr,
                                 &uploadPool_) != VK_SUCCESS) {
            Shutdown();
            return false;
        }

        Core::Log(Core::LogLevel::Info,
            "Vulkan device and texture upload queue initialized. "
            "Swapchain and graphics pipeline remain pending.");
        return true;
#else
        return false;
#endif
    }

    void Shutdown() override {
#if defined(HAMUN_ENABLE_VULKAN)
        // All texture and sampler resources must be released by the caller
        // before shutting down the device.
        if (device_) {
            vkDeviceWaitIdle(device_);
            if (uploadPool_)
                vkDestroyCommandPool(device_, uploadPool_, nullptr);
            vkDestroyDevice(device_, nullptr);
        }
        if (instance_) vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
        device_ = VK_NULL_HANDLE;
        physicalDevice_ = VK_NULL_HANDLE;
        queue_ = VK_NULL_HANDLE;
        uploadPool_ = VK_NULL_HANDLE;
#endif
        caps_ = {};
        adapter_ = {};
    }

    void* NativeDeviceHandle() noexcept override {
#if defined(HAMUN_ENABLE_VULKAN)
        return reinterpret_cast<void*>(device_);
#else
        return nullptr;
#endif
    }
    void* NativeCommandQueueHandle() noexcept override {
#if defined(HAMUN_ENABLE_VULKAN)
        return reinterpret_cast<void*>(queue_);
#else
        return nullptr;
#endif
    }

    std::unique_ptr<IBuffer> CreateBuffer(const BufferDesc&) override {
        return {};
    }

    std::unique_ptr<ITexture> CreateTexture(const TextureDesc& desc) override {
#if defined(HAMUN_ENABLE_VULKAN)
        if (!device_) return {};
        return VulkanDetail::CreateTexture(device_, physicalDevice_,
                                            uploadPool_, queue_, desc);
#else
        (void)desc;
        return {};
#endif
    }

    std::unique_ptr<ISampler> CreateSampler(const SamplerDesc& desc) override {
#if defined(HAMUN_ENABLE_VULKAN)
        if (!device_) return {};
        return VulkanDetail::CreateSampler(device_, desc);
#else
        (void)desc;
        return {};
#endif
    }

    std::unique_ptr<IShader> CreateShader(const ShaderDesc&) override {
        return {};
    }
    std::unique_ptr<IPipeline> CreateGraphicsPipeline(
        const GraphicsPipelineDesc&) override { return {}; }

    ICommandList* BeginFrame() override { return nullptr; }
    bool SubmitFrame() override { return false; }
    ISwapChain* SwapChain() noexcept override { return nullptr; }
    IFence* FrameFence() noexcept override { return nullptr; }

private:
    Capabilities caps_{};
    AdapterInfo adapter_{};
#if defined(HAMUN_ENABLE_VULKAN)
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    VkCommandPool uploadPool_ = VK_NULL_HANDLE;
    std::uint32_t queueFamily_ = 0;
#endif
};

std::unique_ptr<IBackend> CreateVulkanBackend() {
    return std::make_unique<VulkanBackend>();
}

} // namespace Hamun::RHI
