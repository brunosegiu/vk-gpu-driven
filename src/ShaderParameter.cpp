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
    const VmaAllocationCreateFlags& memoryFlags,
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

ShaderParameterBuffer::ShaderParameterBuffer(
    ScopedRefPtr<Context> context,
    const vk::DescriptorType& type,
    const UpdateFrequency& updateFrequency,
    const vk::ShaderStageFlags& stageFlags)
    : ShaderParameter(context, type, updateFrequency, stageFlags, 1, false) {}

ScopedRefPtr<VulkanBuffer> ShaderParameterBuffer::GetBuffer(uint32_t frameIndex) {
    return mBuffers[frameIndex];
}

void ShaderParameterBuffer::Write(uint32_t frameIndex, const uint8_t* const data, size_t size) {
    ScopedRefPtr<VulkanBuffer> buffer = GetBuffer(frameIndex);
    uint8_t* mappedBuffer = buffer->MapBuffer();
    std::copy_n(data, size, mappedBuffer);
    buffer->UnmapBuffer();
}

const vk::DescriptorBufferInfo& ShaderParameterBuffer::GetBufferInfo(uint32_t frameIndex) {
    ScopedRefPtr<VulkanBuffer> buffer = mBuffers[frameIndex];
    return buffer->GetDescriptorInfo();
}

void ShaderParameterBuffer::BindBuffer(ScopedRefPtr<VulkanBuffer> buffer) {
    if (mBuffers.empty()) {
        mBuffers.push_back(buffer);
    }
}

void ShaderParameterBuffer::BindBuffers(const std::vector<ScopedRefPtr<VulkanBuffer>> buffers) {
    if (mBuffers.empty()) {
        mBuffers.insert(mBuffers.end(), buffers.begin(), buffers.end());
    }
}

ShaderParameterImage::ShaderParameterImage(
    ScopedRefPtr<Context> context,
    const vk::DescriptorType& type,
    const vk::ShaderStageFlags& stageFlags,
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

const std::vector<vk::DescriptorImageInfo>& ShaderParameterImage::GetImageInfos() {
    if (mImageInfos.empty()) {
        for (const ScopedRefPtr<Texture>& texture : mTextures) {
            mImageInfos.push_back(vk::DescriptorImageInfo()
                                      .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                                      .setImageView(texture->GetImageView())
                                      .setSampler(nullptr));
        }
    }
    return mImageInfos;
}

ShaderParameterSampler::ShaderParameterSampler(
    ScopedRefPtr<Context> context,
    const vk::ShaderStageFlags& stageFlags,
    vk::SamplerCreateInfo createInfo)
    : ShaderParameter(
          context,
          vk::DescriptorType::eSampler,
          ShaderParameter::UpdateFrequency::Once,
          stageFlags,
          1,
          false) {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    mSampler = VKRT_ASSERT_VK(logicalDevice.createSampler(createInfo));
}

const std::vector<vk::DescriptorImageInfo>& ShaderParameterSampler::GetImageInfos() {
    if (mSamplerInfo.empty()) {
        mSamplerInfo = {vk::DescriptorImageInfo().setSampler(mSampler)};
    }
    return mSamplerInfo;
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