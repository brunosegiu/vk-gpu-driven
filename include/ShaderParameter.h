#pragma once

#include "Context.h"
#include "RefCountPtr.h"
#include "Texture.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"

namespace VKRT {

class ShaderParameter : public RefCountPtr {
public:
    enum class UpdateFrequency { Once, PerFrame, PerDraw };

    ShaderParameter(
        ScopedRefPtr<Context> context,
        vk::DescriptorType type,
        UpdateFrequency updateFrequency,
        vk::ShaderStageFlags stageFlags,
        uint32_t count,
        bool variableCount);

    const vk::DescriptorType& GetDescriptorType() const { return mDescriptorType; }
    const uint32_t& GetCount() const { return mCount; }
    const vk::ShaderStageFlags& GetStageFlags() const { return mStageFlags; }
    const bool& GetHasVariableCount() const { return mHasVariableCount; }
    virtual const uint32_t GetVariableCount() { return 0; }
    UpdateFrequency GetUpdateFrequency() const { return mUpdateFrequency; }

    virtual const vk::DescriptorBufferInfo& GetBufferInfo(uint32_t frameIndex = 0) {
        static vk::DescriptorBufferInfo sDummyDescriptorInfo;
        return sDummyDescriptorInfo;
    };
    virtual const std::vector<vk::DescriptorImageInfo>& GetImageInfos() {
        static std::vector<vk::DescriptorImageInfo> sDummyDescriptorInfo;
        return sDummyDescriptorInfo;
    };

    ~ShaderParameter();

protected:
    ScopedRefPtr<Context> mContext;

private:
    UpdateFrequency mUpdateFrequency;
    vk::DescriptorType mDescriptorType;
    uint32_t mCount;
    vk::ShaderStageFlags mStageFlags;
    bool mHasVariableCount;
    uint32_t mBinding;
};

class ShaderParameterBuffer : public ShaderParameter {
public:
    ShaderParameterBuffer(
        ScopedRefPtr<Context> context,
        const vk::DescriptorType& type,
        const UpdateFrequency& updateFrequency,
        const vk::ShaderStageFlags& stageFlags,
        const vk::DeviceSize& size,
        const vk::BufferUsageFlags& usageFlags,
        const VmaAllocationCreateFlags& memoryFlags,
        const vk::MemoryAllocateFlags& memoryAllocateFlags = {});

    ShaderParameterBuffer(
        ScopedRefPtr<Context> context,
        const vk::DescriptorType& type,
        const UpdateFrequency& updateFrequency,
        const vk::ShaderStageFlags& stageFlags);

    ScopedRefPtr<VulkanBuffer> GetBuffer(uint32_t frameIndex = 0);
    void Write(uint32_t frameIndex, const uint8_t* const data, size_t size);
    const vk::DescriptorBufferInfo& GetBufferInfo(uint32_t frameIndex = 0) override;
    void BindBuffer(ScopedRefPtr<VulkanBuffer> buffer);
    void BindBuffers(const std::vector<ScopedRefPtr<VulkanBuffer>> buffers);

private:
    std::vector<ScopedRefPtr<VulkanBuffer>> mBuffers;
};

class ShaderParameterImage : public ShaderParameter {
public:
    ShaderParameterImage(
        ScopedRefPtr<Context> context,
        const vk::DescriptorType& type,
        const vk::ShaderStageFlags& stageFlags,
        uint32_t count = 1,
        bool variableCount = false);

    void Bind(const ScopedRefPtr<Texture>& texture);

    virtual const uint32_t GetVariableCount() {
        if (GetHasVariableCount()) {
            return mTextures.size();
        }
        return 0;
    }

    const std::vector<vk::DescriptorImageInfo>& GetImageInfos() override;

private:
    std::vector<ScopedRefPtr<Texture>> mTextures;
    std::vector<vk::DescriptorImageInfo> mImageInfos;
};

class ShaderParameterSampler : public ShaderParameter {
public:
    ShaderParameterSampler(
        ScopedRefPtr<Context> context,
        const vk::ShaderStageFlags& stageFlags,
        vk::SamplerCreateInfo createInfo);

    const std::vector<vk::DescriptorImageInfo>& GetImageInfos() override;

    virtual ~ShaderParameterSampler();

private:
    vk::Sampler mSampler;
    std::vector<vk::DescriptorImageInfo> mSamplerInfo;
};

class ShaderParameterPushConstant : public ShaderParameter {
public:
    ShaderParameterPushConstant(
        ScopedRefPtr<Context> context,
        const vk::ShaderStageFlags& stageFlags,
        const vk::DeviceSize& size);

    vk::DeviceSize GetSize() { return mSize; }
    vk::DeviceSize GetOffset() { return mOffset; }

    void SetOffset(vk::DeviceSize offset) { mOffset = offset; }

private:
    vk::DeviceSize mSize;
    vk::DeviceSize mOffset;
};

}  // namespace VKRT
