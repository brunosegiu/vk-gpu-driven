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
        mVertexBuffer = mContext->GetDevice()->CreateBuffer(
            vertexBufferSize,
            vk::BufferUsageFlagBits::eVertexBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        uint8_t* bufferData = mVertexBuffer->MapBuffer();
        std::copy_n(
            reinterpret_cast<uint8_t const*>(vertices.data()),
            vertexBufferSize,
            bufferData);
        mVertexBuffer->UnmapBuffer();
    }

    {
        mIndexCount = indices.size();
        const size_t indexBufferSize = mIndexCount * sizeof(glm::uvec3);
        mIndexBuffer = mContext->GetDevice()->CreateBuffer(
            indexBufferSize,
            vk::BufferUsageFlagBits::eIndexBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        uint8_t* bufferData = mIndexBuffer->MapBuffer();
        std::copy_n(reinterpret_cast<uint8_t const*>(indices.data()), indexBufferSize, bufferData);
        mIndexBuffer->UnmapBuffer();
    }
}

Mesh::~Mesh() {
    mIndexBuffer = nullptr;
    mVertexBuffer = nullptr;
}

}  // namespace VKRT