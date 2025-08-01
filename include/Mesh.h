#pragma once

#include "glm/glm.hpp"

#include "AABB.h"
#include "Context.h"
#include "Material.h"
#include "RefCountPtr.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"

namespace VKRT {

struct GeometryLayout {
    vk::Format format;
    size_t stride;
};

class Mesh : public RefCountPtr {
public:
    Mesh(ScopedRefPtr<Material> material);

    const ScopedRefPtr<Material> GetMaterial() const { return mMaterial; }
    ScopedRefPtr<Material> GetMaterial() { return mMaterial; }

    void SetIndexCount(uint32_t indexCount) { mIndexCount = indexCount; }
    const uint32_t& GetIndexCount() const { return mIndexCount; }

    void SetFirstIndex(uint32_t firstIndex) { mFirstIndex = firstIndex; }
    const uint32_t& GetFirstIndex() const { return mFirstIndex; }

    void SetVertexOffset(uint32_t vertexOffset) { mVertexOffset = vertexOffset; }
    const uint32_t& GetVertexOffset() const { return mVertexOffset; }

    void SetAABB(AABB aabb) { mAABB = aabb; }
    const AABB& GetAABB() const { return mAABB; }

    ~Mesh();

private:
    uint32_t mVertexOffset;
    uint32_t mIndexCount;
    uint32_t mFirstIndex;

    ScopedRefPtr<Material> mMaterial;

    AABB mAABB;
};

}  // namespace VKRT