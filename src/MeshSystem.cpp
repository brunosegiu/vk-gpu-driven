#include "MeshSystem.h"

#include "../baker/include/BakedSceneSerialization.h"
#include "DebugUtils.h"

namespace VKRT {

MeshSystem::MeshSystem(ScopedRefPtr<Context> context) : mContext(context) {}

const std::vector<GeometryLayout> MeshSystem::GetGeometryLayout(
    VertexAttributeFlag attributeFlags) {
    std::vector<GeometryLayout> geometryLayout;

    if (attributeFlags & VertexAttributeFlag::Position) {
        geometryLayout.push_back(
            {.format = vk::Format::eR32G32B32A32Sfloat, .stride = sizeof(glm::vec3)});
    }

    if (attributeFlags & VertexAttributeFlag::NormalTexCoordTangent) {
        geometryLayout.push_back({.format = vk::Format::eR32G32B32Uint, .stride = sizeof(glm::uvec3)});
    }

    return geometryLayout;
}

template <typename T>
void UploadBuffer(
    ScopedRefPtr<Context> context,
    ScopedRefPtr<VulkanBuffer>& target,
    const std::vector<T>& data,
    VertexAttributeFlag type) {
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
        vk::BufferUsageFlags usageFlags =
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer;
        switch (type) {
            case VertexAttributeFlag::Index: {
                usageFlags |= vk::BufferUsageFlagBits::eIndexBuffer;
                usageFlags |= vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
                usageFlags |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
            } break;
            case VertexAttributeFlag::Position: {
                usageFlags |= vk::BufferUsageFlagBits::eVertexBuffer;
                usageFlags |= vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
                usageFlags |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
            } break;
            case VertexAttributeFlag::NormalTexCoordTangent: {
                usageFlags |= vk::BufferUsageFlagBits::eVertexBuffer;
            } break;
        }

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

void MeshSystem::Upload(
    const std::vector<VKRTBaker::Vec3>& vertices,
    const std::vector<VKRTBaker::UVec3>& normalsTexturesTangents,
    const std::vector<uint32_t>& indices) {
    UploadBuffer(mContext, mUnifiedVertexBuffer, vertices, VertexAttributeFlag::Position);
    UploadBuffer(
        mContext,
        mUnifiedNormalTexCoordTangentBuffer,
        normalsTexturesTangents,
        VertexAttributeFlag::NormalTexCoordTangent);
    UploadBuffer(mContext, mUnifiedIndexBuffer, indices, VertexAttributeFlag::Index);

    mVertexCount = vertices.size();
    mPrimitiveCount = indices.size() / 3;
}

void MeshSystem::BindBuffers(vk::CommandBuffer& commandBuffer, VertexAttributeFlag attributeFlags) {
    uint32_t currentIndex = 0;
    if (attributeFlags & VertexAttributeFlag::Position) {
        commandBuffer.bindVertexBuffers(currentIndex, GetVertexBuffer()->GetBufferHandle(), {0});
        ++currentIndex;
    }
    if (attributeFlags & VertexAttributeFlag::NormalTexCoordTangent) {
        commandBuffer.bindVertexBuffers(currentIndex, GetNormalTexCoordTangentBuffer()->GetBufferHandle(), {0});
        ++currentIndex;
    }
    commandBuffer.bindIndexBuffer(GetIndexBuffer()->GetBufferHandle(), {0}, vk::IndexType::eUint32);
}

}  // namespace VKRT