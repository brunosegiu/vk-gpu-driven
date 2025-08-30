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

struct Meshlet {
    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    glm::vec3 minBounds;
    glm::vec3 maxBounds;
    glm::vec3 coneApex;
    glm::vec3 coneAxis;
    float coneCutoff;
};

class Mesh : public RefCountPtr {
public:
    Mesh(
        ScopedRefPtr<Material> material,
        const std::vector<Meshlet>& meshlets,
        uint32_t indexCount,
        uint32_t vertexOffset,
        uint32_t indexOffset);

    const ScopedRefPtr<Material> GetMaterial() const { return mMaterial; }
    ScopedRefPtr<Material> GetMaterial() { return mMaterial; }

    uint32_t GetIndexCount() { return mIndexCount; }
    uint32_t GetVertexOffset() { return mVertexOffset; }
    uint32_t GetIndexOffset() { return mIndexOffset; }

    ~Mesh();

    std::vector<Meshlet> mMeshlets;

private:
    uint32_t mIndexCount;
    uint32_t mVertexOffset;
    uint32_t mIndexOffset;

    ScopedRefPtr<Material> mMaterial;
};

}  // namespace VKRT