#pragma once

#include "Context.h"
#include "GraphicsPipeline.h"
#include "RefCountPtr.h"

namespace VKRT {
class UIRenderer : public RefCountPtr, public InputEventListener {
public:
    UIRenderer(ScopedRefPtr<Context> context, ScopedRefPtr<RenderTarget> uITarget);

    void Render(vk::CommandBuffer commandBuffer);

    ~UIRenderer();
private:
    ScopedRefPtr<Context> mContext;
    vk::DescriptorPool mDescriptorPool;
    ScopedRefPtr<RenderTarget> mUITarget;
    ScopedRefPtr<RenderPass> mRenderPass;
};
}  // namespace VKRT