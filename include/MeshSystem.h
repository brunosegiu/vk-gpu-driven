#pragma once

#include <list>
#include <unordered_map>

#include "Context.h"
#include "Mesh.h"
#include "RefCountPtr.h"
#include "VulkanBuffer.h"

namespace VKRTBaker {
struct Vec3;
}

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

    const ScopedRefPtr<VulkanBuffer>& GetVertexBuffer() const { return mUnifiedVertexBuffer; }
    const ScopedRefPtr<VulkanBuffer>& GetIndexBuffer() const { return mUnifiedIndexBuffer; }
    const ScopedRefPtr<VulkanBuffer>& GetTexCoordBuffer() const { return mUnifiedTexCoordBuffer; }
    const ScopedRefPtr<VulkanBuffer>& GetNormalBuffer() const { return mUnifiedNormalBuffer; }
    const ScopedRefPtr<VulkanBuffer>& GetTangentBuffer() const { return mUnifiedTangentBuffer; }

    void Upload(
        const std::vector<VKRTBaker::Vec3>& vertices,
        const std::vector<uint32_t>& texCoord,
        const std::vector<uint32_t>& normals,
        const std::vector<uint32_t>& tangents,
        const std::vector<uint32_t>& indices);

    void BindBuffers(vk::CommandBuffer& commandBuffer, VertexAttributeFlag attributeFlags);

private:
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<VulkanBuffer> mUnifiedVertexBuffer;
    ScopedRefPtr<VulkanBuffer> mUnifiedIndexBuffer;
    ScopedRefPtr<VulkanBuffer> mUnifiedTexCoordBuffer;
    ScopedRefPtr<VulkanBuffer> mUnifiedNormalBuffer;
    ScopedRefPtr<VulkanBuffer> mUnifiedTangentBuffer;
};

}  // namespace VKRT
