#pragma once

#include "Context.h"
#include "GraphicsPipeline.h"
#include "RefCountPtr.h"

namespace VKRT {

struct SSAOControlData {
    float radius = 2.0f;
    float power = 2.0f;
    uint32_t kernelSize = 32;
    int32_t blurRadius = 2;
};

class UIRenderer : public RefCountPtr, public InputEventListener {
public:
    UIRenderer(ScopedRefPtr<Context> context, ScopedRefPtr<RenderTarget> uITarget);

    void Update();

    void Render(vk::CommandBuffer commandBuffer);

    const SSAOControlData& GetSSAOControlData() { return mSSAOControlData; }
    const uint32_t& GetShadowTaps() { return mShadowTaps; }

    ~UIRenderer();
private:
    ScopedRefPtr<Context> mContext;
    vk::DescriptorPool mDescriptorPool;
    ScopedRefPtr<RenderTarget> mUITarget;
    ScopedRefPtr<RenderPass> mRenderPass;
    SSAOControlData mSSAOControlData;
    uint32_t mShadowTaps;
};
}  // namespace VKRT