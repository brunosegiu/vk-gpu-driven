#pragma once

#include <glm/glm.hpp>

#include "Context.h"
#include "RefCountPtr.h"
#include "Texture.h"
#include "VulkanBase.h"

namespace VKRT {
class Device;

class Material : public RefCountPtr {
public:
    enum class AlphaMode { Opaque, Masked, Blended };

    static const std::array<AlphaMode, 3> AlphaModes;

    Material();

    Material(
        AlphaMode alphaMode,
        const glm::vec3& albedo,
        float roughness,
        float metallic,
        ScopedRefPtr<Texture> albedoTexture = nullptr,
        ScopedRefPtr<Texture> metallicRoughnessTexture = nullptr,
        ScopedRefPtr<Texture> normalTexture = nullptr);

    const glm::vec3 GetAlbedo() const { return mAlbedo; }
    const float GetRoughness() const { return mRoughness; }
    const float GetMetallic() const { return mMetallic; }
    AlphaMode GetAlphaMode() const { return mAlphaMode; }
    const ScopedRefPtr<Texture> GetAlbedoTexture() const { return mAlbedoTexture; }
    const ScopedRefPtr<Texture> GetMetallicRoughnessTexture() const {
        return mMetallicRoughnessTexture;
    }
    const ScopedRefPtr<Texture> GetNormalTexture() const { return mNormalTexture; }

    void SetAlbedo(const glm::vec3& albedo) { mAlbedo = albedo; }
    void SetRoughness(float roughness) { mRoughness = roughness; }
    void SetMetallic(float metallic) { mMetallic = metallic; }
    void SetAlphaMode(AlphaMode alphaMode) { mAlphaMode = alphaMode; }

    void SetMaterialId(uint32_t materialId) { mMaterialId = materialId; }
    const uint32_t& GetMaterialId() const { return mMaterialId; }

    ~Material();

private:
    AlphaMode mAlphaMode;
    glm::vec3 mAlbedo;
    float mRoughness;
    float mMetallic;

    uint32_t mMaterialId;

    ScopedRefPtr<Texture> mAlbedoTexture;
    ScopedRefPtr<Texture> mMetallicRoughnessTexture;
    ScopedRefPtr<Texture> mNormalTexture;
};
}  // namespace VKRT
