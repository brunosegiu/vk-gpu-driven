#include "Mesh.h"

#include "DebugUtils.h"
#include "Material.h"
#include "Texture.h"

namespace VKRT {

Mesh::Mesh(ScopedRefPtr<Material> material) : mMaterial(material) {}

Mesh::~Mesh() {}

}  // namespace VKRT