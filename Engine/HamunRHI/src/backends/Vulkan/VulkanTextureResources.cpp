#include "VulkanTextureResources.hpp"

#include <cstring>
#include <limits>
#include <vector>

namespace Hamun::RHI::VulkanDetail {
namespace {

VkFormat ToFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_UNorm: return VK_FORMAT_R8_UNORM;
        case TextureFormat::RGBA8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RG16_Float: return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::RGBA16_Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::R32_Float: return VK_FORMAT_R32_SFLOAT;
    }
    return VK_FORMAT_UNDEFINED;
}

std::uint32_t BytesPerPixel(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_UNorm: return 1;
        case TextureFormat::RGBA8_UNorm: return 4;
        case TextureFormat::RG16_Float: return 4;
        case TextureFormat::RGBA16_Float: return 8;
        case TextureFormat::R32_Float: return 4;
    }
    return 0;
}

bool MemoryType(VkPhysicalDevice physicalDevice, std::uint32_t typeBits,
                VkMemoryPropertyFlags requested, std::uint32_t& selected) {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
    for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) != 0 &&
            (properties.memoryTypes[i].propertyFlags & requested) == requested) {
            selected = i;
            return true;
        }
    }
    return false;
}

bool AllocateMemory(VkDevice device, VkPhysicalDevice physicalDevice,
                    const VkMemoryRequirements& requirements,
                    VkMemoryPropertyFlags flags, VkDeviceMemory& memory) {
    std::uint32_t index = 0;
    if (!MemoryType(physicalDevice, requirements.memoryTypeBits, flags, index))
        return false;
    VkMemoryAllocateInfo info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    info.allocationSize = requirements.size;
    info.memoryTypeIndex = index;
    return vkAllocateMemory(device, &info, nullptr, &memory) == VK_SUCCESS;
}

bool Upload(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool pool,
            VkQueue queue, VkImage image, const TextureDesc& desc,
            std::uint32_t pixelBytes) {
    const VkDeviceSize tightPitch =
        static_cast<VkDeviceSize>(desc.width) * pixelBytes;
    const VkDeviceSize suppliedPitch = desc.rowPitch ? desc.rowPitch : tightPitch;
    if (suppliedPitch < tightPitch)
        return false;
    const VkDeviceSize byteCount = tightPitch * desc.height;
    if (byteCount == 0 || !desc.initialData || pool == VK_NULL_HANDLE ||
        queue == VK_NULL_HANDLE)
        return false;

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    bool success = false;

    do {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = byteCount;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &staging) != VK_SUCCESS)
            break;

        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, staging, &requirements);
        if (!AllocateMemory(device, physicalDevice, requirements,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            stagingMemory))
            break;
        if (vkBindBufferMemory(device, staging, stagingMemory, 0) != VK_SUCCESS)
            break;

        void* mapped = nullptr;
        if (vkMapMemory(device, stagingMemory, 0, byteCount, 0, &mapped)
            != VK_SUCCESS)
            break;

        const auto* source = static_cast<const std::uint8_t*>(desc.initialData);
        auto* destination = static_cast<std::uint8_t*>(mapped);
        for (std::uint32_t y = 0; y < desc.height; ++y) {
            std::memcpy(destination + y * tightPitch,
                        source + y * suppliedPitch,
                        static_cast<std::size_t>(tightPitch));
        }
        vkUnmapMemory(device, stagingMemory);

        VkCommandBufferAllocateInfo allocate{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(device, &allocate, &command) != VK_SUCCESS)
            break;

        VkCommandBufferBeginInfo begin{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(command, &begin) != VK_SUCCESS)
            break;

        VkImageMemoryBarrier toTransfer{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = image;
        toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.levelCount = 1;
        toTransfer.subresourceRange.layerCount = 1;
        toTransfer.srcAccessMask = 0;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &toTransfer);

        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {desc.width, desc.height, 1};
        vkCmdCopyBufferToImage(command, staging, image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1, &region);

        VkImageMemoryBarrier toShader{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toShader.image = image;
        toShader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toShader.subresourceRange.levelCount = 1;
        toShader.subresourceRange.layerCount = 1;
        toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &toShader);

        if (vkEndCommandBuffer(command) != VK_SUCCESS)
            break;
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS)
            break;

        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        if (vkQueueSubmit(queue, 1, &submit, fence) != VK_SUCCESS)
            break;
        // Must finish before freeing staging memory or resetting this pool.
        if (vkWaitForFences(device, 1, &fence, VK_TRUE,
                            std::numeric_limits<std::uint64_t>::max())
            != VK_SUCCESS) {
            // Submission may still be in flight; preserve resource lifetime.
            vkQueueWaitIdle(queue);
            break;
        }
        success = true;
    } while (false);

    if (fence != VK_NULL_HANDLE)
        vkDestroyFence(device, fence, nullptr);
    if (command != VK_NULL_HANDLE)
        vkFreeCommandBuffers(device, pool, 1, &command);
    if (staging != VK_NULL_HANDLE)
        vkDestroyBuffer(device, staging, nullptr);
    if (stagingMemory != VK_NULL_HANDLE)
        vkFreeMemory(device, stagingMemory, nullptr);
    return success;
}

} // namespace

