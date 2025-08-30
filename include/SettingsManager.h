#pragma once

#include <glm/glm.hpp>

#include "RefCountPtr.h"

namespace VKRT {

struct SSAOControlData {
    float radius = 2.0f;
    float power = 2.0f;
    uint32_t kernelSize = 32;
    int32_t blurRadius = 2;
};

class SettingsManager : public RefCountPtr {
public:
    SettingsManager();

    SSAOControlData& GetSSAOControlData() { return mSSAOControlData; }

    uint32_t& GetShadowTaps() { return mShadowTaps; }
    uint32_t& GetShadowMapResolution() { return mShadowMapResolution; }
    float& GetShadowFrustumWidth() { return mShadowFrustumWidth; }
    float& GetShadowDistance() { return mShadowDistance; }
    float& GetShadowNear() { return mShadowNear; }
    float& GetShadowFar() { return mShadowFar; }

    glm::uvec3& GetProbeGridCount() { return mProbeGridCount; }
    glm::uvec2& GetProbeResolution() { return mProbeResolution; }
    glm::vec3& GetProbeSpacing() { return mProbeSpacing; }
    glm::vec3& GetProbeGridOrigin() { return mProbeOrigin; }
    float& GetProbeMaxRayLength() { return mProbeMaxRayLength; }
    float& GetProbeMinRayLength() { return mProbeMinRayLength; }
    float& GetHysteresis() { return mHysteresis; }
    uint32_t& GetProbeRayCount() { return mRaysPerProbe; }

    ~SettingsManager();

private:
    SSAOControlData mSSAOControlData;
    uint32_t mShadowTaps;
    uint32_t mShadowMapResolution;
    float mShadowFrustumWidth;
    float mShadowDistance;
    float mShadowNear;
    float mShadowFar;

    // DGGI
    glm::uvec3 mProbeGridCount;
    glm::uvec2 mProbeResolution;
    glm::vec3 mProbeSpacing;
    glm::vec3 mProbeOrigin;
    float mProbeMaxRayLength;
    float mProbeMinRayLength;
    float mHysteresis;
    uint32_t mRaysPerProbe;
};
}  // namespace VKRT