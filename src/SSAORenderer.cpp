#include "SSAORenderer.h"

#include "DebugUtils.h"

namespace VKRT {

SSAORenderer::SSAORenderer(
    ScopedRefPtr<Context> context,
    ScopedRefPtr<Scene> scene,
    ScopedRefPtr<SettingsManager> settingsManager)
    : mContext(context), mSettingsManager(settingsManager) {
    AddRenderTargets();
    AddPipelines();
    AddResources();
}

void SSAORenderer::AddRenderTargets() {
    const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();
    // SSAO resources
    {
        mSSAOBuffer = new Texture(
            mContext,
            imageSize.width,
            imageSize.height,
            vk::Format::eR16Unorm,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
        mSSAORenderTarget = new RenderTarget(mContext, mSSAOBuffer);

        mSSAOPass = new RenderPass(
            mContext,
            {{.renderTarget = mSSAORenderTarget,
              .loadOp = vk::AttachmentLoadOp::eClear,
              .initialLayout = vk::ImageLayout::eUndefined,
              .storeOp = vk::AttachmentStoreOp::eStore,
              .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal}});

        mSSAOBlurredBuffer = new Texture(
            mContext,
            imageSize.width,
            imageSize.height,
            vk::Format::eR16Unorm,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
        mSSAOBlurredRenderTarget = new RenderTarget(mContext, mSSAOBlurredBuffer);

        mSSAOBlurPass = new RenderPass(
            mContext,
            {{.renderTarget = mSSAOBlurredRenderTarget,
              .loadOp = vk::AttachmentLoadOp::eClear,
              .initialLayout = vk::ImageLayout::eUndefined,
              .storeOp = vk::AttachmentStoreOp::eStore,
              .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal}});
    }
}

void SSAORenderer::AddPipelines() {
    // SSAO resources
    {
        std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>> stages{
            {vk::ShaderStageFlagBits::eVertex, {Resource::Id::SSAOVertexShader}},
            {vk::ShaderStageFlagBits::eFragment, {Resource::Id::SSAOFragmentShader}},
        };

        mSSAOPipeline = new GraphicsPipeline(
            mContext,
            stages,
            mSSAOPass,
            std::vector<GeometryLayout>{},
            {.enableDepthTest = false});
    }

    // SSAO blur resources
    {
        std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>> stages{
            {vk::ShaderStageFlagBits::eVertex, {Resource::Id::EdgeAwareBoxBlurVertexShader}},
            {vk::ShaderStageFlagBits::eFragment, {Resource::Id::EdgeAwareBoxBlurFragmentShader}},
        };

        mSSAOBlurPipeline = new GraphicsPipeline(
            mContext,
            stages,
            mSSAOBlurPass,
            std::vector<GeometryLayout>{},
            {.enableDepthTest = false});
    }
}

void SSAORenderer::AddResources() {
    mSSAOControlBuffer = mContext->GetDevice()->CreateBuffers(
        mContext->GetMaxInFlightFrameCount(),
        sizeof(SSAOControlData),
        vk::BufferUsageFlagBits::eUniformBuffer,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
}

void SSAORenderer::RemoveRenderTargets() {
    mSSAOBuffer = nullptr;
    mSSAOPass = nullptr;
    mSSAOBlurredBuffer = nullptr;
    mSSAOBlurPass = nullptr;
}

void SSAORenderer::RemovePipelines() {
    mSSAOPipeline = nullptr;
    mSSAOBlurPipeline = nullptr;
}

void SSAORenderer::RemoveResources() {
    mSSAOControlBuffer.clear();
}

void SSAORenderer::UpdatePersistentUniforms(const PersistentParameters& parameters) {
    mSSAOPipeline->Bind(0, parameters.mFrameBufferSampler);
    mSSAOPipeline->Bind(1, parameters.mDepthBuffer);

    mSSAOBlurPipeline->Bind(0, parameters.mFrameBufferSampler);
    mSSAOBlurPipeline->Bind(1, parameters.mDepthBuffer);
    mSSAOBlurPipeline->Bind(2, mSSAOBuffer);
}

void SSAORenderer::UpdateUniforms(const PerFrameParameters& parameters, uint32_t frameIndex) {
    mSSAOPipeline->Bind(frameIndex, 0, parameters.mCameraUniform[frameIndex]);
    mSSAOPipeline->Bind(frameIndex, 1, mSSAOControlBuffer[frameIndex]);

    mSSAOBlurPipeline->Bind(frameIndex, 0, parameters.mCameraUniform[frameIndex]);
    mSSAOBlurPipeline->Bind(frameIndex, 1, mSSAOControlBuffer[frameIndex]);

    // SSAO
    {
        const SSAOControlData& ssaoControl = mSettingsManager->GetSSAOControlData();
        mSSAOControlBuffer[frameIndex]->Write(ssaoControl);
    }
}

void SSAORenderer::Render(
    vk::CommandBuffer commandBuffer,
    const uint32_t frameIndex,
    const ScopedRefPtr<Texture>& depthBuffer) {
    // SSAO pass
    const vk::Extent2D imageSize(mSSAOBuffer->GetWidth(), mSSAOBuffer->GetHeight());
    {
        mContext->BeginMarker(commandBuffer, "SSAO");
        {
            mContext->BeginMarker(commandBuffer, "SSAO draw");
            {
                std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    {{depthBuffer,
                      vk::ImageLayout::eDepthAttachmentOptimal,
                      vk::ImageLayout::eShaderReadOnlyOptimal}});

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::DependencyFlags{},
                    {},
                    {},
                    imageBarriers);
            }

            const std::vector<vk::ClearValue> clearValues{
                vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f),
            };
            const vk::RenderPassBeginInfo renderPassBeginInfo =
                vk::RenderPassBeginInfo()
                    .setRenderPass(mSSAOPass->GetRenderPassHandle())
                    .setFramebuffer(mSSAOPass->GetFramebufferHandle())
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
                mSSAOPipeline->GetPipelineHandle());

            std::vector<vk::DescriptorSet> descriptorSets =
                mSSAOPipeline->GetDescriptorSets(frameIndex);

            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                mSSAOPipeline->GetPipelineLayout(),
                0,
                descriptorSets,
                nullptr);

            commandBuffer.draw(3, 1, 0, 0);

            commandBuffer.endRenderPass();
            mContext->EndMarker(commandBuffer);
            mContext->BeginMarker(commandBuffer, "SSAO blur");
            {
                const std::vector<vk::ClearValue> clearValues{
                    vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f),
                };
                const vk::RenderPassBeginInfo renderPassBeginInfo =
                    vk::RenderPassBeginInfo()
                        .setRenderPass(mSSAOBlurPass->GetRenderPassHandle())
                        .setFramebuffer(mSSAOBlurPass->GetFramebufferHandle())
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
                    mSSAOBlurPipeline->GetPipelineHandle());

                std::vector<vk::DescriptorSet> descriptorSets =
                    mSSAOBlurPipeline->GetDescriptorSets(frameIndex);

                commandBuffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    mSSAOBlurPipeline->GetPipelineLayout(),
                    0,
                    descriptorSets,
                    nullptr);

                commandBuffer.draw(3, 1, 0, 0);

                commandBuffer.endRenderPass();
                mContext->EndMarker(commandBuffer);
            }
        }
        mContext->EndMarker(commandBuffer);
    }
}

SSAORenderer::~SSAORenderer() {}

}  // namespace VKRT