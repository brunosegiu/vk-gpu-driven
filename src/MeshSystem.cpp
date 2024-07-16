#include "MeshSystem.h"

#include "DebugUtils.h"

namespace VKRT {

MeshSystem::MeshSystem(ScopedRefPtr<Context> context) : mContext(context) {}

ScopedRefPtr<Mesh> MeshSystem::GetOrCreate(
    uint32_t meshId,
    const std::vector<glm::vec3>& vertices,
    const std::vector<uint32_t>& indices,
    ScopedRefPtr<Material> material) {
    auto it = mMeshes.find(meshId);
    if (it != mMeshes.end()) {
        return it->second;
    } else {
        mMeshData.emplace(meshId, MeshData{.vertices = vertices, .indices = indices});
        ScopedRefPtr<Mesh> newMesh = new Mesh(material);
        mMeshes.emplace(meshId, newMesh);
        return newMesh;
    }
}

void MeshSystem::Upload() {

    if (mUnifiedVertexBuffer != nullptr) {
        // TODO: Support updates
        return;
    }

    size_t vertexBufferOffset = 0;
    size_t indexBufferOffset = 0;
    for (auto& entry : mMeshData) {
        MeshData& data = entry.second;
        const size_t vertexCount = data.vertices.size();
        const size_t indexCount = data.indices.size();

        ScopedRefPtr<Mesh> mesh = mMeshes.at(entry.first);

        mesh->SetIndexCount(indexCount);
        mesh->SetFirstIndex(indexBufferOffset);
        mesh->SetVertexOffset(vertexBufferOffset);

        vertexBufferOffset += vertexCount;
        indexBufferOffset += indexCount;
    }

    const size_t vertexBufferSize = vertexBufferOffset * sizeof(glm::vec3);

    // Vertex copy
    {
        ScopedRefPtr<VulkanBuffer> stagingBuffer = mContext->GetDevice()->CreateBuffer(
            vertexBufferSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        uint8_t* bufferData = stagingBuffer->MapBuffer();
        size_t vertexBufferOffset = 0;
        for (auto& entry : mMeshData) {
            MeshData& data = entry.second;
            const size_t vertexCount = data.vertices.size();
            const size_t meshVertexSize = vertexCount * sizeof(glm::vec3);
            std::copy_n(
                reinterpret_cast<uint8_t const*>(data.vertices.data()),
                meshVertexSize,
                bufferData + vertexBufferOffset);
            vertexBufferOffset += meshVertexSize;
        }
        stagingBuffer->UnmapBuffer();

        {
            mUnifiedVertexBuffer = mContext->GetDevice()->CreateBuffer(
                vertexBufferSize,
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                vk::MemoryPropertyFlagBits::eDeviceLocal);

            vk::CommandBuffer commandBuffer = mContext->GetDevice()->CreateCommandBuffer();
            VKRT_ASSERT_VK(commandBuffer.begin(vk::CommandBufferBeginInfo{}));

            vk::BufferCopy bufferCopy =
                vk::BufferCopy().setSize(vertexBufferSize).setDstOffset(0).setSrcOffset(0);

            commandBuffer.copyBuffer(
                stagingBuffer->GetBufferHandle(),
                mUnifiedVertexBuffer->GetBufferHandle(),
                bufferCopy);

            VKRT_ASSERT_VK(commandBuffer.end());
            mContext->GetDevice()->SubmitCommandAndFlush(commandBuffer);
            mContext->GetDevice()->DestroyCommand(commandBuffer);
        }
    }

    const size_t indexBufferSize = indexBufferOffset * sizeof(uint32_t);
    // Index copy
    {
        ScopedRefPtr<VulkanBuffer> stagingBuffer = mContext->GetDevice()->CreateBuffer(
            indexBufferSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        uint8_t* bufferData = stagingBuffer->MapBuffer();
        size_t indexBufferOffset = 0;
        for (auto& entry : mMeshData) {
            MeshData& data = entry.second;
            const size_t indexCount = data.indices.size();
            const size_t meshIndexSize = indexCount * sizeof(uint32_t);
            std::copy_n(
                reinterpret_cast<uint8_t const*>(data.indices.data()),
                meshIndexSize,
                bufferData + indexBufferOffset);
            indexBufferOffset += meshIndexSize;
        }
        stagingBuffer->UnmapBuffer();

        {
            mUnifiedIndexBuffer = mContext->GetDevice()->CreateBuffer(
                indexBufferSize,
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                vk::MemoryPropertyFlagBits::eDeviceLocal);

            vk::CommandBuffer commandBuffer = mContext->GetDevice()->CreateCommandBuffer();
            VKRT_ASSERT_VK(commandBuffer.begin(vk::CommandBufferBeginInfo{}));

            vk::BufferCopy bufferCopy =
                vk::BufferCopy().setSize(indexBufferSize).setDstOffset(0).setSrcOffset(0);

            commandBuffer.copyBuffer(
                stagingBuffer->GetBufferHandle(),
                mUnifiedIndexBuffer->GetBufferHandle(),
                bufferCopy);

            VKRT_ASSERT_VK(commandBuffer.end());
            mContext->GetDevice()->SubmitCommandAndFlush(commandBuffer);
            mContext->GetDevice()->DestroyCommand(commandBuffer);
        }
    }

    mMeshData.clear();
}

}  // namespace VKRT