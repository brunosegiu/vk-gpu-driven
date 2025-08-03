#include "Material.h"

#include "DebugUtils.h"
#include "Device.h"

namespace VKRT {

const std::array<Material::AlphaMode, 3> Material::AlphaModes{
    AlphaMode::Opaque,
    AlphaMode::Masked,
    AlphaMode::Blended};

Material::Material() : Material(AlphaMode::Opaque, glm::vec3(0.5), 1.0f, 0.0f) {}

Material::Material(
    AlphaMode alphaMode,
    const glm::vec3& albedo,
    float roughness,
    float metallic,
    ScopedRefPtr<Texture> albedoTexture,
    ScopedRefPtr<Texture> metallicRoughnessTexture,
    ScopedRefPtr<Texture> normalTexture)
    : mAlphaMode(alphaMode),
      mAlbedo(albedo),
      mRoughness(roughness),
      mMetallic(metallic),
      mAlbedoTexture(albedoTexture),
      mMetallicRoughnessTexture(metallicRoughnessTexture),
      mNormalTexture(normalTexture),
      mMaterialId(0) {}

Material::~Material() {}

}  // namespace VKRT