#include "VulkanBuffer.h"

#include "DebugUtils.h"
#include "Device.h"
#include "VulkanHelpers.h"

namespace VKRT {

ScopedRefPtr<VulkanBuffer> VulkanBuffer::Create(
    ScopedRefPtr<Context> context,
    const vk::DeviceSize& size,
    const vk::BufferUsageFlags& usageFlags,
    const VmaAllocationCreateFlags& memoryFlags) {
    const vk::BufferCreateInfo bufferCreateInfo = vk::BufferCreateInfo()
                                                      .setSize(size)
                                                      .setUsage(usageFlags)
                                                      .setSharingMode(vk::SharingMode::eExclusive);
    VmaAllocator allocator = context->GetDevice()->GetAllocator();

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationInfo.flags = memoryFlags;
    VmaAllocationInfo allocInfo{};

    vk::Buffer bufferHandle;
    VmaAllocation allocation;
    VkResult result = vmaCreateBuffer(
        allocator,
        (VkBufferCreateInfo*)&bufferCreateInfo,
        &allocationInfo,
        (VkBuffer*)&bufferHandle,
        &allocation,
        &allocInfo);

    const vk::DescriptorBufferInfo bufferInfo =
        vk::DescriptorBufferInfo().setBuffer(bufferHandle).setOffset(0).setRange(size);

    return new VulkanBuffer(context, size, bufferHandle, allocation, bufferInfo);
}

VulkanBuffer::VulkanBuffer(
    ScopedRefPtr<Context> context,
    vk::DeviceSize size,
    vk::Buffer bufferHandle,
    VmaAllocation allocation,
    vk::DescriptorBufferInfo descriptorInfo)
    : mContext(context),
      mSize(size),
      mBufferHandle(bufferHandle),
      mAllocation(allocation),
      mDescriptorInfo(descriptorInfo) {}

std::vector<vk::BufferMemoryBarrier> VulkanBuffer::GetBarriers(
    std::vector<ScopedRefPtr<VulkanBuffer>> buffers,
    vk::PipelineStageFlags srcStageMask,
    vk::PipelineStageFlags dstStageMask) {
    std::vector<vk::BufferMemoryBarrier> barriers;
    barriers.reserve(buffers.size());
    for (const ScopedRefPtr<VulkanBuffer>& buffer : buffers) {
        barriers.push_back(buffer->GetBufferBarrierInfo(srcStageMask, dstStageMask));
    }
    return barriers;
}

vk::BufferMemoryBarrier VulkanBuffer::GetBufferBarrierInfo(
    vk::PipelineStageFlags srcStageMask,
    vk::PipelineStageFlags dstStageMask) {
    return vk::BufferMemoryBarrier()
        .setBuffer(GetBufferHandle())
        .setSize(GetBufferSize())
        .setSrcAccessMask(Helpers::GetAccessMasksForStage(srcStageMask, true))
        .setDstAccessMask(Helpers::GetAccessMasksForStage(dstStageMask, false));
}

uint8_t* VulkanBuffer::MapBuffer() {
    VmaAllocator allocator = mContext->GetDevice()->GetAllocator();
    uint8_t* mappedResult = nullptr;
    vmaMapMemory(allocator, mAllocation, (void**)&mappedResult);
    return mappedResult;
}

void VulkanBuffer::UnmapBuffer() {
    VmaAllocator allocator = mContext->GetDevice()->GetAllocator();
    vmaUnmapMemory(allocator, mAllocation);
}

vk::DeviceAddress VulkanBuffer::GetDeviceAddress() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    vk::BufferDeviceAddressInfoKHR bufferAddressInfo =
        vk::BufferDeviceAddressInfoKHR().setBuffer(mBufferHandle);
    return logicalDevice.getBufferAddress(bufferAddressInfo);
}

VulkanBuffer::~VulkanBuffer() {
    VmaAllocator allocator = mContext->GetDevice()->GetAllocator();
    vmaDestroyBuffer(allocator, mBufferHandle, mAllocation);
}

}  // namespace VKRT