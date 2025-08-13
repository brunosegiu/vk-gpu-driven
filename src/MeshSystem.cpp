#include "MeshSystem.h"

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

    if (attributeFlags & VertexAttributeFlag::TexCoord) {
        geometryLayout.push_back({.format = vk::Format::eR32Uint, .stride = sizeof(uint32_t)});
    }

    if (attributeFlags & VertexAttributeFlag::Normal) {
        geometryLayout.push_back({.format = vk::Format::eR32Uint, .stride = sizeof(uint32_t)});
    }

    if (attributeFlags & VertexAttributeFlag::Tangent) {
        geometryLayout.push_back({.format = vk::Format::eR32Uint, .stride = sizeof(uint32_t)});
    }

    return geometryLayout;
}

ScopedRefPtr<Mesh> MeshSystem::GetOrCreate(
    uint32_t meshId,
    const std::vector<glm::vec3>& vertices,
    const std::vector<uint32_t>& indices,
    const std::vector<uint32_t>& texCoord,
    const std::vector<uint32_t>& normals,
    const std::vector<uint32_t>& tangents,
    ScopedRefPtr<Material> material) {
    auto it = mMeshes.find(meshId);
    if (it != mMeshes.end()) {
        return it->second;
    } else {
        VKRT_ASSERT(vertices.size() == texCoord.size());
        VKRT_ASSERT(vertices.size() == normals.size());
        VKRT_ASSERT(vertices.size() == tangents.size());
        mMeshData.emplace(
            meshId,
            MeshData{
                .vertices = vertices,
                .indices = indices,
                .texCoord = texCoord,
                .normals = normals,
                .tangents = tangents});
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
    VertexAttributeFlag type) {
    size_t itemCount = 0;
    for (const auto& entry : meshData) {
        const MeshData& data = entry.second;
        switch (type) {
            case VertexAttributeFlag::Index: {
                itemCount += data.indices.size();
            } break;
            case VertexAttributeFlag::Position: {
                itemCount += data.vertices.size();
            } break;
            case VertexAttributeFlag::TexCoord: {
                itemCount += data.texCoord.size();
            } break;
            case VertexAttributeFlag::Normal: {
                itemCount += data.normals.size();
            } break;
            case VertexAttributeFlag::Tangent: {
                itemCount += data.tangents.size();
            } break;
        }
    }

    size_t bufferSize = itemCount;
    switch (type) {
        case VertexAttributeFlag::Index: {
            bufferSize *= sizeof(MeshData::indices[0]);
        } break;
        case VertexAttributeFlag::Position: {
            bufferSize *= sizeof(MeshData::vertices[0]);
        } break;
        case VertexAttributeFlag::TexCoord: {
            bufferSize *= sizeof(MeshData::texCoord[0]);
        } break;
        case VertexAttributeFlag::Normal: {
            bufferSize *= sizeof(MeshData::normals[0]);
        } break;
        case VertexAttributeFlag::Tangent: {
            bufferSize *= sizeof(MeshData::tangents[0]);
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
            case VertexAttributeFlag::Index: {
                subBufferSize = data.indices.size() * sizeof(data.indices[0]);
                dataBuffer = reinterpret_cast<uint8_t const*>(data.indices.data());
            } break;
            case VertexAttributeFlag::Position: {
                subBufferSize = data.vertices.size() * sizeof(data.vertices[0]);
                dataBuffer = reinterpret_cast<uint8_t const*>(data.vertices.data());
            } break;
            case VertexAttributeFlag::TexCoord: {
                subBufferSize = data.texCoord.size() * sizeof(data.texCoord[0]);
                dataBuffer = reinterpret_cast<uint8_t const*>(data.texCoord.data());
            } break;
            case VertexAttributeFlag::Normal: {
                subBufferSize = data.normals.size() * sizeof(data.normals[0]);
                dataBuffer = reinterpret_cast<uint8_t const*>(data.normals.data());
            } break;
            case VertexAttributeFlag::Tangent: {
                subBufferSize = data.tangents.size() * sizeof(data.tangents[0]);
                dataBuffer = reinterpret_cast<uint8_t const*>(data.tangents.data());
            } break;
        }
        std::copy_n(dataBuffer, subBufferSize, bufferData + globalBufferOffset);
        globalBufferOffset += subBufferSize;
    }
    stagingBuffer->UnmapBuffer();

    {
        vk::BufferUsageFlags usageFlags =
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer;
        switch (type) {
            case VertexAttributeFlag::Index: {
                usageFlags |= vk::BufferUsageFlagBits::eIndexBuffer;
            } break;
            case VertexAttributeFlag::Position:
            case VertexAttributeFlag::TexCoord:
            case VertexAttributeFlag::Normal:
            case VertexAttributeFlag::Tangent: {
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

    FlattenBuffer(mContext, mUnifiedIndexBuffer, mMeshData, VertexAttributeFlag::Index);
    FlattenBuffer(mContext, mUnifiedVertexBuffer, mMeshData, VertexAttributeFlag::Position);
    FlattenBuffer(mContext, mUnifiedTexCoordBuffer, mMeshData, VertexAttributeFlag::TexCoord);
    FlattenBuffer(mContext, mUnifiedNormalBuffer, mMeshData, VertexAttributeFlag::Normal);
    FlattenBuffer(mContext, mUnifiedTangentBuffer, mMeshData, VertexAttributeFlag::Tangent);

    mMeshData.clear();
}

void MeshSystem::BindBuffers(vk::CommandBuffer& commandBuffer, VertexAttributeFlag attributeFlags) {
    uint32_t currentIndex = 0;
    if (attributeFlags & VertexAttributeFlag::Position) {
        commandBuffer.bindVertexBuffers(currentIndex, GetVertexBuffer()->GetBufferHandle(), {0});
        ++currentIndex;
    }
    if (attributeFlags & VertexAttributeFlag::TexCoord) {
        commandBuffer.bindVertexBuffers(currentIndex, GetTexCoordBuffer()->GetBufferHandle(), {0});
        ++currentIndex;
    }
    if (attributeFlags & VertexAttributeFlag::Normal) {
        commandBuffer.bindVertexBuffers(currentIndex, GetNormalBuffer()->GetBufferHandle(), {0});
        ++currentIndex;
    }
    if (attributeFlags & VertexAttributeFlag::Tangent) {
        commandBuffer.bindVertexBuffers(currentIndex, GetTangentBuffer()->GetBufferHandle(), {0});
        ++currentIndex;
    }
    commandBuffer.bindIndexBuffer(GetIndexBuffer()->GetBufferHandle(), {0}, vk::IndexType::eUint32);
}

}  // namespace VKRT