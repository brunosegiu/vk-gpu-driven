#include "Mesh.h"

#include "DebugUtils.h"
#include "Material.h"
#include "Texture.h"

namespace VKRT {

Mesh::Mesh(
    ScopedRefPtr<Material> material,
    const std::vector<Meshlet>& meshlets,
    uint32_t indexCount,
    uint32_t vertexOffset,
    uint32_t indexOffset)
    : mIndexCount(indexCount),
      mVertexOffset(vertexOffset),
      mIndexOffset(indexOffset),
      mMaterial(material),
      mMeshlets(meshlets) {}

Mesh::~Mesh() {}

}  // namespace VKRT