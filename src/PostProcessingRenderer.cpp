#include "PostProcessingRenderer.h"

#include "DebugUtils.h"

namespace VKRT {

PostProcessingRenderer::PostProcessingRenderer(
    ScopedRefPtr<Context> context,
    ScopedRefPtr<Scene> scene,
    ScopedRefPtr<SettingsManager> settingsManager)
    : mContext(context), mSettingsManager(settingsManager) {
}

struct PostProcessingControlData {
    ToneMapper tonemapper;
    uint32_t fxaa;
    float fxaaMaxSpan;
    float fxaaReduceMin;
};

void PostProcessingRenderer::AddRenderTargets(ScopedRefPtr<RenderTarget> dstTarget) {
    const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();
    mPostProcessingPass = new RenderPass(
        mContext,
        {{.renderTarget = dstTarget,
          .loadOp = vk::AttachmentLoadOp::eClear,
          .initialLayout = vk::ImageLayout::eUndefined,
          .storeOp = vk::AttachmentStoreOp::eStore,
          .finalLayout = vk::ImageLayout::eColorAttachmentOptimal}});
}

void PostProcessingRenderer::AddPipelines() {
    {
        std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>> stages{
            {vk::ShaderStageFlagBits::eVertex, {Resource::Id::PostProcessingVertexShader}},
            {vk::ShaderStageFlagBits::eFragment, {Resource::Id::PostProcessingFragmentShader}},
        };

        mPostProcessingPipeline = new GraphicsPipeline(
            mContext,
            stages,
            mPostProcessingPass,
            std::vector<GeometryLayout>{},
            {.enableDepthTest = false});
    }
}

void PostProcessingRenderer::AddResources() {
    mPostProcessingControlBuffer = mContext->GetDevice()->CreateBuffers(
        mContext->GetMaxInFlightFrameCount(),
        sizeof(PostProcessingControlData),
        vk::BufferUsageFlagBits::eUniformBuffer,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
}

void PostProcessingRenderer::RemoveRenderTargets() {
    mPostProcessingPass = nullptr;
}

void PostProcessingRenderer::RemovePipelines() {
    mPostProcessingPipeline = nullptr;
}

void PostProcessingRenderer::RemoveResources() {
    mPostProcessingControlBuffer.clear();
}

void PostProcessingRenderer::UpdatePersistentUniforms(const PersistentParameters& parameters) {
    mPostProcessingPipeline->Bind(0, parameters.mFrameBufferSampler);
    mPostProcessingPipeline->Bind(1, parameters.mScreenTexture);
}

void PostProcessingRenderer::UpdateUniforms(uint32_t frameIndex) {
    mPostProcessingPipeline->Bind(frameIndex, 0, mPostProcessingControlBuffer[frameIndex]);
    {
        const PostProcessingControlData postProcessingControl {
            .tonemapper = mSettingsManager->GetTonemapper(),
            .fxaa = mSettingsManager->GetEnableFXAA() ? 1u : 0u,
            .fxaaMaxSpan = mSettingsManager->GetFXAAMaxSpan(),
            .fxaaReduceMin = 1.0f / mSettingsManager->GetFXAAReduceMin(),
        };
        mPostProcessingControlBuffer[frameIndex]->Write(postProcessingControl);
    }
}

void PostProcessingRenderer::Render(vk::CommandBuffer commandBuffer, const uint32_t frameIndex) {
    const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();
    {
        mContext->BeginMarker(commandBuffer, "Post-processing");
        {
            const std::vector<vk::ClearValue> clearValues{
                vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f),
            };
            const vk::RenderPassBeginInfo renderPassBeginInfo =
                vk::RenderPassBeginInfo()
                    .setRenderPass(mPostProcessingPass->GetRenderPassHandle())
                    .setFramebuffer(mPostProcessingPass->GetFramebufferHandle(
                        mContext->GetSwapchain()->GetCurrentIndex()))
                    .setRenderArea({vk::Offset2D{0, 0}, imageSize})
                    .setClearValues(clearValues);
            commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

            {
                const vk::Viewport viewport{
                    0.0f,
                    0.0f,
                    static_cast<float>(imageSize.width),
                    static_cast<float>(imageSize.height),
                    0.0f,
                    1.0f};
                commandBuffer.setViewport(0, viewport);

                const vk::Rect2D scissor = vk::Rect2D().setOffset(0).setExtent(
                    vk::Extent2D{imageSize.width, imageSize.height});
                commandBuffer.setScissor(0, scissor);
            }

            commandBuffer.bindPipeline(
                vk::PipelineBindPoint::eGraphics,
                mPostProcessingPipeline->GetPipelineHandle());

            std::vector<vk::DescriptorSet> descriptorSets =
                mPostProcessingPipeline->GetDescriptorSets(frameIndex);

            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                mPostProcessingPipeline->GetPipelineLayout(),
                0,
                descriptorSets,
                nullptr);

            commandBuffer.draw(3, 1, 0, 0);

            commandBuffer.endRenderPass();
        }
        mContext->EndMarker(commandBuffer);
    }
}

PostProcessingRenderer::~PostProcessingRenderer() {}

}  // namespace VKRT