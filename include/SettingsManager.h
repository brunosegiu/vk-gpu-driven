#pragma once

#include <glm/glm.hpp>

#include "RefCountPtr.h"

namespace VKRT {

struct SSAOControlData {
    float radius = 0.1f;
    float power = 4.0f;
    uint32_t kernelSize = 16;
    int32_t blurRadius = 2;
};

class SettingsManager : public RefCountPtr {
public:
    SettingsManager();

    SSAOControlData& GetSSAOControlData() { return mSSAOControlData; }

    uint32_t GetShadowMapResolution() { return mShadowMapResolution; }
    float& GetShadowFrustumWidth() { return mShadowFrustumWidth; }
    float& GetShadowDistance() { return mShadowDistance; }
    float& GetShadowNear() { return mShadowNear; }
    float& GetShadowFar() { return mShadowFar; }
    glm::vec3& GetLightDir() { return mLightDir; }
    glm::vec3& GetLightRadiance() { return mLightRadiance; }
    float& GetESMControl() { return mShadowESMControl; }
    float& GetShadowBlurRadius() { return mShadowBlurRadius; }

    void SetShadowMapResolution(const uint32_t shadowMapResolution) {
        mShadowMapResolution = shadowMapResolution;
    }
    void SetProbeResolution(const uint32_t probeResolution) { mProbeResolution = probeResolution; }
    void SetRaysPerProbe(const uint32_t raysPerProbe) { mRaysPerProbe = raysPerProbe; }

    glm::uvec3& GetProbeGridCount() { return mProbeGridCount; }
    uint32_t GetProbeResolution() { return mProbeResolution; }
    glm::vec3& GetProbeSpacing() { return mProbeSpacing; }
    glm::vec3& GetProbeGridOrigin() { return mProbeOrigin; }
    float& GetProbeMaxRayLength() { return mProbeMaxRayLength; }
    float& GetProbeMinRayLength() { return mProbeMinRayLength; }
    float& GetHysteresis() { return mHysteresis; }
    float& GetProbeRadius() { return mProbeRadius; }
    uint32_t GetProbeRayCount() { return mRaysPerProbe; }
    bool& GetRenderProbes() { return mRenderProbes; }
    float& GetEnergyPreservation() { return mEnergyPreservation; }

    float& GetDirectWeight() { return mDirectWeight; }
    float& GetIndirectDiffuseWeight() { return mIndirectDiffuseWeight; }
    float& GetIndirectGlossyWeight() { return mIndirectGlossyWeight; }

    float& GetDepthSigma() { return mDepthSigma; }
    float& GetSpatialSigma() { return mSpatialSigma; }
    float& GetGlossyDepthBias() { return mGlossyDepthBias; }
    float& GetGlossyHitDepthBias() { return mGlossyHitDepthBias; }

    ~SettingsManager();

    static const std::array<uint32_t, 6> ShadowMapResolutions;
    static const std::array<uint32_t, 5> ProbeResolutions;
    static const std::array<uint32_t, 7> RaysPerProbe;

private:
    SSAOControlData mSSAOControlData;
    uint32_t mShadowMapResolution;
    float mShadowFrustumWidth;
    float mShadowDistance;
    float mShadowNear;
    float mShadowFar;
    glm::vec3 mLightDir;
    glm::vec3 mLightRadiance;
    float mShadowESMControl;
    float mShadowBlurRadius;

    // DGGI
    glm::uvec3 mProbeGridCount;
    uint32_t mProbeResolution;
    glm::vec3 mProbeSpacing;
    glm::vec3 mProbeOrigin;
    float mProbeMaxRayLength;
    float mProbeMinRayLength;
    float mHysteresis;
    float mProbeRadius;
    uint32_t mRaysPerProbe;
    bool mRenderProbes;
    float mEnergyPreservation;

    // Debug
    float mDirectWeight;
    float mIndirectDiffuseWeight;
    float mIndirectGlossyWeight;

    // Glossy reflections
    float mDepthSigma;
    float mSpatialSigma;
    float mGlossyDepthBias;
    float mGlossyHitDepthBias;
};
}  // namespace VKRT