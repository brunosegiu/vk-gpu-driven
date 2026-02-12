#include "ShadowRenderer.h"

#include "DebugUtils.h"
#include "Texture.h"

namespace VKRT {

struct LightData {
    glm::vec3 radiance;
    glm::vec3 direction;
    glm::mat4 viewProjection;
    glm::mat4 view;
    float shadowFar;
    float shadowNear;
    float esmExp;
    float shadowMapBlurRadius;
    float directWeight;
    float indirectDiffuseWeight;
    float indirectGlossyWeight;
};

struct ShadowControlData {
    float blurriness;
    float radius;
};

ShadowRenderer::ShadowRenderer(
    ScopedRefPtr<Context> context,
    ScopedRefPtr<Scene> scene,
    ScopedRefPtr<SettingsManager> settingsManager)
    : mContext(context), mScene(scene), mSettingsManager(settingsManager) {
    for (const Material::AlphaMode alphaMode : Material::AlphaModes) {
        if (alphaMode == Material::AlphaMode::Blended) {
            continue;
        }
        mVisibilityManagers[alphaMode] = new VisibilityManager(mContext, mScene, alphaMode);
    }
}

void ShadowRenderer::AddRenderTargets() {
    mShadowDepth = new Texture(
        mContext,
        mSettingsManager->GetShadowMapResolution(),
        mSettingsManager->GetShadowMapResolution(),
        vk::Format::eD16Unorm,
        vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
        vk::ImageLayout::eShaderReadOnlyOptimal);

    mDepthOnlyPassRenderTarget = new RenderTarget(mContext, mShadowDepth);

    for (uint32_t passIndex = 0; passIndex < 2; ++passIndex) {
        mShadowMap[passIndex] = new Texture(
            mContext,
            mSettingsManager->GetShadowMapResolution(),
            mSettingsManager->GetShadowMapResolution(),
            vk::Format::eR32Sfloat,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);
        mShadowMapRenderTarget[passIndex] = new RenderTarget(mContext, mShadowMap[passIndex]);

        mBlurPass[passIndex] = new RenderPass(
            mContext,
            {.renderTarget = mShadowMapRenderTarget[passIndex],
             .loadOp = vk::AttachmentLoadOp::eClear,
             .initialLayout = vk::ImageLayout::eUndefined,
             .storeOp = vk::AttachmentStoreOp::eStore,
             .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal});
    }

    mDepthOnlyPass = new RenderPass(
        mContext,
        {{.renderTarget = mShadowMapRenderTarget[1],
          .loadOp = vk::AttachmentLoadOp::eClear,
          .initialLayout = vk::ImageLayout::eUndefined,
          .storeOp = vk::AttachmentStoreOp::eStore,
          .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal},
         {.renderTarget = mDepthOnlyPassRenderTarget,
          .loadOp = vk::AttachmentLoadOp::eClear,
          .initialLayout = vk::ImageLayout::eUndefined,
          .storeOp = vk::AttachmentStoreOp::eStore,
          .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal}});
}

void ShadowRenderer::AddPipelines() {
    for (auto& visEntry : mVisibilityManagers) {
        visEntry.second->AddPipelines();
    }

    std::unordered_map<
        Material::AlphaMode,
        std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>>>
        stages{
            {Material::AlphaMode::Opaque,
             {
                 {vk::ShaderStageFlagBits::eVertex, {Resource::Id::DepthOnlyOpaqueVertexShader}},
                 {vk::ShaderStageFlagBits::eFragment,
                  {Resource::Id::DepthOnlyOpaqueFragmentShader}},
             }},
            {Material::AlphaMode::Masked,
             {
                 {vk::ShaderStageFlagBits::eVertex,
                  {Resource::Id::DepthOnlyAlphaMaskedVertexShader}},
                 {vk::ShaderStageFlagBits::eFragment,
                  {Resource::Id::DepthOnlyAlphaMaskedFragmentShader}},
             }}};

    for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
        if (alphaMode != Material::AlphaMode::Blended) {  // Blended materials aren't shadow casters
            ScopedRefPtr<GraphicsPipeline>& pipeline = mShadowPassPipelines[alphaMode];

            VertexAttributeFlag geometryFlags =
                alphaMode == Material::AlphaMode::Opaque
                    ? VertexAttributeFlag::Position
                    : VertexAttributeFlag(
                          VertexAttributeFlag::Position | VertexAttributeFlag::NormalTexCoordTangent);
            const std::vector<GeometryLayout> geometryLayout =
                MeshSystem::GetGeometryLayout(geometryFlags);

            pipeline = new GraphicsPipeline(
                mContext,
                stages[alphaMode],
                mDepthOnlyPass,
                geometryLayout,
                GraphicsPipelineOptionals{
                    .enableDepthBias = true,
                    .enableCulling = true,
                    .depthBias = 0.01,
                    .depthSlope = 1.3,
                });
        }
    }

