#include "MeshSystem.h"

#include "DebugUtils.h"

namespace VKRT {

MeshSystem::MeshSystem(ScopedRefPtr<Context> context) : mContext(context) {}

ScopedRefPtr<Mesh> MeshSystem::GetOrCreate(
    uint32_t meshId,
    const std::vector<glm::vec3>& vertices,
    const std::vector<uint32_t>& indices,
    const std::vector<uint32_t>& texCoord,
    const std::vector<uint32_t>& normals,
    ScopedRefPtr<Material> material) {
    auto it = mMeshes.find(meshId);
    if (it != mMeshes.end()) {
        return it->second;
    } else {
        VKRT_ASSERT(vertices.size() == texCoord.size());
        VKRT_ASSERT(vertices.size() == normals.size());
        mMeshData.emplace(
            meshId,
            MeshData{.vertices = vertices, .indices = indices, .texCoord = texCoord, .normals = normals});
        ScopedRefPtr<Mesh> newMesh = new Mesh(material);
        newMesh->SetAABB(AABB(vertices));
        mMeshes.emplace(meshId, newMesh);
        return newMesh;
    }
}

void MeshSystem::FlattenBuffer(
    ScopedRefPtr<Context> context,
    ScopedRefPtr<VulkanBuffer>& target,
    const std::unordered_map<uint32_t, MeshData>& meshData,
    AttributeType type) {
    size_t itemCount = 0;
    for (const auto& entry : meshData) {
        const MeshData& data = entry.second;
        switch (type) {
            case AttributeType::Index: {
                itemCount += data.indices.size();
            } break;
            case AttributeType::Position: {
                itemCount += data.vertices.size();
            } break;
            case AttributeType::TexCoord: {
                itemCount += data.texCoord.size();
            } break;
            case AttributeType::Normal: {
                itemCount += data.normals.size();
            } break;
        }
    }

    size_t bufferSize = itemCount;
    switch (type) {
        case AttributeType::Index: {
            bufferSize *= sizeof(MeshData::indices[0]);
        } break;
        case AttributeType::Position: {
            bufferSize *= sizeof(MeshData::vertices[0]);
        } break;
        case AttributeType::TexCoord: {
            bufferSize *= sizeof(MeshData::texCoord[0]);
        } break;
        case AttributeType::Normal: {
            bufferSize *= sizeof(MeshData::normals[0]);
        } break;
    }

    ScopedRefPtr<VulkanBuffer> stagingBuffer = context->GetDevice()->CreateBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    uint8_t* bufferData = stagingBuffer->MapBuffer();
    size_t globalBufferOffset = 0;
    for (const auto& entry : meshData) {
        const MeshData& data = entry.second;
        size_t subBufferSize = 0;
        uint8_t const* dataBuffer = nullptr;
        switch (type) {
            case AttributeType::Index: {
                subBufferSize = data.indices.size() * sizeof(data.indices[0]);
                dataBuffer = reinterpret_cast<uint8_t const*>(data.indices.data());
            } break;
            case AttributeType::Position: {
                subBufferSize = data.vertices.size() * sizeof(data.vertices[0]);
                dataBuffer = reinterpret_cast<uint8_t const*>(data.vertices.data());
            } break;
            case AttributeType::TexCoord: {
                subBufferSize = data.texCoord.size() * sizeof(data.texCoord[0]);
                dataBuffer = reinterpret_cast<uint8_t const*>(data.texCoord.data());
            } break;
            case AttributeType::Normal: {
                subBufferSize = data.normals.size() * sizeof(data.normals[0]);
                dataBuffer = reinterpret_cast<uint8_t const*>(data.normals.data());
            } break;
        }
        std::copy_n(dataBuffer, subBufferSize, bufferData + globalBufferOffset);
        globalBufferOffset += subBufferSize;
    }
    stagingBuffer->UnmapBuffer();

    {
        vk::BufferUsageFlags usageFlags = vk::BufferUsageFlagBits::eTransferDst;
        switch (type) {
            case AttributeType::Index: {
                usageFlags |= vk::BufferUsageFlagBits::eIndexBuffer;
            } break;
            case AttributeType::Position: {
                usageFlags |= vk::BufferUsageFlagBits::eVertexBuffer;
            } break;
            case AttributeType::TexCoord: {
                usageFlags |= vk::BufferUsageFlagBits::eVertexBuffer;
            } break;
            case AttributeType::Normal: {
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

    FlattenBuffer(mContext, mUnifiedIndexBuffer, mMeshData, AttributeType::Index);
    FlattenBuffer(mContext, mUnifiedVertexBuffer, mMeshData, AttributeType::Position);
    FlattenBuffer(mContext, mUnifiedTexCoordBuffer, mMeshData, AttributeType::TexCoord);
    FlattenBuffer(mContext, mUnifiedNormalBuffer, mMeshData, AttributeType::Normal);

    mMeshData.clear();
}

}  // namespace VKRT