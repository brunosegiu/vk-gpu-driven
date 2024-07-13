#pragma once

#include "glm/glm.hpp"

#include "Context.h"
#include "Material.h"
#include "RefCountPtr.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"

namespace VKRT {

class Mesh : public RefCountPtr {
public:
    struct Vertex {
        glm::vec3 position;
    };
    Mesh(
        ScopedRefPtr<Context> context,
        const std::vector<Vertex>& vertices,
        const std::vector<glm::uvec3>& indices,
        ScopedRefPtr<Material> material);

    const ScopedRefPtr<Material> GetMaterial() const { return mMaterial; }
    ScopedRefPtr<Material> GetMaterial() { return mMaterial; }
    
    const ScopedRefPtr<VulkanBuffer>& GetVertexBuffer() const { return mVertexBuffer; }
    const uint32_t& GetVertexCount() const { return mVertexCount; }

    const ScopedRefPtr<VulkanBuffer>& GetIndexBuffer() const { return mIndexBuffer; }
    const uint32_t& GetIndexCount() const { return mIndexCount; }

    ~Mesh();

private:
    ScopedRefPtr<Context> mContext;

    ScopedRefPtr<VulkanBuffer> mVertexBuffer;
    uint32_t mVertexCount;
    ScopedRefPtr<VulkanBuffer> mIndexBuffer;
    uint32_t mIndexCount;

    ScopedRefPtr<Material> mMaterial;
};

}  // namespace VKRT