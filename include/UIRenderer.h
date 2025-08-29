#pragma once

#include "Context.h"
#include "GraphicsPipeline.h"
#include "RefCountPtr.h"
#include "SettingsManager.h"

namespace VKRT {

class UIRenderer : public RefCountPtr, public InputEventListener {
public:
    UIRenderer(
        ScopedRefPtr<Context> context,
        ScopedRefPtr<SettingsManager> settingsManager);

    void AddRenderTargets(ScopedRefPtr<RenderTarget> uiTarget);
    void AddResources();

    void Update();

    void Render(vk::CommandBuffer commandBuffer);

    ~UIRenderer();

private:
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<SettingsManager> mSettingsManager;
    vk::DescriptorPool mDescriptorPool;
    ScopedRefPtr<RenderTarget> mUITarget;
    ScopedRefPtr<RenderPass> mRenderPass;
};
}  // namespace VKRT