    mBlurPipeline[0] = new GraphicsPipeline(
        mContext,
        {
            {vk::ShaderStageFlagBits::eVertex,
             {Resource::Id::ShadowMomentsBlurHorizontalVertexShader}},
            {vk::ShaderStageFlagBits::eFragment,
             {Resource::Id::ShadowMomentsBlurHorizontalFragmentShader}},
        },
        mBlurPass[0],
        std::vector<GeometryLayout>{},
        {
            .enableDepthTest = false,
        });

    mBlurPipeline[1] = new GraphicsPipeline(
        mContext,
        {
            {vk::ShaderStageFlagBits::eVertex,
             {Resource::Id::ShadowMomentsBlurVerticalVertexShader}},
            {vk::ShaderStageFlagBits::eFragment,
             {Resource::Id::ShadowMomentsBlurVerticalFragmentShader}},
        },
        mBlurPass[1],
        std::vector<GeometryLayout>{},
        {
            .enableDepthTest = false,
        });
}

void ShadowRenderer::RemoveRenderTargets() {
    mDepthOnlyPass = nullptr;
    mDepthOnlyPassRenderTarget = nullptr;
    mShadowDepth = nullptr;

    mBlurPass[0] = nullptr;
    mShadowMapRenderTarget[0] = nullptr;
    mShadowMap[0] = nullptr;

    mBlurPass[1] = nullptr;
    mShadowMapRenderTarget[1] = nullptr;
    mShadowMap[1] = nullptr;
}

void ShadowRenderer::RemovePipelines() {
    mShadowPassPipelines.clear();
}

void ShadowRenderer::AddResources() {
    // Shadow pass parameters

    for (auto& visEntry : mVisibilityManagers) {
        visEntry.second->AddResources();
    }

    {
        mShadowCameraUniform = mContext->GetDevice()->CreateBuffers(
            mContext->GetMaxInFlightFrameCount(),
            sizeof(LightData),
            vk::BufferUsageFlagBits::eUniformBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);
    }

    vk::SamplerCreateInfo frameBufferSamplerCreateInfo =
        vk::SamplerCreateInfo()
            .setMagFilter(vk::Filter::eLinear)
            .setMinFilter(vk::Filter::eLinear)
            .setMipmapMode(vk::SamplerMipmapMode::eLinear)
            .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
            .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
            .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
            .setMipLodBias(0.0f)
            .setCompareOp(vk::CompareOp::eNever)
            .setMinLod(0.0f)
            .setMaxLod(0.0f)
            .setAnisotropyEnable(false);
    mShadowSampler = VKRT_ASSERT_VK(
        mContext->GetDevice()->GetLogicalDevice().createSampler(frameBufferSamplerCreateInfo));
}

void ShadowRenderer::UpdatePersistentUniforms(const PersistentParameters& paramaters) {
    for (auto& visEntry : mVisibilityManagers) {
        visEntry.second->UpdatePersistentUniforms(paramaters.scenePersistentDataBuffer);
    }

    mShadowPassPipelines[Material::AlphaMode::Opaque]->Bind(
        0,
        paramaters.scenePersistentDataBuffer);

    mShadowPassPipelines[Material::AlphaMode::Masked]->Bind(
        0,
        paramaters.scenePersistentDataBuffer);
    mShadowPassPipelines[Material::AlphaMode::Masked]->Bind(1, paramaters.materialSampler);
    mShadowPassPipelines[Material::AlphaMode::Masked]->Bind(2, paramaters.materialUniform);
    mShadowPassPipelines[Material::AlphaMode::Masked]->Bind(3, paramaters.sceneTextures);

    mBlurPipeline[0]->Bind(0, mShadowSampler);
    mBlurPipeline[0]->Bind(1, mShadowMap[1]);

    mBlurPipeline[1]->Bind(0, mShadowSampler);
    mBlurPipeline[1]->Bind(1, mShadowMap[0]);
}

