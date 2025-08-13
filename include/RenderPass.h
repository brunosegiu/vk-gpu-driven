#pragma once

#include <vulkan/vulkan.hpp>

#include "Texture.h"
#include "RenderTarget.h"

namespace VKRT {
class RenderPass : public RefCountPtr {
public:
    struct RenderTargetBinding {
        ScopedRefPtr<RenderTarget> renderTarget = nullptr;
        vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear;
        vk::ImageLayout initialLayout = vk::ImageLayout::eUndefined;
        vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore;
        vk::ImageLayout finalLayout = vk::ImageLayout::eUndefined;
    };
    RenderPass(
        ScopedRefPtr<Context> context,
        const std::vector<RenderTargetBinding>& renderTargetBindings);

    RenderPass(ScopedRefPtr<Context> context, const RenderTargetBinding& renderTarget);

    const vk::RenderPass& GetRenderPassHandle() const { return mRenderPassHandle; };
    const vk::Framebuffer& GetFramebufferHandle(uint32_t index = 0) const {
        return mFramebufferHandles[index];
    };
    const bool& GetHasDepthTesting() const { return mHasDepthTesting; }
    const uint32_t& GetColorAttachmentCount() const { return mColorAttachmentCount; }

    ~RenderPass();

private:
    ScopedRefPtr<Context> mContext;

    vk::RenderPass mRenderPassHandle;
    std::vector<vk::Framebuffer> mFramebufferHandles;
    uint32_t mColorAttachmentCount;
    bool mHasDepthTesting;
};
}  // namespace VKRT
