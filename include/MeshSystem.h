#pragma once

#include <list>
#include <unordered_map>

#include "Context.h"
#include "Mesh.h"
#include "RefCountPtr.h"
#include "VulkanBuffer.h"

namespace VKRT {
class Device;

class MeshSystem : public RefCountPtr {
public:
    MeshSystem(ScopedRefPtr<Context> context);

    static const std::vector<GeometryLayout> GetGeometryLayout();

    ScopedRefPtr<Mesh> GetOrCreate(
        uint32_t meshId,
        const std::vector<glm::vec3>& vertices,
        const std::vector<uint32_t>& indices,
        const std::vector<uint32_t>& texCoord,
        const std::vector<uint32_t>& normals,
        ScopedRefPtr<Material> material);

    const ScopedRefPtr<VulkanBuffer>& GetVertexBuffer() const { return mUnifiedVertexBuffer; }
    const ScopedRefPtr<VulkanBuffer>& GetIndexBuffer() const { return mUnifiedIndexBuffer; }
    const ScopedRefPtr<VulkanBuffer>& GetTexCoordBuffer() const { return mUnifiedTexCoordBuffer; }
    const ScopedRefPtr<VulkanBuffer>& GetNormalBuffer() const { return mUnifiedNormalBuffer; }

    void Upload();

    void BindBuffers(vk::CommandBuffer& commandBuffer);

private:
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<VulkanBuffer> mUnifiedVertexBuffer;
    ScopedRefPtr<VulkanBuffer> mUnifiedIndexBuffer;
    ScopedRefPtr<VulkanBuffer> mUnifiedTexCoordBuffer;
    ScopedRefPtr<VulkanBuffer> mUnifiedNormalBuffer;

    std::unordered_map<uint32_t, ScopedRefPtr<Mesh>> mMeshes;

    struct MeshData {
        std::vector<glm::vec3> vertices;
        std::vector<uint32_t> indices;
        std::vector<uint32_t> texCoord;
        std::vector<uint32_t> normals;
    };
    std::unordered_map<uint32_t, MeshData> mMeshData;
    enum class AttributeType { Index, Position, TexCoord, Normal };
    static void FlattenBuffer(
        ScopedRefPtr<Context> context,
        ScopedRefPtr<VulkanBuffer>& target,
        const std::unordered_map<uint32_t, MeshData>& meshData,
        AttributeType type);
};

}  // namespace VKRT
