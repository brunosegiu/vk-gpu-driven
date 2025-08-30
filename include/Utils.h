#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "Context.h"
#include "DebugUtils.h"
#include "VulkanBuffer.h"

namespace VKRT {

class Utils {
public:
    struct Geometry {
        std::vector<glm::vec3> positions;
        std::vector<uint32_t> indices;
    };
    static Geometry BuildSphere(uint32_t slices, uint32_t stacks);

    template <typename T>
    static void UploadBuffer(
        ScopedRefPtr<Context> context,
        ScopedRefPtr<VulkanBuffer>& target,
        const std::vector<T>& data,
        vk::BufferUsageFlags usageFlags);
};

template <typename T>
void Utils::UploadBuffer(
    ScopedRefPtr<Context> context,
    ScopedRefPtr<VulkanBuffer>& target,
    const std::vector<T>& data,
    vk::BufferUsageFlags usageFlags) {
    size_t bufferSize = data.size() * sizeof(data[0]);
    ScopedRefPtr<VulkanBuffer> stagingBuffer = context->GetDevice()->CreateBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    uint8_t* bufferData = stagingBuffer->MapBuffer();
    {
        size_t subBufferSize = data.size() * sizeof(data[0]);
        uint8_t const* dataBuffer = reinterpret_cast<uint8_t const*>(data.data());
        std::copy_n(dataBuffer, subBufferSize, bufferData);
    }
    stagingBuffer->UnmapBuffer();

    {
        usageFlags |= vk::BufferUsageFlagBits::eTransferDst;

        target = context->GetDevice()->CreateBuffer(
            bufferSize,
            usageFlags,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

        vk::CommandBuffer commandBuffer = context->GetDevice()->CreateCommandBuffer();
        VKRT_ASSERT_VK(commandBuffer.begin(vk::CommandBufferBeginInfo{}));

        vk::BufferCopy bufferCopy =
            vk::BufferCopy().setSize(bufferSize).setDstOffset(0).setSrcOffset(0);

        commandBuffer.copyBuffer(
            stagingBuffer->GetBufferHandle(),
            target->GetBufferHandle(),
            bufferCopy);

        VKRT_ASSERT_VK(commandBuffer.end());
        context->GetDevice()->SubmitCommandAndFlush(commandBuffer);
        context->GetDevice()->DestroyCommand(commandBuffer);
    }
}

}  // namespace VKRT