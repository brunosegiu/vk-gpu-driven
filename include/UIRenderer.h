#pragma once

#include "Context.h"
#include "GraphicsPipeline.h"
#include "RefCountPtr.h"
#include "SettingsManager.h"

namespace VKRT {

class UIRenderer : public RefCountPtr, public InputEventListener {
public:
    UIRenderer(ScopedRefPtr<Context> context, ScopedRefPtr<SettingsManager> settingsManager);

    void AddRenderTargets(ScopedRefPtr<RenderTarget> uiTarget);
    void AddResources();

    void RemoveRenderTargets();

    void Update();

    void Render(vk::CommandBuffer commandBuffer);

    ~UIRenderer();

private:
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<SettingsManager> mSettingsManager;
    vk::DescriptorPool mDescriptorPool;
    ScopedRefPtr<RenderTarget> mUITarget;
    ScopedRefPtr<RenderPass> mRenderPass;

    std::vector<std::string> mShadowResolutionOptions;
    int32_t mSelectedShadowResolutionIndex;

    std::vector<std::string> mProbeResolutionOptions;
    int32_t mSelectedProbeResolutionIndex;

    std::vector<std::string> mRaysPerProbeOptions;
    int32_t mSelectedRaysPerProbeIndex;

    glm::vec2 mLightPitchYaw;
};
}  // namespace VKRT