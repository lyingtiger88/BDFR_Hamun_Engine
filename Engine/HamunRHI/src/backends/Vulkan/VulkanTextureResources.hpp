#pragma once

#include <Hamun/RHI/RHI.hpp>
#include <vulkan/vulkan.h>

#include <memory>

namespace Hamun::RHI::VulkanDetail {

// All Vulkan handles are owned by these objects. The Vulkan device must
// outlive every texture and sampler created from it.
class VulkanTexture final : public ITexture {
public:
    VulkanTexture(VkDevice device, VkImage image, VkDeviceMemory memory,
                  VkImageView view, TextureDesc desc) noexcept;
    ~VulkanTexture() override;

    VulkanTexture(const VulkanTexture&) = delete;
    VulkanTexture& operator=(const VulkanTexture&) = delete;

    std::uint32_t Width() const noexcept override { return desc_.width; }
    std::uint32_t Height() const noexcept override { return desc_.height; }
    TextureFormat Format() const noexcept override { return desc_.format; }
    TextureUsage Usage() const noexcept override { return desc_.usage; }
    void* NativeResourceHandle() noexcept override {
        return reinterpret_cast<void*>(image_);
    }

    VkImage Image() const noexcept { return image_; }
    VkImageView View() const noexcept { return view_; }
    VkImageLayout Layout() const noexcept { return layout_; }
    void SetLayout(VkImageLayout value) noexcept { layout_ = value; }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkImage image_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
    VkImageLayout layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    TextureDesc desc_{};
};

class VulkanSampler final : public ISampler {
public:
    VulkanSampler(VkDevice device, VkSampler sampler) noexcept;
    ~VulkanSampler() override;
    VulkanSampler(const VulkanSampler&) = delete;
    VulkanSampler& operator=(const VulkanSampler&) = delete;
    VkSampler Handle() const noexcept { return sampler_; }
private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
};

// Creates and uploads a texture with a tightly packed VkImage.
// initialData is copied synchronously; its memory can be released on return.
// The supplied command pool must belong to the family of the supplied queue.
// DepthStencil textures are intentionally rejected until depth-specific
// formats, aspects and layout transitions are supported.
std::unique_ptr<VulkanTexture> CreateTexture(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool uploadPool,
    VkQueue uploadQueue, const TextureDesc& desc);

std::unique_ptr<VulkanSampler> CreateSampler(
    VkDevice device, const SamplerDesc& desc);

} // namespace Hamun::RHI::VulkanDetail
