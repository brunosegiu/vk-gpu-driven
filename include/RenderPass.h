#pragma once

#include <vulkan/vulkan.hpp>

#include "Texture.h"
#include "RenderTarget.h"

namespace VKRT {
class RenderPass : public RefCountPtr {
public:
    RenderPass(
        ScopedRefPtr<Context> context,
        const std::vector<ScopedRefPtr<RenderTarget>>& renderTargets);

    RenderPass(
        ScopedRefPtr<Context> context, const ScopedRefPtr<RenderTarget>& renderTarget);

    const vk::RenderPass& GetRenderPassHandle() const { return mRenderPassHandle; };
    const vk::Framebuffer& GetFramebufferHandle(uint32_t index = 0) const {
        return mFramebufferHandles[index];
    };

    const bool& GetHasDepthTesting() const { return mHasDepthTesting; }

    ~RenderPass();

private:
    ScopedRefPtr<Context> mContext;

    vk::RenderPass mRenderPassHandle;
    std::vector<vk::Framebuffer> mFramebufferHandles;
    uint32_t mColorAttachmentCount;
    bool mHasDepthTesting;
};
}  // namespace VKRT
