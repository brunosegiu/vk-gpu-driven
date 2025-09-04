#pragma once

#include "Context.h"
#include "RefCountPtr.h"
#include "VulkanBase.h"

namespace VKRT {
class Device;

class Texture : public RefCountPtr {
public:
    Texture(
        ScopedRefPtr<Context> context,
        uint32_t width,
        uint32_t height,
        uint32_t layers,
        uint32_t mipLevels,
        vk::Format format,
        vk::ImageUsageFlags usageFlags,
        vk::ImageLayout initialLayout = vk::ImageLayout::eUndefined,
        vk::Image image = nullptr);

    Texture(
        ScopedRefPtr<Context> context,
        uint32_t width,
        uint32_t height,
        vk::Format format,
        vk::ImageUsageFlags usageFlags,
        vk::ImageLayout initialLayout = vk::ImageLayout::eUndefined,
        vk::Image image = nullptr);

    Texture(
        ScopedRefPtr<Context> context,
        uint32_t width,
        uint32_t height,
        vk::Format format,
        const uint8_t* buffer,
        size_t bufferSize);

    const vk::ImageView& GetImageView(int32_t mipIndex) const {
        return mipIndex < 0 ? mImageView : mPerMipImageViews[mipIndex];
    }
    const vk::Image& GetImage() const { return mImage; }
    const vk::Format& GetFormat() const { return mFormat; }
    const vk::ImageAspectFlagBits& GetImageAspect() const { return mImageAspect; }
    const uint32_t& GetWidth() const { return mWidth; }
    const uint32_t& GetHeight() const { return mHeight; }
    vk::Extent2D GetExtent() { return vk::Extent2D{mWidth, mHeight}; }
    const uint32_t& GetMipLevels() const { return mMipLevels; }

    struct ImageBarrierInfo {
        ScopedRefPtr<Texture> texture;
        vk::ImageLayout srcLayout;
        vk::ImageLayout dstLayout;
        int32_t mipIndex = -1;
    };
    static std::vector<vk::ImageMemoryBarrier> GetBarriers(
        vk::PipelineStageFlags srcStage,
        vk::PipelineStageFlags dstStage,
        std::vector<ImageBarrierInfo> textures);

    vk::ImageMemoryBarrier GetImageBarrierInfo(
        vk::ImageLayout srcLayout,
        vk::ImageLayout dstLayout,
        vk::PipelineStageFlags srcStage,
        vk::PipelineStageFlags dstStage,
        int32_t mipIndex);

    void SetImageLayout(
        vk::CommandBuffer& commandBuffer,
        vk::ImageLayout oldLayout,
        vk::ImageLayout newLayout,
        vk::PipelineStageFlags srcStage,
        vk::PipelineStageFlags dstStage);

    vk::DescriptorImageInfo GetDescriptorInfo(bool isReadOnly, int32_t mipIndex);

    ~Texture();

private:
    ScopedRefPtr<Context> mContext;

    vk::Image mImage;
    VmaAllocation mAllocation;
    vk::ImageView mImageView;
    std::vector<vk::ImageView> mPerMipImageViews;
    bool mOwnsImage;
    uint32_t mWidth, mHeight, mLayers, mMipLevels;
    vk::Format mFormat;
    vk::ImageAspectFlagBits mImageAspect;
};
}  // namespace VKRT
