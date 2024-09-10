#include "Material.h"

#include "DebugUtils.h"
#include "Device.h"

namespace VKRT {

Material::Material() : Material(glm::vec3(0.5), 1.0f, 0.0f) {}

Material::Material(
    const glm::vec3& albedo,
    float roughness,
    float metallic,
    ScopedRefPtr<Texture> albedoTexture,
    ScopedRefPtr<Texture> metallicRoughnessTexture,
    ScopedRefPtr<Texture> normalTexture)
    : mAlbedo(albedo),
      mRoughness(roughness),
      mMetallic(metallic),
      mAlbedoTexture(albedoTexture),
      mMetallicRoughnessTexture(metallicRoughnessTexture),
      mNormalTexture(normalTexture),
      mMaterialId(0) {}

Material::~Material() {}

}  // namespace VKRT