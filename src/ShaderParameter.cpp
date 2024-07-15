#include "ShaderParameter.h"

#include "DebugUtils.h"

namespace VKRT {

ShaderParameter::ShaderParameter(
    ScopedRefPtr<Context> context,
    vk::DescriptorType type,
    UpdateFrequency updateFrequency,
    vk::ShaderStageFlags stageFlags,
    uint32_t count,
    bool variableCount)
    : mContext(context),
      mUpdateFrequency(updateFrequency),
      mDescriptorType(type),
      mCount(count),
      mStageFlags(stageFlags),
      mHasVariableCount(variableCount) {}

ShaderParameterBuffer::ShaderParameterBuffer(
    ScopedRefPtr<Context> context,
    const vk::DescriptorType& type,
    const UpdateFrequency& updateFrequency,
    const vk::ShaderStageFlags& stageFlags,
    const vk::DeviceSize& size,
    const vk::BufferUsageFlags& usageFlags,
    const vk::MemoryPropertyFlags& memoryFlags,
    const vk::MemoryAllocateFlags& memoryAllocateFlags)
    : ShaderParameter(context, type, updateFrequency, stageFlags, 1, false) {
    VKRT_ASSERT(
        type == vk::DescriptorType::eUniformBuffer || type == vk::DescriptorType::eStorageBuffer);

    uint32_t bufferCount = 1;
    if (updateFrequency == ShaderParameter::UpdateFrequency::PerFrame) {
        bufferCount = mContext->GetMaxInFlightFrameCount();
    }

    for (uint32_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex) {
        ScopedRefPtr<VulkanBuffer> buffer =
            mContext->GetDevice()->CreateBuffer(size, usageFlags, memoryFlags, memoryAllocateFlags);
        mBuffers.emplace_back(buffer);
    }
}

ScopedRefPtr<VulkanBuffer> ShaderParameterBuffer::GetBuffer(uint32_t frameIndex) {
    return mBuffers[frameIndex];
}

vk::DescriptorBufferInfo ShaderParameterBuffer::GetBufferInfo(uint32_t frameIndex) {
    ScopedRefPtr<VulkanBuffer> buffer = mBuffers[frameIndex];
    return buffer->GetDescriptorInfo();
}

ShaderParameterImage::ShaderParameterImage(
    ScopedRefPtr<Context> context,
    const vk::DescriptorType& type,
    const vk::ShaderStageFlags& stageFlags,
    const vk::DeviceSize& size,
    uint32_t count,
    bool variableCount)
    : ShaderParameter(
          context,
          type,
          ShaderParameter::UpdateFrequency::Once,
          stageFlags,
          count,
          variableCount) {}

void ShaderParameterImage::Bind(const ScopedRefPtr<Texture>& texture) {
    mTextures.emplace_back(texture);
}

std::vector<vk::DescriptorImageInfo> ShaderParameterImage::GetImageInfos() {
    std::vector<vk::DescriptorImageInfo> imageInfos;
    for (const ScopedRefPtr<Texture>& texture : mTextures) {
        imageInfos.push_back(vk::DescriptorImageInfo()
                                 .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                                 .setImageView(texture->GetImageView())
                                 .setSampler(nullptr));
    }
    return imageInfos;
}

ShaderParameterSampler::ShaderParameterSampler(
    ScopedRefPtr<Context> context,
    const vk::DescriptorType& type,
    const vk::ShaderStageFlags& stageFlags,
    vk::SamplerCreateInfo createInfo)
    : ShaderParameter(context, type, ShaderParameter::UpdateFrequency::Once, stageFlags, 1, false) {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    mSampler = VKRT_ASSERT_VK(logicalDevice.createSampler(createInfo));
}

std::vector<vk::DescriptorImageInfo> ShaderParameterSampler::GetImageInfos() {
    vk::DescriptorImageInfo samplerInfo = vk::DescriptorImageInfo().setSampler(mSampler);
    return std::vector<vk::DescriptorImageInfo>{samplerInfo};
}

ShaderParameterSampler::~ShaderParameterSampler() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    logicalDevice.destroySampler(mSampler);
}

ShaderParameterPushConstant::ShaderParameterPushConstant(
    ScopedRefPtr<Context> context,
    const vk::ShaderStageFlags& stageFlags,
    const vk::DeviceSize& size)
    : ShaderParameter(
          context,
          vk::DescriptorType::eUniformBuffer,
          ShaderParameter::UpdateFrequency::PerDraw,
          stageFlags,
          1,
          false),
      mSize(size),
      mOffset(0) {}

ShaderParameter::~ShaderParameter() {}

}  // namespace VKRT