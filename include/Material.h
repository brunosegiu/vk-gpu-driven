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
        glm::vec3 emissive,
        ScopedRefPtr<Texture> albedoTexture = nullptr,
        ScopedRefPtr<Texture> metallicRoughnessTexture = nullptr,
        ScopedRefPtr<Texture> normalTexture = nullptr,
        ScopedRefPtr<Texture> emissiveTexture = nullptr);

    const glm::vec3 GetAlbedo() const { return mAlbedo; }
    const float GetRoughness() const { return mRoughness; }
    const float GetMetallic() const { return mMetallic; }
    const glm::vec3 GetEmissive() const { return mEmissive; }
    AlphaMode GetAlphaMode() const { return mAlphaMode; }
    const ScopedRefPtr<Texture> GetAlbedoTexture() const { return mAlbedoTexture; }
    const ScopedRefPtr<Texture> GetMetallicRoughnessTexture() const {
        return mMetallicRoughnessTexture;
    }
    const ScopedRefPtr<Texture> GetNormalTexture() const { return mNormalTexture; }
    const ScopedRefPtr<Texture> GetEmissiveTexture() const { return mEmissiveTexture; }

    void SetAlbedo(const glm::vec3& albedo) { mAlbedo = albedo; }
    void SetRoughness(float roughness) { mRoughness = roughness; }
    void SetMetallic(float metallic) { mMetallic = metallic; }
    void SetEmissive(const glm::vec3& emissive) { mEmissive = emissive; }
    void SetAlphaMode(AlphaMode alphaMode) { mAlphaMode = alphaMode; }

    void SetMaterialId(uint32_t materialId) { mMaterialId = materialId; }
    const uint32_t& GetMaterialId() const { return mMaterialId; }

    ~Material();

private:
    AlphaMode mAlphaMode;
    glm::vec3 mAlbedo;
    float mRoughness;
    float mMetallic;
    glm::vec3 mEmissive;

    uint32_t mMaterialId;

    ScopedRefPtr<Texture> mAlbedoTexture;
    ScopedRefPtr<Texture> mMetallicRoughnessTexture;
    ScopedRefPtr<Texture> mNormalTexture;
    ScopedRefPtr<Texture> mEmissiveTexture;
};

// TODO: Move to utils
static std::string AlphaModeToStr(const Material::AlphaMode& alphaMode) {
    switch (alphaMode) {
        case Material::AlphaMode::Opaque:
            return "Opaque";
        case Material::AlphaMode::Masked:
            return "Masked";
        case Material::AlphaMode::Blended:
            return "Blended";
    }
    return "";
}

}  // namespace VKRT
