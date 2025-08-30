#include "SettingsManager.h"

#include "DebugUtils.h"

namespace VKRT {

SettingsManager::SettingsManager()
    : mSSAOControlData(),
      mShadowTaps(16),
      mShadowMapResolution(2048),
      mShadowFrustumWidth(50.0f),
      mShadowDistance(500.0f),
      mShadowNear(1.0f),
      mShadowFar(1500.0f),
      mProbeGridCount(16u, 16u, 16u),
      mProbeResolution(128u, 128u),
      mProbeSpacing(1.0f, 1.0f, 1.0f),
      mProbeOrigin(glm::vec3(0.0f, 1.5f, 0.0f) - glm::vec3(mProbeGridCount) * mProbeSpacing * 0.5f),
      mProbeMaxRayLength(1000.0f),
      mProbeMinRayLength(0.01f),
      mHysteresis(0.78),
      mRaysPerProbe(128) {}

SettingsManager::~SettingsManager() {}

}  // namespace VKRT