VulkanTexture::VulkanTexture(VkDevice device, VkImage image,
                             VkDeviceMemory memory, VkImageView view,
                             TextureDesc desc) noexcept
    : device_(device), image_(image), memory_(memory), view_(view), desc_(desc) {
    desc_.initialData = nullptr; // never retain caller-owned CPU memory
}

VulkanTexture::~VulkanTexture() {
    if (view_) vkDestroyImageView(device_, view_, nullptr);
    if (image_) vkDestroyImage(device_, image_, nullptr);
    if (memory_) vkFreeMemory(device_, memory_, nullptr);
}

VulkanSampler::VulkanSampler(VkDevice device, VkSampler sampler) noexcept
    : device_(device), sampler_(sampler) {}

VulkanSampler::~VulkanSampler() {
    if (sampler_) vkDestroySampler(device_, sampler_, nullptr);
}

std::unique_ptr<VulkanTexture> CreateTexture(
    VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool pool,
    VkQueue queue, const TextureDesc& desc) {
    if (!device || !physicalDevice || !desc.width || !desc.height ||
        desc.usage == TextureUsage::None ||
        HasTextureUsage(desc.usage, TextureUsage::DepthStencil))
        return {};
    const VkFormat format = ToFormat(desc.format);
    const std::uint32_t pixelBytes = BytesPerPixel(desc.format);
    if (format == VK_FORMAT_UNDEFINED || pixelBytes == 0)
        return {};
    if (desc.width > std::numeric_limits<VkDeviceSize>::max() /
                     pixelBytes / desc.height)
        return {};
    if (desc.initialData && !HasTextureUsage(
            desc.usage, TextureUsage::ShaderResource))
        return {};

    VkImageUsageFlags usage = 0;
    if (HasTextureUsage(desc.usage, TextureUsage::ShaderResource))
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (HasTextureUsage(desc.usage, TextureUsage::Storage))
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (HasTextureUsage(desc.usage, TextureUsage::RenderTarget))
        usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (desc.initialData)
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    bool success = false;
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    do {
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = {desc.width, desc.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
            break;

        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device, image, &requirements);
        if (!AllocateMemory(device, physicalDevice, requirements,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memory))
            break;
        if (vkBindImageMemory(device, image, memory, 0) != VK_SUCCESS)
            break;

        if (desc.initialData) {
            if (!Upload(device, physicalDevice, pool, queue, image, desc,
                        pixelBytes))
                break;
            finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        VkImageViewCreateInfo viewInfo{
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS)
            break;
        success = true;
    } while (false);

    if (!success) {
        if (view) vkDestroyImageView(device, view, nullptr);
        if (image) vkDestroyImage(device, image, nullptr);
        if (memory) vkFreeMemory(device, memory, nullptr);
        return {};
    }

    auto texture = std::make_unique<VulkanTexture>(
        device, image, memory, view, desc);
    texture->SetLayout(finalLayout);
    return texture;
}

std::unique_ptr<VulkanSampler> CreateSampler(
    VkDevice device, const SamplerDesc& desc) {
    if (!device) return {};
    const VkFilter filter = desc.filter == SamplerFilter::Nearest
        ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    const auto address = [](SamplerAddressMode value) {
        return value == SamplerAddressMode::Clamp
            ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
            : VK_SAMPLER_ADDRESS_MODE_REPEAT;
    };

    VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    info.magFilter = filter;
    info.minFilter = filter;
    info.mipmapMode = desc.filter == SamplerFilter::Nearest
        ? VK_SAMPLER_MIPMAP_MODE_NEAREST
        : VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.addressModeU = address(desc.addressU);
    info.addressModeV = address(desc.addressV);
    info.addressModeW = address(desc.addressW);
    info.anisotropyEnable = VK_FALSE;
    info.compareEnable = VK_FALSE;
    info.minLod = 0.0f;
    info.maxLod = 0.0f; // single mip level
    info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

    VkSampler sampler = VK_NULL_HANDLE;
    if (vkCreateSampler(device, &info, nullptr, &sampler) != VK_SUCCESS)
        return {};
    return std::make_unique<VulkanSampler>(device, sampler);
}

} // namespace Hamun::RHI::VulkanDetail
