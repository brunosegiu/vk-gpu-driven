#include "SettingsManager.h"

#include "DebugUtils.h"

namespace VKRT {

const std::array<uint32_t, 6>
    SettingsManager::ShadowMapResolutions{256, 512, 1024, 2048, 4096, 8192};

const std::array<uint32_t, 5> SettingsManager::ProbeResolutions{16, 32, 64, 128, 256};

const std::array<uint32_t, 7> SettingsManager::RaysPerProbe{8, 16, 32, 64, 128, 256, 512};

SettingsManager::SettingsManager()
    : mSSAOControlData(),
      mShadowTaps(16),
      mShadowMapResolution(ShadowMapResolutions[4]),
      mShadowFrustumWidth(50.0f),
      mShadowDistance(500.0f),
      mShadowNear(1.0f),
      mShadowFar(1500.0f),
      mProbeGridCount(24u, 12u, 24u),
      mProbeResolution(ProbeResolutions[2]),
      mProbeSpacing(3.0f, 2.5f, 3.0f),
      mProbeOrigin(-42.0f, -1.0f, -44.0f),
      mProbeMaxRayLength(1000.0f),
      mProbeMinRayLength(0.50f),
      mHysteresis(0.95),
      mProbeRadius(0.15f),
      mRaysPerProbe(RaysPerProbe[2]),
      mRenderProbes(true) {}

SettingsManager::~SettingsManager() {}

}  // namespace VKRT