void ShadowRenderer::UpdateUniforms(
    const uint32_t frameIndex,
    Camera* camera,
    const PerFrameParameters& parameters) {
    // Shadow camera parameters
    {
        const glm::mat4 shadowView = mScene->GetLight().ComputeShadowView(
            camera->GetPosition(),
            mSettingsManager->GetShadowDistance());

        const glm::mat4 shadowProjection = mScene->GetLight().ComputeShadowProjection(
            mSettingsManager->GetShadowFrustumWidth(),
            mSettingsManager->GetShadowNear(),
            mSettingsManager->GetShadowFar());

        for (const Material::AlphaMode alphaMode : Material::AlphaModes) {
            uint32_t drawCallCount = mScene->GetDrawCallCount(alphaMode);
            if (alphaMode == Material::AlphaMode::Blended || drawCallCount == 0) {
                continue;
            }

            VisibilityManager::CullData shadowCullingData{
                .ortho = 1,
                .viewDirectionOrCameraPos = mScene->GetLight().GetDirection(),
                .frustumPlanes = ViewFrustum(shadowProjection * shadowView).GetPlanes(),
                .globalDrawOffset = static_cast<uint32_t>(mScene->GetDrawCallOffset(alphaMode)),
                .maxDrawCount = static_cast<uint32_t>(mScene->GetDrawCallCount(alphaMode))};

            mVisibilityManagers[alphaMode]->UpdateUniforms(
                shadowCullingData,
                frameIndex,
                parameters.meshDataBuffer);
        }

        DirectionalLight& light = mScene->GetLight();
        LightData cameraMatrices{
            .radiance = light.GetRadiance(),
            .direction = light.GetDirection(),
            .viewProjection = shadowProjection * shadowView,
            .view = shadowView,
            .shadowFar = mSettingsManager->GetShadowFar(),
            .shadowNear = mSettingsManager->GetShadowNear(),
            .esmExp = mSettingsManager->GetESMControl(),
            .shadowMapBlurRadius = mSettingsManager->GetShadowBlurRadius(),
            .directWeight = mSettingsManager->GetDirectWeight(),
            .indirectDiffuseWeight = mSettingsManager->GetIndirectDiffuseWeight(),
            .indirectGlossyWeight = mSettingsManager->GetIndirectGlossyWeight(),
        };
        mShadowCameraUniform[frameIndex]->Write(cameraMatrices);
    }

    for (const Material::AlphaMode alphaMode : Material::AlphaModes) {
        uint32_t drawCallCount = mScene->GetDrawCallCount(alphaMode);
        if (alphaMode == Material::AlphaMode::Blended || drawCallCount == 0) {
            continue;
        }
        mShadowPassPipelines[alphaMode]->Bind(frameIndex, 0, mShadowCameraUniform[frameIndex]);
        mShadowPassPipelines[alphaMode]->Bind(frameIndex, 1, parameters.meshDataBuffer[frameIndex]);
        mShadowPassPipelines[alphaMode]->Bind(
            frameIndex,
            2,
            mVisibilityManagers[alphaMode]->GetAdditionalDrawDataBuffer(frameIndex));
    }

    {
        mBlurPipeline[0]->Bind(frameIndex, 0, mShadowCameraUniform[frameIndex]);
        mBlurPipeline[1]->Bind(frameIndex, 0, mShadowCameraUniform[frameIndex]);
    }
}

