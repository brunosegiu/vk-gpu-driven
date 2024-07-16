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
    Mesh(ScopedRefPtr<Material> material);

    const ScopedRefPtr<Material> GetMaterial() const { return mMaterial; }
    ScopedRefPtr<Material> GetMaterial() { return mMaterial; }

    void SetIndexCount(uint32_t indexCount) { mIndexCount = indexCount; }
    const uint32_t& GetIndexCount() const { return mIndexCount; }

    void SetFirstIndex(uint32_t firstIndex) { mFirstIndex = firstIndex; }
    const uint32_t& GetFirstIndex() const { return mFirstIndex; }

    void SetVertexOffset(uint32_t vertexOffset) { mVertexOffset = vertexOffset; }
    const uint32_t& GetVertexOffset() const { return mVertexOffset; }

    ~Mesh();

private:
    uint32_t mVertexOffset;
    uint32_t mIndexCount;
    uint32_t mFirstIndex;

    ScopedRefPtr<Material> mMaterial;
};

}  // namespace VKRT