#pragma once

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

    ~SettingsManager();

private:
    SSAOControlData mSSAOControlData;
    uint32_t mShadowTaps;
};
}  // namespace VKRT