void ShadowRenderer::Render(vk::CommandBuffer commandBuffer, const uint32_t frameIndex) {
    mContext->BeginMarker(commandBuffer, "Shadow pass");
    {
        mContext->BeginMarker(commandBuffer, "Shadow culling");
        for (auto& visEntry : mVisibilityManagers) {
            visEntry.second->Dispatch(commandBuffer, frameIndex);
        }
        mContext->EndMarker(commandBuffer);

        mContext->BeginMarker(commandBuffer, "Shadowmap rendering");
        {
            // Transition shadow map to depth attachment
            std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::PipelineStageFlagBits::eVertexShader,
                {{mShadowDepth,
                  vk::ImageLayout::eShaderReadOnlyOptimal,
                  vk::ImageLayout::eDepthAttachmentOptimal}});

            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::PipelineStageFlagBits::eVertexShader,
                vk::DependencyFlags{},
                {},
                {},
                imageBarriers);
        }

        const std::vector<vk::ClearValue> clearValues{
            vk::ClearColorValue(1.0f, 0.0f, 0.0f, 0.0f),
            vk::ClearDepthStencilValue(1.0f, 0),
        };
        const vk::RenderPassBeginInfo renderPassBeginInfo =
            vk::RenderPassBeginInfo()
                .setRenderPass(mDepthOnlyPass->GetRenderPassHandle())
                .setFramebuffer(mDepthOnlyPass->GetFramebufferHandle())
                .setRenderArea({vk::Offset2D{0, 0}, mShadowDepth->GetExtent()})
                .setClearValues(clearValues);
        commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

        {
            const vk::Viewport viewport{
                0.0f,
                0.0f,
                static_cast<float>(mShadowDepth->GetWidth()),
                static_cast<float>(mShadowDepth->GetHeight()),
                0.0f,
                1.0f};
            commandBuffer.setViewport(0, viewport);

            const vk::Rect2D scissor =
                vk::Rect2D().setOffset(0).setExtent(mShadowDepth->GetExtent());
            commandBuffer.setScissor(0, scissor);
        }

        for (const Material::AlphaMode alphaMode : Material::AlphaModes) {
            // Skip transparent objects while rendering shadows
            uint32_t drawCallCount = mScene->GetDrawCallCount(alphaMode);
            if (alphaMode == Material::AlphaMode::Blended || drawCallCount == 0) {
                continue;
            }
            mContext->BeginMarker(commandBuffer, AlphaModeToStr(alphaMode));

            commandBuffer.bindPipeline(
                vk::PipelineBindPoint::eGraphics,
                mShadowPassPipelines[alphaMode]->GetPipelineHandle());

            std::vector<vk::DescriptorSet> descriptorSets =
                mShadowPassPipelines[alphaMode]->GetDescriptorSets(frameIndex);

            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                mShadowPassPipelines[alphaMode]->GetPipelineLayout(),
                0,
                descriptorSets,
                nullptr);

            {
                mScene->GetMeshSystem()->BindBuffers(
                    commandBuffer,
                    alphaMode == Material::AlphaMode::Opaque
                        ? VertexAttributeFlag::Position
                        : VertexAttributeFlag(
                              VertexAttributeFlag::Position |
                              VertexAttributeFlag::NormalTexCoordTangent));

                uint32_t maxDrawIndirectCount =
                    mContext->GetDevice()->GetDeviceProperties().limits.maxDrawIndirectCount;
                VKRT_ASSERT(maxDrawIndirectCount >= drawCallCount);

                commandBuffer.drawIndexedIndirectCount(
                    mVisibilityManagers[alphaMode]
                        ->GetIndirectDrawBuffer(frameIndex)
                        ->GetBufferHandle(),
                    0,
                    mVisibilityManagers[alphaMode]
                        ->GetIndirectDrawCountBuffer(frameIndex)
                        ->GetBufferHandle(),
                    0,
                    drawCallCount,
                    sizeof(VkDrawIndexedIndirectCommand));
            }
            mContext->EndMarker(commandBuffer);
        }

        commandBuffer.endRenderPass();
        mContext->EndMarker(commandBuffer);

        mContext->BeginMarker(commandBuffer, "Shadowmap blur");
        {
            for (uint32_t passIndex = 0; passIndex < 2; ++passIndex) {
                if (passIndex >= 1) {
                    std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        {{mShadowMap[1],
                          vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::ImageLayout::eColorAttachmentOptimal}});

                    commandBuffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::PipelineStageFlagBits::eColorAttachmentOutput,
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
                        .setRenderPass(mBlurPass[passIndex]->GetRenderPassHandle())
                        .setFramebuffer(mBlurPass[passIndex]->GetFramebufferHandle())
                        .setRenderArea({vk::Offset2D{0, 0}, mShadowMap[passIndex]->GetExtent()})
                        .setClearValues(clearValues);
                commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

                {
                    const vk::Viewport viewport{
                        0.0f,
                        0.0f,
                        static_cast<float>(mShadowMap[passIndex]->GetWidth()),
                        static_cast<float>(mShadowMap[passIndex]->GetHeight()),
                        0.0f,
                        1.0f};
                    commandBuffer.setViewport(0, viewport);

                    const vk::Rect2D scissor = vk::Rect2D().setOffset(0).setExtent(vk::Extent2D{
                        mShadowMap[passIndex]->GetWidth(),
                        mShadowMap[passIndex]->GetHeight()});
                    commandBuffer.setScissor(0, scissor);
                }

                commandBuffer.bindPipeline(
                    vk::PipelineBindPoint::eGraphics,
                    mBlurPipeline[passIndex]->GetPipelineHandle());

                std::vector<vk::DescriptorSet> descriptorSets =
                    mBlurPipeline[passIndex]->GetDescriptorSets(frameIndex);

                commandBuffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    mBlurPipeline[passIndex]->GetPipelineLayout(),
                    0,
                    descriptorSets,
                    nullptr);

                commandBuffer.draw(3, 1, 0, 0);

                commandBuffer.endRenderPass();
            }
        }
        mContext->EndMarker(commandBuffer);
    }
    mContext->EndMarker(commandBuffer);
}

ShadowRenderer::~ShadowRenderer() {
    mContext->GetDevice()->GetLogicalDevice().destroySampler(mShadowSampler);
}

}  // namespace VKRT