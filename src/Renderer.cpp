#include "Renderer.h"

#include <random>

#include "DebugUtils.h"
#include "Texture.h"

namespace VKRT {
Renderer::Renderer(ScopedRefPtr<Context> context, ScopedRefPtr<Scene> scene)
    : mContext(context), mScene(scene), mCurrentFrameIndex(0), mMaterialsBuffer(nullptr) {
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
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
            sizeof(PerDrawParameters));
        mMainPassParameters->AddParameter(mPushConstant);

        const float anisotropy =
            mContext->GetDevice()->GetDeviceProperties().limits.maxSamplerAnisotropy;
        vk::SamplerCreateInfo samplerCreateInfo =
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                .setMipLodBias(0.0f)
                .setCompareOp(vk::CompareOp::eNever)
                .setMinLod(0.0f)
                .setMaxLod(0.0f)
                .setAnisotropyEnable(true)
                .setMaxAnisotropy(anisotropy);
        mMaterialSampler = new ShaderParameterSampler(
            mContext,
            vk::ShaderStageFlagBits::eFragment,
            samplerCreateInfo);
        mMainPassParameters->AddParameter(mMaterialSampler);

        mMaterialsUniform = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eStorageBuffer,
            ShaderParameter::UpdateFrequency::Once,
            vk::ShaderStageFlagBits::eFragment);
        mMainPassParameters->AddParameter(mMaterialsUniform);

        mMaterialsTextures = new ShaderParameterImage(
            mContext,
            vk::DescriptorType::eSampledImage,
            vk::ShaderStageFlagBits::eFragment,
            4096,
            true);
        mMainPassParameters->AddParameter(mMaterialsTextures);

        std::unordered_map<vk::ShaderStageFlagBits, Resource::Id> stages{
            {vk::ShaderStageFlagBits::eVertex, Resource::Id::VertexShader},
            {vk::ShaderStageFlagBits::eFragment, Resource::Id::FragmentShader},
        };

        const std::vector<GeometryLayout> geometryLayout{
            {.format = vk::Format::eR32G32B32A32Sfloat, .stride = sizeof(glm::vec3)},
            {.format = vk::Format::eR32Uint, .stride = sizeof(uint32_t)},
            {.format = vk::Format::eR32Uint, .stride = sizeof(uint32_t)},
        };

        mMainPassPipeline =
            new Pipeline(context, mMainPassParameters, stages, mRenderPass, geometryLayout);
    }
}

void Renderer::UpdateCameraUniforms(Camera* camera, uint32_t imageIndex) {
    ScopedRefPtr<VulkanBuffer> buffer = mCameraUniform->GetBuffer(imageIndex);
    uint8_t* mappedBuffer = buffer->MapBuffer();
    CameraProperties cameraMatrices{
        .viewProjection = camera->GetProjectionTransform() * camera->GetViewTransform(),
    };
    std::copy_n(
        reinterpret_cast<uint8_t*>(&cameraMatrices),
        sizeof(CameraProperties),
        mappedBuffer);
    buffer->UnmapBuffer();
}

void Renderer::UpdateMaterialUniform() {
    if (mMaterialsBuffer == nullptr) {
        uint32_t descriptorCount = 0;
        Scene::SceneMaterials sceneMaterials = mScene->GetMaterialProxies();
        size_t materialBufferSize = sceneMaterials.materials.size() * sizeof(Scene::MaterialProxy);
        VKRT_ASSERT(materialBufferSize > 0);
        mMaterialsBuffer = mContext->GetDevice()->CreateBuffer(
            materialBufferSize,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        uint8_t* buffer = mMaterialsBuffer->MapBuffer();
        std::copy_n(
            reinterpret_cast<const uint8_t*>(sceneMaterials.materials.data()),
            materialBufferSize,
            buffer);
        mMaterialsBuffer->UnmapBuffer();
        descriptorCount = sceneMaterials.textures.size();
        for (const ScopedRefPtr<Texture>& materialTexture : sceneMaterials.textures) {
            mMaterialsTextures->Bind(materialTexture);
        }
        { mMaterialsUniform->BindBuffer(mMaterialsBuffer); }
    }
}

void Renderer::Render(Camera* camera) {
    mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mContext->GetMaxInFlightFrameCount();
    CommandRing::CommandResources command = mCommandRing->Cycle();
    mScene->Lock();
    mContext->GetSwapchain()->AcquireNextImage(mCurrentFrameIndex);
    {
        VKRT_ASSERT_VK(command.buffer.begin(vk::CommandBufferBeginInfo{}));

        // Create and update all buffers and textures
        {
            UpdateCameraUniforms(camera, mCurrentFrameIndex);
            UpdateMaterialUniform();
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
            camera,
            [&](vk::CommandBuffer commandBuffer,
                ScopedRefPtr<Object> object,
                ScopedRefPtr<Mesh> mesh) {
                PerDrawParameters parameters{
                    .transform = object->GetAbsoluteTransform(),
                    .materialId = mesh->GetMaterial()->GetMaterialId(),
                };
                commandBuffer.pushConstants<PerDrawParameters>(
                    mMainPassPipeline->GetPipelineLayout(),
                    mPushConstant->GetStageFlags(),
                    mPushConstant->GetOffset(),
                    parameters);
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