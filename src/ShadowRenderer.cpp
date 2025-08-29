#include "ShadowRenderer.h"

#include "DebugUtils.h"
#include "Texture.h"

namespace VKRT {

constexpr uint32_t ShadowMapResolution = 4096;

struct LightData {
    glm::vec3 radiance;
    glm::vec3 direction;
    glm::mat4 viewProjection;
    uint32_t shadowTaps;
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
    // Shadow pass
    {
        mShadowMap = new Texture(
            mContext,
            ShadowMapResolution,
            ShadowMapResolution,
            vk::Format::eD16Unorm,
            vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        mDepthOnlyPassRenderTarget = new RenderTarget(mContext, mShadowMap);
        mDepthOnlyPass = new RenderPass(
            mContext,
            {.renderTarget = mDepthOnlyPassRenderTarget,
             .loadOp = vk::AttachmentLoadOp::eClear,
             .initialLayout = vk::ImageLayout::eUndefined,
             .storeOp = vk::AttachmentStoreOp::eStore,
             .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal});
    }
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
                          VertexAttributeFlag::Position | VertexAttributeFlag::TexCoord);
            const std::vector<GeometryLayout> geometryLayout =
                MeshSystem::GetGeometryLayout(geometryFlags);

            pipeline = new GraphicsPipeline(
                mContext,
                stages[alphaMode],
                mDepthOnlyPass,
                geometryLayout,
                GraphicsPipelineOptionals{
                    .enableDepthBias = true,
                    .depthBias = 1.0f,
                    .depthSlope = 1.0f * (2 + 1)});
        }
    }
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
        ParameterUpdateFrequency::Once,
        0,
        paramaters.scenePersistentDataBuffer);

    mShadowPassPipelines[Material::AlphaMode::Masked]->Bind(
        ParameterUpdateFrequency::Once,
        0,
        paramaters.scenePersistentDataBuffer);
    mShadowPassPipelines[Material::AlphaMode::Masked]->Bind(
        ParameterUpdateFrequency::Once,
        1,
        paramaters.materialSampler);
    mShadowPassPipelines[Material::AlphaMode::Masked]->Bind(
        ParameterUpdateFrequency::Once,
        2,
        paramaters.materialUniform);
    mShadowPassPipelines[Material::AlphaMode::Masked]->Bind(
        ParameterUpdateFrequency::Once,
        3,
        paramaters.sceneTextures);
}

void ShadowRenderer::UpdateUniforms(
    const uint32_t frameIndex,
    const PerFrameParameters& parameters) {
    // Shadow camera parameters
    {
        for (const Material::AlphaMode alphaMode : Material::AlphaModes) {
            uint32_t drawCallCount = mScene->GetDrawCallCount(alphaMode);
            if (alphaMode == Material::AlphaMode::Blended || drawCallCount == 0) {
                continue;
            }

            VisibilityManager::CullData shadowCullingData{
                .ortho = 1,
                .viewDirectionOrCameraPos = mScene->GetLight().GetDirection(),
                .frustumPlanes = ViewFrustum(mScene->GetLight().ComputeShadowMatrix()).GetPlanes(),
                .globalDrawOffset = static_cast<uint32_t>(mScene->GetDrawCallOffset(alphaMode)),
                .maxDrawCount = static_cast<uint32_t>(mScene->GetDrawCallCount(alphaMode))};

            mVisibilityManagers[alphaMode]->UpdateUniforms(
                shadowCullingData,
                frameIndex,
                parameters.meshDataBuffer);
        }

        DirectionalLight& light = mScene->GetLight();
        glm::mat4 shadowMatrix = light.ComputeShadowMatrix();
        LightData cameraMatrices{
            .radiance = light.GetRadiance(),
            .direction = light.GetDirection(),
            .viewProjection = shadowMatrix,
            .shadowTaps = mSettingsManager->GetShadowTaps()};
        mShadowCameraUniform[frameIndex]->Write(cameraMatrices);
    }

    for (const Material::AlphaMode alphaMode : Material::AlphaModes) {
        uint32_t drawCallCount = mScene->GetDrawCallCount(alphaMode);
        if (alphaMode == Material::AlphaMode::Blended || drawCallCount == 0) {
            continue;
        }
        mShadowPassPipelines[alphaMode]->Bind(
            ParameterUpdateFrequency::PerFrame,
            0,
            mShadowCameraUniform);
        mShadowPassPipelines[alphaMode]->Bind(
            ParameterUpdateFrequency::PerFrame,
            1,
            parameters.meshDataBuffer);
        mShadowPassPipelines[alphaMode]->Bind(
            ParameterUpdateFrequency::PerFrame,
            2,
            mVisibilityManagers[alphaMode]->GetAdditionalDrawDataBuffers());
    }
}

void ShadowRenderer::Render(vk::CommandBuffer commandBuffer, const uint32_t frameIndex) {
    // Shadow pass
    {
        for (auto& visEntry : mVisibilityManagers) {
            visEntry.second->Dispatch(commandBuffer, frameIndex);
        }

        mContext->BeginMarker(commandBuffer, "Shadowmap Rendering");
        {
            // Transition shadow map to depth attachment
            std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::PipelineStageFlagBits::eVertexShader,
                {{mShadowMap,
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
            vk::ClearDepthStencilValue(1.0f, 0),
        };
        const vk::RenderPassBeginInfo renderPassBeginInfo =
            vk::RenderPassBeginInfo()
                .setRenderPass(mDepthOnlyPass->GetRenderPassHandle())
                .setFramebuffer(mDepthOnlyPass->GetFramebufferHandle())
                .setRenderArea({vk::Offset2D{0, 0}, mShadowMap->GetExtent()})
                .setClearValues(clearValues);
        commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

        {
            const vk::Viewport viewport{
                0.0f,
                0.0f,
                static_cast<float>(mShadowMap->GetWidth()),
                static_cast<float>(mShadowMap->GetHeight()),
                0.0f,
                1.0f};
            commandBuffer.setViewport(0, viewport);

            const vk::Rect2D scissor = vk::Rect2D().setOffset(0).setExtent(mShadowMap->GetExtent());
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
                              VertexAttributeFlag::Position | VertexAttributeFlag::TexCoord));

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
    }
}

ShadowRenderer::~ShadowRenderer() {
    mContext->GetDevice()->GetLogicalDevice().destroySampler(mShadowSampler);
}

}  // namespace VKRT