#include "SettingsManager.h"

#include "DebugUtils.h"

namespace VKRT {

const std::array<uint32_t, 6>
    SettingsManager::ShadowMapResolutions{256, 512, 1024, 2048, 4096, 8192};

const std::array<uint32_t, 5> SettingsManager::ProbeResolutions{16, 32, 64, 128, 256};

const std::array<uint32_t, 7> SettingsManager::RaysPerProbe{8, 16, 32, 64, 128, 256, 512};

const std::array<ToneMapper, 3> SettingsManager::ToneMapOptions{
    ToneMapper::ACES,
    ToneMapper::Reinhard,
    ToneMapper::Uncharted2};

SettingsManager::SettingsManager()
    : mSSAOControlData(),
      mShadowMapResolution(ShadowMapResolutions[4]),
      mShadowFrustumWidth(50.0f),
      mShadowDistance(100.0f),
      mShadowNear(1.0f),
      mShadowFar(500.0f),
      mLightDir(0.0f, -1.0f, 0.0f),
      mLightRadiance(1.0f),
      mShadowESMControl(300.0f),
      mShadowBlurRadius(0.15f),
      mProbeGridCount(12u, 12u, 12u),
      mProbeResolution(ProbeResolutions[2]),
      mProbeSpacing(5.0f, 2.5f, 5.0f),
      mProbeOrigin(-24.0f, -2.0f, -29.0f),
      mProbeMaxRayLength(1000.0f),
      mProbeMinRayLength(0.02f),
      mHysteresis(0.95),
      mProbeRadius(0.1f),
      mRaysPerProbe(RaysPerProbe[2]),
      mRenderProbes(true),
      mEnergyPreservation(0.5f),
      mDirectWeight(1.0f),
      mIndirectDiffuseWeight(1.0f),
      mIndirectGlossyWeight(1.0f),
      mDepthSigma(0.5f),
      mSpatialSigma(1.5f),
      mGlossyDepthBias(1.0),
      mGlossyHitDepthBias(0.0),
      mEnableFXAA(true),
      mTonemapper(ToneMapper::ACES),
      mFXAAMaxSpan(8.0),
      mFXAAReduceMin(128.0f) {}

SettingsManager::~SettingsManager() {}

}  // namespace VKRT