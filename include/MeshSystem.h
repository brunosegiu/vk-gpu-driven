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

    ScopedRefPtr<Mesh> GetOrCreate(
        uint32_t meshId,
        const std::vector<glm::vec3>& vertices,
        const std::vector<uint32_t>& indices,
        ScopedRefPtr<Material> material);

    const ScopedRefPtr<VulkanBuffer>& GetVertexBuffer() const { return mUnifiedVertexBuffer; }
    const ScopedRefPtr<VulkanBuffer>& GetIndexBuffer() const { return mUnifiedIndexBuffer; }

    void Upload();

private:
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<VulkanBuffer> mUnifiedVertexBuffer;
    ScopedRefPtr<VulkanBuffer> mUnifiedIndexBuffer;

    std::unordered_map<uint32_t, ScopedRefPtr<Mesh>> mMeshes;

    struct MeshData {
        std::vector<glm::vec3> vertices;
        std::vector<uint32_t> indices;
    };
    std::unordered_map<uint32_t, MeshData> mMeshData;
};

}  // namespace VKRT
