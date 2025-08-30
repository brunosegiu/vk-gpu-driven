#include "SettingsManager.h"

#include "DebugUtils.h"

namespace VKRT {

SettingsManager::SettingsManager()
    : mSSAOControlData(),
      mShadowTaps(16),
      mShadowMapResolution(4096),
      mShadowFrustumWidth(50.0f),
      mShadowDistance(500.0f),
      mShadowNear(1.0f),
      mShadowFar(1500.0f),
      mProbeGridCount(24u, 12u, 24u),
      mProbeResolution(64u),
      mProbeSpacing(3.0f, 2.5f, 3.0f),
      mProbeOrigin(-42.0f, 0.5f, -44.0f),
      mProbeMaxRayLength(1000.0f),
      mProbeMinRayLength(0.01f),
      mHysteresis(0.95),
      mProbeRadius(0.15f),
      mRaysPerProbe(32u),
      mRenderProbes(true) {}

SettingsManager::~SettingsManager() {}

}  // namespace VKRT