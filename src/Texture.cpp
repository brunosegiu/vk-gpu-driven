#include "Texture.h"

#include "DebugUtils.h"
#include "Device.h"
#include "VulkanBuffer.h"

namespace VKRT {

Texture::Texture(
    ScopedRefPtr<Context> context,
    uint32_t width,
    uint32_t height,
    uint32_t layers,
    vk::Format format,
    vk::ImageUsageFlags usageFlags,
    vk::ImageLayout initialLayout,
    vk::Image image)
    : mContext(context),
      mImage(image),
      mOwnsImage(true),
      mWidth(width),
      mHeight(height),
      mLayers(layers),
      mFormat(format) {
    mOwnsImage = !image;

    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();

    if (mOwnsImage) {
        vk::ImageCreateInfo imageCreateInfo = vk::ImageCreateInfo()
                                                  .setImageType(vk::ImageType::e2D)
                                                  .setFormat(format)
                                                  .setExtent(vk::Extent3D{width, height, 1})
                                                  .setMipLevels(1)
                                                  .setArrayLayers(layers)
                                                  .setSamples(vk::SampleCountFlagBits::e1)
                                                  .setTiling(vk::ImageTiling::eOptimal)
                                                  .setUsage(usageFlags)
                                                  .setInitialLayout(vk::ImageLayout::eUndefined);
        VmaAllocator allocator = context->GetDevice()->GetAllocator();

        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        allocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        VmaAllocationInfo allocInfo{};

        VKRT_ASSERT_VK(vmaCreateImage(
            allocator,
            (VkImageCreateInfo*)&imageCreateInfo,
            &allocationInfo,
            (VkImage*)&mImage,
            &mAllocation,
            &allocInfo));
    }

    mImageAspect = bool(usageFlags & vk::ImageUsageFlagBits::eDepthStencilAttachment)
                       ? vk::ImageAspectFlagBits::eDepth
                       : vk::ImageAspectFlagBits::eColor;

    vk::ImageViewCreateInfo imageViewCreateInfo =
        vk::ImageViewCreateInfo()
            .setViewType(layers == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray)
            .setFormat(format)
            .setSubresourceRange(vk::ImageSubresourceRange()
                                     .setAspectMask(mImageAspect)
                                     .setBaseMipLevel(0)
                                     .setLevelCount(1)
                                     .setBaseArrayLayer(0)
                                     .setLayerCount(layers))
            .setImage(mImage);

    mImageView = VKRT_ASSERT_VK(logicalDevice.createImageView(imageViewCreateInfo));

    if (initialLayout != vk::ImageLayout::eUndefined) {
        vk::CommandBuffer commandBuffer = mContext->GetDevice()->CreateCommandBuffer();
        VKRT_ASSERT_VK(commandBuffer.begin(vk::CommandBufferBeginInfo{}));

        const vk::ImageSubresourceRange subresourceRange =
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

        SetImageLayout(
            commandBuffer,
            vk::ImageLayout::eUndefined,
            initialLayout,
            vk::PipelineStageFlagBits::eAllCommands,
            vk::PipelineStageFlagBits::eAllCommands);

        VKRT_ASSERT_VK(commandBuffer.end());
        mContext->GetDevice()->SubmitCommandAndFlush(commandBuffer);
        mContext->GetDevice()->DestroyCommand(commandBuffer);
    }
}

Texture::Texture(
    ScopedRefPtr<Context> context,
    uint32_t width,
    uint32_t height,
    vk::Format format,
    vk::ImageUsageFlags usageFlags,
    vk::ImageLayout initialLayout,
    vk::Image image)
    : Texture(context, width, height, 1, format, usageFlags, initialLayout, image) {}

Texture::Texture(
    ScopedRefPtr<Context> context,
    uint32_t width,
    uint32_t height,
    vk::Format format,
    const uint8_t* buffer,
    size_t bufferSize)
    : Texture(
          context,
          width,
          height,
          format,
          vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled) {
    ScopedRefPtr<VulkanBuffer> stagingBuffer = mContext->GetDevice()->CreateBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    uint8_t* stagingData = stagingBuffer->MapBuffer();
    std::copy_n(buffer, bufferSize, stagingData);
    stagingBuffer->UnmapBuffer();

    vk::CommandBuffer commandBuffer = mContext->GetDevice()->CreateCommandBuffer();
    VKRT_ASSERT_VK(commandBuffer.begin(vk::CommandBufferBeginInfo{}));

    const vk::ImageSubresourceRange subresourceRange =
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    SetImageLayout(
        commandBuffer,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits::eAllCommands,
        vk::PipelineStageFlagBits::eAllCommands);

    vk::BufferImageCopy imageCopy =
        vk::BufferImageCopy()
            .setImageSubresource(
                vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
            .setImageExtent(vk::Extent3D{width, height, 1})
            .setBufferOffset(0);

    commandBuffer.copyBufferToImage(
        stagingBuffer->GetBufferHandle(),
        mImage,
        vk::ImageLayout::eTransferDstOptimal,
        imageCopy);

    SetImageLayout(
        commandBuffer,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::PipelineStageFlagBits::eAllCommands,
        vk::PipelineStageFlagBits::eAllCommands);

    VKRT_ASSERT_VK(commandBuffer.end());
    mContext->GetDevice()->SubmitCommandAndFlush(commandBuffer);
    mContext->GetDevice()->DestroyCommand(commandBuffer);
}

static vk::AccessFlags GetAccessForLayout(
    vk::ImageLayout layout,
    vk::PipelineStageFlags stageFlags) {
    switch (layout) {
        case vk::ImageLayout::eUndefined:
            return {};
        case vk::ImageLayout::eGeneral:
            return vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
        case vk::ImageLayout::eTransferSrcOptimal:
            return vk::AccessFlagBits::eTransferRead;
        case vk::ImageLayout::eTransferDstOptimal:
            return vk::AccessFlagBits::eTransferWrite;
        case vk::ImageLayout::eShaderReadOnlyOptimal: {
            vk::AccessFlags flags = vk::AccessFlagBits::eInputAttachmentRead;
            if (stageFlags & vk::PipelineStageFlagBits::eRayTracingShaderKHR ||
                stageFlags & vk::PipelineStageFlagBits::eComputeShader) {
                flags = vk::AccessFlags{};
            }
            return vk::AccessFlagBits::eShaderRead | flags;
        }
        case vk::ImageLayout::eColorAttachmentOptimal:
            return vk::AccessFlagBits::eColorAttachmentRead |
                   vk::AccessFlagBits::eColorAttachmentWrite;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            return vk::AccessFlagBits::eDepthStencilAttachmentRead |
                   vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
            return vk::AccessFlagBits::eDepthStencilAttachmentRead;
        case vk::ImageLayout::ePresentSrcKHR:
            return {};
        default:
            return vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
    }
}

void Texture::SetImageLayout(
    vk::CommandBuffer& commandBuffer,
    vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout,
    vk::PipelineStageFlags srcStage,
    vk::PipelineStageFlags dstStage) {
    const vk::ImageSubresourceRange subresourceRange =
        vk::ImageSubresourceRange(mImageAspect, 0, 1, 0, mLayers);
    vk::ImageMemoryBarrier imageBarrier = vk::ImageMemoryBarrier()
                                              .setOldLayout(oldLayout)
                                              .setNewLayout(newLayout)
                                              .setImage(mImage)
                                              .setSubresourceRange(subresourceRange);

    imageBarrier.setSrcAccessMask(GetAccessForLayout(oldLayout, srcStage));
    imageBarrier.setDstAccessMask(GetAccessForLayout(newLayout, dstStage));

    commandBuffer.pipelineBarrier(srcStage, dstStage, {}, {}, {}, imageBarrier);
}

std::vector<vk::ImageMemoryBarrier> Texture::GetBarriers(
    vk::PipelineStageFlags srcStage,
    vk::PipelineStageFlags dstStage,
    std::vector<ImageBarrierInfo> textures) {
    std::vector<vk::ImageMemoryBarrier> barriers;
    barriers.reserve(textures.size());
    for (const ImageBarrierInfo& textureInfo : textures) {
        barriers.push_back(
            textureInfo.texture->GetImageBarrierInfo(textureInfo.srcLayout, textureInfo.dstLayout, srcStage, dstStage));
    }
    return barriers;
}

vk::ImageMemoryBarrier Texture::GetImageBarrierInfo(
    vk::ImageLayout srcLayout,
    vk::ImageLayout dstLayout,
    vk::PipelineStageFlags srcStage,
    vk::PipelineStageFlags dstStage) {
    return vk::ImageMemoryBarrier()
        .setImage(GetImage())
        .setOldLayout(srcLayout)
        .setNewLayout(dstLayout)
        .setSrcAccessMask(GetAccessForLayout(srcLayout, srcStage))
        .setDstAccessMask(GetAccessForLayout(dstLayout, dstStage))
        .setSubresourceRange(vk::ImageSubresourceRange{}
                                 .setAspectMask(mImageAspect)
                                 .setLevelCount(1)
                                 .setLayerCount(mLayers));
}

Texture::~Texture() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    logicalDevice.destroyImageView(mImageView);
    if (mOwnsImage) {
        VmaAllocator allocator = mContext->GetDevice()->GetAllocator();
        vmaDestroyImage(allocator, mImage, mAllocation);
    }
}

}  // namespace VKRT