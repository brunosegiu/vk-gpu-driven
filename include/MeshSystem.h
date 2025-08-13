#pragma once

#include <list>
#include <unordered_map>

#include "Context.h"
#include "Mesh.h"
#include "RefCountPtr.h"
#include "VulkanBuffer.h"

namespace VKRT {
class Device;

enum VertexAttributeFlag : uint32_t {
    Index = 0,
    Position = 0x1,
    TexCoord = 0x2,
    Normal = 0x4,
    Tangent = 0x8,
    All = Position | TexCoord | Normal | Tangent
};

class MeshSystem : public RefCountPtr {
public:
    MeshSystem(ScopedRefPtr<Context> context);

    static const std::vector<GeometryLayout> GetGeometryLayout(VertexAttributeFlag attributeFlags);

    ScopedRefPtr<Mesh> GetOrCreate(
        uint32_t meshId,
        const std::vector<glm::vec3>& vertices,
        const std::vector<uint32_t>& indices,
        const std::vector<uint32_t>& texCoord,
        const std::vector<uint32_t>& normals,
        const std::vector<uint32_t>& tangents,
        ScopedRefPtr<Material> material);

    const ScopedRefPtr<VulkanBuffer>& GetVertexBuffer() const { return mUnifiedVertexBuffer; }
    const ScopedRefPtr<VulkanBuffer>& GetIndexBuffer() const { return mUnifiedIndexBuffer; }
    const ScopedRefPtr<VulkanBuffer>& GetTexCoordBuffer() const { return mUnifiedTexCoordBuffer; }
    const ScopedRefPtr<VulkanBuffer>& GetNormalBuffer() const { return mUnifiedNormalBuffer; }
    const ScopedRefPtr<VulkanBuffer>& GetTangentBuffer() const { return mUnifiedTangentBuffer; }

    void Upload();

    void BindBuffers(vk::CommandBuffer& commandBuffer, VertexAttributeFlag attributeFlags);

private:
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<VulkanBuffer> mUnifiedVertexBuffer;
    ScopedRefPtr<VulkanBuffer> mUnifiedIndexBuffer;
    ScopedRefPtr<VulkanBuffer> mUnifiedTexCoordBuffer;
    ScopedRefPtr<VulkanBuffer> mUnifiedNormalBuffer;
    ScopedRefPtr<VulkanBuffer> mUnifiedTangentBuffer;

    std::unordered_map<uint32_t, ScopedRefPtr<Mesh>> mMeshes;

    struct MeshData {
        std::vector<glm::vec3> vertices;
        std::vector<uint32_t> indices;
        std::vector<uint32_t> texCoord;
        std::vector<uint32_t> normals;
        std::vector<uint32_t> tangents;
    };
    std::unordered_map<uint32_t, MeshData> mMeshData;
    static void FlattenBuffer(
        ScopedRefPtr<Context> context,
        ScopedRefPtr<VulkanBuffer>& target,
        const std::unordered_map<uint32_t, MeshData>& meshData,
        VertexAttributeFlag type);
};

}  // namespace VKRT
