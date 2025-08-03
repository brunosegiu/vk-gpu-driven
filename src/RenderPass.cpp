#include "RenderPass.h"

#include <algorithm>

#include "DebugUtils.h"
#include "Device.h"
#include "Texture.h"
#include "VulkanBuffer.h"

namespace VKRT {

RenderPass::RenderPass(
    ScopedRefPtr<Context> context,
    const std::vector<RenderTargetBinding>& renderTargetBindings)
    : mContext(context), mColorAttachmentCount(0) {
    VKRT_ASSERT(!renderTargetBindings.empty());

    for (const RenderTargetBinding& renderTargetBinding : renderTargetBindings) {
        if (renderTargetBinding.renderTarget->GetImageAspect() & vk::ImageAspectFlagBits::eColor) {
            ++mColorAttachmentCount;
        }
    }

    std::vector<vk::AttachmentDescription> attachments;
    std::vector<vk::AttachmentReference> colorReferences;
    attachments.reserve(renderTargetBindings.size());
    colorReferences.reserve(renderTargetBindings.size());
    for (uint32_t index = 0; index < mColorAttachmentCount; ++index) {
        const RenderTargetBinding& renderTargetBinding = renderTargetBindings[index];
        const ScopedRefPtr<RenderTarget>& renderTarget = renderTargetBinding.renderTarget;
        attachments.emplace_back(vk::AttachmentDescription()
                                     .setFormat(renderTarget->GetFormat())
                                     .setSamples(vk::SampleCountFlagBits::e1)
                                     .setLoadOp(renderTargetBinding.loadOp)
                                     .setStoreOp(renderTargetBinding.storeOp)
                                     .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                                     .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                                     .setInitialLayout(renderTargetBinding.initialLayout)
                                     .setFinalLayout(renderTargetBinding.finalLayout));
        colorReferences.emplace_back(vk::AttachmentReference()
                                         .setLayout(vk::ImageLayout::eColorAttachmentOptimal)
                                         .setAttachment(index));
    }

    const auto depthRenderTargetIt = std::find_if(
        renderTargetBindings.begin(),
        renderTargetBindings.end(),
        [](const RenderTargetBinding& renderTargetBinding) -> bool {
            return (renderTargetBinding.renderTarget->GetImageAspect() &
                    vk::ImageAspectFlagBits::eDepth) != vk::ImageAspectFlagBits::eNoneKHR;
        });

    mHasDepthTesting = depthRenderTargetIt != renderTargetBindings.end();

    if (mHasDepthTesting) {
        const RenderTargetBinding depthRenderTargetBinding = *depthRenderTargetIt;
        const vk::AttachmentDescription depthAttachmentDescription =
            vk::AttachmentDescription()
                .setFormat(depthRenderTargetBinding.renderTarget->GetFormat())
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(depthRenderTargetBinding.loadOp)
                .setStoreOp(depthRenderTargetBinding.storeOp)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(depthRenderTargetBinding.initialLayout)
                .setFinalLayout(depthRenderTargetBinding.finalLayout);
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

    const uint32_t framebufferCount =
        renderTargetBindings.front().renderTarget->GetImageViews().size();
    for (uint32_t framebufferIndex = 0; framebufferIndex < framebufferCount; ++framebufferIndex) {
        std::vector<vk::ImageView> framebufferAttachments;
        for (const RenderTargetBinding& renderTargetBinding : renderTargetBindings) {
            const std::vector<vk::ImageView> imageViews =
                renderTargetBinding.renderTarget->GetImageViews();
            const vk::ImageView imageView = framebufferIndex >= imageViews.size()
                                                ? imageViews.front()
                                                : imageViews[framebufferIndex];
            framebufferAttachments.emplace_back(imageView);
        }
        const vk::FramebufferCreateInfo framebufferCreateInfo =
            vk::FramebufferCreateInfo()
                .setRenderPass(mRenderPassHandle)
                .setAttachments(framebufferAttachments)
                .setWidth(renderTargetBindings.front().renderTarget->GetWidth())
                .setHeight(renderTargetBindings.front().renderTarget->GetHeight())
                .setLayers(1);

        const vk::Framebuffer framebuffer =
            VKRT_ASSERT_VK(logicalDevice.createFramebuffer(framebufferCreateInfo));

        mFramebufferHandles.emplace_back(framebuffer);
    }
}

RenderPass::RenderPass(
    ScopedRefPtr<Context> context,
    const RenderTargetBinding& renderTargetBinding)
    : RenderPass(context, std::vector<RenderTargetBinding>{renderTargetBinding}) {}

RenderPass::~RenderPass() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    for (vk::Framebuffer& framebuffer : mFramebufferHandles) {
        logicalDevice.destroyFramebuffer(framebuffer);
    }
    logicalDevice.destroyRenderPass(mRenderPassHandle);
}

}  // namespace VKRT