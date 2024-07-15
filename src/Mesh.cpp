#include "Mesh.h"

#include "DebugUtils.h"
#include "Material.h"
#include "Texture.h"

namespace VKRT {

Mesh::Mesh(
    ScopedRefPtr<Context> context,
    const std::vector<Vertex>& vertices,
    const std::vector<glm::uvec3>& indices,
    ScopedRefPtr<Material> material)
    : mContext(context), mMaterial(material) {
    {
        mVertexCount = vertices.size();
        const size_t vertexBufferSize = mVertexCount * sizeof(Vertex);
        ScopedRefPtr<VulkanBuffer> stagingBuffer = mContext->GetDevice()->CreateBuffer(
            vertexBufferSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        uint8_t* bufferData = stagingBuffer->MapBuffer();
        std::copy_n(
            reinterpret_cast<uint8_t const*>(vertices.data()),
            vertexBufferSize,
            bufferData);
        stagingBuffer->UnmapBuffer();

        mVertexBuffer = mContext->GetDevice()->CreateBuffer(
            vertexBufferSize,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::CommandBuffer commandBuffer = mContext->GetDevice()->CreateCommandBuffer();
        VKRT_ASSERT_VK(commandBuffer.begin(vk::CommandBufferBeginInfo{}));

        vk::BufferCopy bufferCopy =
            vk::BufferCopy().setSize(vertexBufferSize).setDstOffset(0).setSrcOffset(0);

        commandBuffer.copyBuffer(
            stagingBuffer->GetBufferHandle(),
            mVertexBuffer->GetBufferHandle(),
            bufferCopy);

        VKRT_ASSERT_VK(commandBuffer.end());
        mContext->GetDevice()->SubmitCommandAndFlush(commandBuffer);
        mContext->GetDevice()->DestroyCommand(commandBuffer);
    }

    {
        mIndexCount = indices.size();
        const size_t indexBufferSize = mIndexCount * sizeof(glm::uvec3);
        ScopedRefPtr<VulkanBuffer> stagingBuffer = mContext->GetDevice()->CreateBuffer(
            indexBufferSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        uint8_t* bufferData = stagingBuffer->MapBuffer();
        std::copy_n(reinterpret_cast<uint8_t const*>(indices.data()), indexBufferSize, bufferData);
        stagingBuffer->UnmapBuffer();

        mIndexBuffer = mContext->GetDevice()->CreateBuffer(
            indexBufferSize,
            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        vk::CommandBuffer commandBuffer = mContext->GetDevice()->CreateCommandBuffer();
        VKRT_ASSERT_VK(commandBuffer.begin(vk::CommandBufferBeginInfo{}));

        vk::BufferCopy bufferCopy =
            vk::BufferCopy().setSize(indexBufferSize).setDstOffset(0).setSrcOffset(0);

        commandBuffer.copyBuffer(
            stagingBuffer->GetBufferHandle(),
            mIndexBuffer->GetBufferHandle(),
            bufferCopy);

        VKRT_ASSERT_VK(commandBuffer.end());
        mContext->GetDevice()->SubmitCommandAndFlush(commandBuffer);
        mContext->GetDevice()->DestroyCommand(commandBuffer);
    }
}

Mesh::~Mesh() {
    mIndexBuffer = nullptr;
    mVertexBuffer = nullptr;
}

}  // namespace VKRT