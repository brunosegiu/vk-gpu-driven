#pragma once

#include "Context.h"
#include "RefCountPtr.h"
#include "VulkanBase.h"

namespace VKRT {
class Device;

class VulkanBuffer : public RefCountPtr {
public:
    static ScopedRefPtr<VulkanBuffer> Create(
        ScopedRefPtr<Context> context,
        const vk::DeviceSize& size,
        const vk::BufferUsageFlags& usageFlags,
        const VmaAllocationCreateFlags& memoryFlags);

    const vk::DeviceSize& GetBufferSize() const { return mSize; }
    const vk::Buffer& GetBufferHandle() const { return mBufferHandle; }
    const vk::DescriptorBufferInfo& GetDescriptorInfo() const { return mDescriptorInfo; }
    static std::vector<vk::BufferMemoryBarrier> GetBarriers(
        std::vector<ScopedRefPtr<VulkanBuffer>> buffers,
        vk::PipelineStageFlags srcStageMask,
        vk::PipelineStageFlags dstStageMask);
    vk::BufferMemoryBarrier GetBufferBarrierInfo(
        vk::PipelineStageFlags srcStageMask,
        vk::PipelineStageFlags dstStageMask);

    template <typename T>
    void Write(const T& data);
    void Write(const uint8_t* const data, size_t size);
    uint8_t* MapBuffer();
    void UnmapBuffer();

    vk::DeviceAddress GetDeviceAddress();

private:
    VulkanBuffer(
        ScopedRefPtr<Context> context,
        vk::DeviceSize size,
        vk::Buffer bufferHandle,
        VmaAllocation allocation,
        vk::DescriptorBufferInfo descriptorInfo);

    ~VulkanBuffer() override;

    ScopedRefPtr<Context> mContext;
    vk::DeviceSize mSize;
    vk::Buffer mBufferHandle;
    VmaAllocation mAllocation;
    vk::DescriptorBufferInfo mDescriptorInfo;
};

template <typename T>
void VulkanBuffer::Write(const T& data) {
    Write(reinterpret_cast<uint8_t const*>(&data), sizeof(T));
}

}  // namespace VKRT
