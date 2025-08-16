#include "Mesh.h"

#include "DebugUtils.h"
#include "Material.h"
#include "Texture.h"

namespace VKRT {

Mesh::Mesh(ScopedRefPtr<Material> material, const std::vector<Meshlet>& meshlets)
    : mIndexCount(0), mMaterial(material), mMeshlets(meshlets) {}

Mesh::~Mesh() {}

}  // namespace VKRT