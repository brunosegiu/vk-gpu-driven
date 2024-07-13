#include "RenderPass.h"

#include <algorithm>

#include "DebugUtils.h"
#include "Device.h"
#include "Texture.h"
#include "VulkanBuffer.h"

namespace VKRT {

RenderPass::RenderPass(
    ScopedRefPtr<Context> context,
    const std::vector<ScopedRefPtr<RenderTarget>>& renderTargets)
    : mContext(context), mColorAttachmentCount(0) {
    VKRT_ASSERT(!renderTargets.empty());

    for (const ScopedRefPtr<RenderTarget>& renderTarget : renderTargets) {
        if (renderTarget->GetImageAspect() & vk::ImageAspectFlagBits::eColor) {
            ++mColorAttachmentCount;
        }
    }

    std::vector<vk::AttachmentDescription> attachments;
    std::vector<vk::AttachmentReference> colorReferences;
    attachments.reserve(renderTargets.size());
    colorReferences.reserve(renderTargets.size());
    for (uint32_t index = 0; index < mColorAttachmentCount; ++index) {
        const ScopedRefPtr<RenderTarget> renderTarget = renderTargets[index];
        attachments.emplace_back(vk::AttachmentDescription()
                                     .setFormat(renderTarget->GetFormat())
                                     .setSamples(vk::SampleCountFlagBits::e1)
                                     .setLoadOp(vk::AttachmentLoadOp::eClear)
                                     .setStoreOp(vk::AttachmentStoreOp::eStore)
                                     .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                                     .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                                     .setInitialLayout(vk::ImageLayout::eUndefined)
                                     .setFinalLayout(vk::ImageLayout::ePresentSrcKHR));
        colorReferences.emplace_back(vk::AttachmentReference()
                                         .setLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                         .setAttachment(index));
    }

    const auto depthRenderTargetIt = std::find_if(
        renderTargets.begin(),
        renderTargets.end(),
        [](const ScopedRefPtr<RenderTarget>& renderTarget) -> bool {
            return (renderTarget->GetImageAspect() & vk::ImageAspectFlagBits::eDepth) !=
                   vk::ImageAspectFlagBits::eNoneKHR;
        });

    mHasDepthTesting = depthRenderTargetIt != renderTargets.end();

    if (mHasDepthTesting) {
        const ScopedRefPtr<RenderTarget> depthRenderTarget = *depthRenderTargetIt;
        const vk::AttachmentDescription depthAttachmentDescription =
            vk::AttachmentDescription()
                .setFormat(depthRenderTarget->GetFormat())
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
        attachments.emplace_back(depthAttachmentDescription);
    }

    vk::AttachmentReference depthReference =
        vk::AttachmentReference()
            .setAttachment(mColorAttachmentCount)
            .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::SubpassDescription subpass =
        vk::SubpassDescription()
            .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
            .setColorAttachments(colorReferences)
            .setPDepthStencilAttachment(mHasDepthTesting ? &depthReference : nullptr);

    vk::RenderPassCreateInfo renderPassCreateInfo =
        vk::RenderPassCreateInfo().setAttachments(attachments).setSubpasses(subpass);

    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    mRenderPassHandle = VKRT_ASSERT_VK(logicalDevice.createRenderPass(renderPassCreateInfo));

    const uint32_t framebufferCount = renderTargets.front()->GetImageViews().size();
    for (uint32_t framebufferIndex = 0; framebufferIndex < framebufferCount; ++framebufferIndex) {
        std::vector<vk::ImageView> framebufferAttachments;
        for (const ScopedRefPtr<RenderTarget>& renderTarget : renderTargets) {
            const std::vector<vk::ImageView> imageViews = renderTarget->GetImageViews();
            const vk::ImageView imageView = framebufferIndex >= imageViews.size()
                                                ? imageViews.front()
                                                : imageViews[framebufferIndex];
            framebufferAttachments.emplace_back(imageView);
        }
        const vk::FramebufferCreateInfo framebufferCreateInfo =
            vk::FramebufferCreateInfo()
                .setRenderPass(mRenderPassHandle)
                .setAttachments(framebufferAttachments)
                .setWidth(renderTargets.front()->GetWidth())
                .setHeight(renderTargets.front()->GetHeight())
                .setLayers(1);

        const vk::Framebuffer framebuffer =
            VKRT_ASSERT_VK(logicalDevice.createFramebuffer(framebufferCreateInfo));

        mFramebufferHandles.emplace_back(framebuffer);
    }
}

RenderPass::RenderPass(
    ScopedRefPtr<Context> context,
    const ScopedRefPtr<RenderTarget>& renderTarget)
    : RenderPass(context, std::vector<ScopedRefPtr<RenderTarget>>{renderTarget}) {}

RenderPass::~RenderPass() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    for (vk::Framebuffer& framebuffer : mFramebufferHandles) {
        logicalDevice.destroyFramebuffer(framebuffer);
    }
    logicalDevice.destroyRenderPass(mRenderPassHandle);
}

}  // namespace VKRT