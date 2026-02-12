#pragma once

#include <list>
#include <unordered_map>

#include "Context.h"
#include "Mesh.h"
#include "RefCountPtr.h"
#include "VulkanBuffer.h"

namespace VKRTBaker {
struct Vec3;
struct UVec3;
}

namespace VKRT {
class Device;

enum VertexAttributeFlag : uint32_t {
    Index = 0,
    Position = 0x1,
    NormalTexCoordTangent = 0x2,
    All = Position | NormalTexCoordTangent
};

class MeshSystem : public RefCountPtr {
public:
    MeshSystem(ScopedRefPtr<Context> context);

    static const std::vector<GeometryLayout> GetGeometryLayout(VertexAttributeFlag attributeFlags);

    const ScopedRefPtr<VulkanBuffer>& GetVertexBuffer() const { return mUnifiedVertexBuffer; }
    const ScopedRefPtr<VulkanBuffer>& GetIndexBuffer() const { return mUnifiedIndexBuffer; }
    const ScopedRefPtr<VulkanBuffer>& GetNormalTexCoordTangentBuffer() const {
        return mUnifiedNormalTexCoordTangentBuffer;
    };

    uint32_t GetVertexCount() { return mVertexCount; }
    uint32_t GetPrimitiveCount() { return mPrimitiveCount; }

    void Upload(
        const std::vector<VKRTBaker::Vec3>& vertices,
        const std::vector<VKRTBaker::UVec3>& normalsTexturesTangents,
        const std::vector<uint32_t>& indices);

    void BindBuffers(vk::CommandBuffer& commandBuffer, VertexAttributeFlag attributeFlags);

private:
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<VulkanBuffer> mUnifiedVertexBuffer;
    ScopedRefPtr<VulkanBuffer> mUnifiedIndexBuffer;
    ScopedRefPtr<VulkanBuffer> mUnifiedNormalTexCoordTangentBuffer;
    uint32_t mVertexCount;
    uint32_t mPrimitiveCount;
};

}  // namespace VKRT
