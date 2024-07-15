#include "Renderer.h"

#include <random>

#include "DebugUtils.h"
#include "Texture.h"

namespace VKRT {
Renderer::Renderer(ScopedRefPtr<Context> context, ScopedRefPtr<Scene> scene)
    : mContext(context), mScene(scene), mCurrentFrameIndex(0) {
    ScopedRefPtr<InputManager> inputManager = mContext->GetWindow()->GetInputManager();
    inputManager->Subscribe(this);

    const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();
    mDepthBuffer = new Texture(
        mContext,
        imageSize.width,
        imageSize.height,
        vk::Format::eD32Sfloat,
        vk::ImageUsageFlagBits::eDepthStencilAttachment);
    mDepthRenderTarget = new RenderTarget(mContext, mDepthBuffer);

    mCommandRing = new CommandRing(mContext);
    mRenderTarget = new RenderTarget(mContext, mContext->GetSwapchain()->GetRenderTargets());
    mRenderPass = new RenderPass(context, {mRenderTarget, mDepthRenderTarget});

    {
        mMainPassParameters = new ShaderParameterCollection(mContext);
        mCameraUniform = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eUniformBuffer,
            ShaderParameter::UpdateFrequency::PerFrame,
            vk::ShaderStageFlagBits::eVertex,
            sizeof(CameraProperties),
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        mMainPassParameters->AddParameter(mCameraUniform);

        mPushConstant = new ShaderParameterPushConstant(
            mContext,
            vk::ShaderStageFlagBits::eVertex,
            sizeof(glm::mat4));
        mMainPassParameters->AddParameter(mPushConstant);

        std::unordered_map<vk::ShaderStageFlagBits, Resource::Id> stages{
            {vk::ShaderStageFlagBits::eVertex, Resource::Id::VertexShader},
            {vk::ShaderStageFlagBits::eFragment, Resource::Id::FragmentShader},
        };

        mMainPassPipeline = new Pipeline(context, mMainPassParameters, stages, mRenderPass);
    }
}

void Renderer::UpdateCameraUniforms(Camera* camera, uint32_t imageIndex) {
    ScopedRefPtr<VulkanBuffer> buffer = mCameraUniform->GetBuffer(imageIndex);
    uint8_t* mappedBuffer = buffer->MapBuffer();
    CameraProperties cameraMatrices{
        .view = camera->GetViewTransform(),
        .projection = camera->GetProjectionTransform()};
    std::copy_n(
        reinterpret_cast<uint8_t*>(&cameraMatrices),
        sizeof(CameraProperties),
        mappedBuffer);
    buffer->UnmapBuffer();
}

void Renderer::Render(Camera* camera) {
    mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mContext->GetMaxInFlightFrameCount();
    CommandRing::CommandResources command = mCommandRing->Cycle();
    mContext->GetSwapchain()->AcquireNextImage(mCurrentFrameIndex);
    {
        VKRT_ASSERT_VK(command.buffer.begin(vk::CommandBufferBeginInfo{}));

        // Create and update all buffers and textures
        {
            UpdateCameraUniforms(camera, mCurrentFrameIndex);
            mMainPassParameters->CreateDescriptorSets();
            mMainPassParameters->UpdateDescriptors(mCurrentFrameIndex);
        }

        const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();

        {
            const std::vector<vk::ClearValue> clearValues{
                vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
                vk::ClearDepthStencilValue(1.0f, 0),
            };
            const vk::RenderPassBeginInfo renderPassBeginInfo =
                vk::RenderPassBeginInfo()
                    .setRenderPass(mRenderPass->GetRenderPassHandle())
                    .setFramebuffer(mRenderPass->GetFramebufferHandle(
                        mContext->GetSwapchain()->GetCurrentIndex()))
                    .setRenderArea({vk::Offset2D{0, 0}, imageSize})
                    .setClearValues(clearValues);
            command.buffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        }

        {
            const vk::Viewport viewport{
                0.0f,
                0.0f,
                static_cast<float>(imageSize.width),
                static_cast<float>(imageSize.height),
                0.0f,
                1.0f};
            command.buffer.setViewport(0, viewport);

            const vk::Rect2D scissor = vk::Rect2D().setOffset(0).setExtent(
                vk::Extent2D{imageSize.width, imageSize.height});
            command.buffer.setScissor(0, scissor);
        }

        command.buffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            mMainPassPipeline->GetPipelineHandle());

        std::vector<vk::DescriptorSet> descriptorSets =
            mMainPassParameters->GetDescriptorSets(mCurrentFrameIndex);

        command.buffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mMainPassPipeline->GetPipelineLayout(),
            0,
            descriptorSets,
            nullptr);

        mScene->Draw(
            command.buffer,
            [this](
                vk::CommandBuffer commandBuffer,
                ScopedRefPtr<Object> object,
                ScopedRefPtr<Mesh> mesh) {
                commandBuffer.pushConstants<glm::mat4>(
                    mMainPassPipeline->GetPipelineLayout(),
                    mPushConstant->GetStageFlags(),
                    mPushConstant->GetOffset(),
                    object->GetAbsoluteTransform());
            });

        command.buffer.endRenderPass();

        VKRT_ASSERT_VK(command.buffer.end());
    }

    mContext->GetSwapchain()->Present(command.buffer, command.fence, mCurrentFrameIndex);
}

void Renderer::OnKeyPressed(int key) {}

void Renderer::OnKeyReleased(int key) {}

void Renderer::OnMouseMoved(glm::vec2 newPos) {}

void Renderer::OnLeftMouseButtonPressed() {}

void Renderer::OnLeftMouseButtonReleased() {}

void Renderer::OnRightMouseButtonPressed() {}

void Renderer::OnRightMouseButtonReleased() {}

Renderer::~Renderer() {
    mCommandRing->Flush();
    ScopedRefPtr<InputManager> inputManager = mContext->GetWindow()->GetInputManager();
    inputManager->Unsuscribe(this);
}

}  // namespace VKRT