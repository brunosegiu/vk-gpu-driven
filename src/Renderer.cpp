#include "Renderer.h"

#include <random>

#include "DebugUtils.h"
#include "Texture.h"

namespace VKRT {

struct CameraData {
    glm::mat4 projection;
    glm::mat4 viewProjection;
    glm::mat4 invViewProjection;
    glm::mat4 invProjection;
    glm::mat4 invView;
    glm::vec4 cameraPos;
    float _near;
    float _far;
    float glossyDepthBias;
    float glossyHitDepthBias;
};

Renderer::Renderer(
    ScopedRefPtr<Context> context,
    ScopedRefPtr<Scene> scene,
    ScopedRefPtr<SettingsManager> settingsManager)
    : mContext(context),
      mScene(scene),
      mSettingsManager(settingsManager),
      mCurrentFrameIndex(0),
      mMaterialsUniform(),
      mScenePersistentDataBuffer(),
      mHasResouces(false),
      mHasBoundResources(false) {
    ScopedRefPtr<InputManager> inputManager = mContext->GetWindow()->GetInputManager();
    inputManager->Subscribe(this);

    mCommandRing = new CommandRing(mContext);

    for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
        mVisibilityManagers[alphaMode] = new VisibilityManager(mContext, mScene, alphaMode);
    }
    mUIRenderer = new UIRenderer(mContext, mSettingsManager);
    mDDGIRenderer = new DDGIRenderer(mContext, mScene, mSettingsManager);
    mShadowRenderer = new ShadowRenderer(mContext, mScene, mSettingsManager);
    mSSAORenderer = new SSAORenderer(mContext, mScene, mSettingsManager);
    mReflectionsRenderer = new GlossyReflectionsRenderer(mContext, mScene, mSettingsManager);
    mPostProcessingRenderer = new PostProcessingRenderer(mContext, mScene, mSettingsManager);

    AddRenderTargets();
    AddPipelines();
}

void Renderer::AddRenderTargets() {
    const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();

    // Base pass resouces
    {
        mDepthBuffer = new Texture(
            mContext,
            imageSize.width,
            imageSize.height,
            vk::Format::eD32Sfloat,
            vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled);
        mDepthRenderTarget = new RenderTarget(mContext, mDepthBuffer);

        mMainRenderTarget =
            new RenderTarget(mContext, mContext->GetSwapchain()->GetRenderTargets());

        mVisibilityBuffer = new Texture(
            mContext,
            imageSize.width,
            imageSize.height,
            vk::Format::eR32Uint,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);
        mVisibilityBufferRT = new RenderTarget(mContext, mVisibilityBuffer);

        mGeometryPass = new RenderPass(
            mContext,
            {{.renderTarget = mVisibilityBufferRT,
              .loadOp = vk::AttachmentLoadOp::eClear,
              .initialLayout = vk::ImageLayout::eUndefined,
              .storeOp = vk::AttachmentStoreOp::eStore,
              .finalLayout = vk::ImageLayout::eColorAttachmentOptimal},
             {.renderTarget = mDepthRenderTarget,
              .loadOp = vk::AttachmentLoadOp::eClear,
              .initialLayout = vk::ImageLayout::eUndefined,
              .storeOp = vk::AttachmentStoreOp::eStore,
              .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal}});
    }

    // Shading + transparent render targets
    {
        mHDRTarget = new Texture(
            mContext,
            imageSize.width,
            imageSize.height,
            vk::Format::eR16G16B16A16Sfloat,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);
        mHDRTargetRT = new RenderTarget(mContext, mHDRTarget);

        mShadePass = new RenderPass(
            mContext,
            {{.renderTarget = mHDRTargetRT,
              .loadOp = vk::AttachmentLoadOp::eClear,
              .initialLayout = vk::ImageLayout::eUndefined,
              .storeOp = vk::AttachmentStoreOp::eStore,
              .finalLayout = vk::ImageLayout::eColorAttachmentOptimal}});

        mTransparentPass = new RenderPass(
            mContext,
            {
                {.renderTarget = mHDRTargetRT,
                 .loadOp = vk::AttachmentLoadOp::eLoad,
                 .initialLayout = vk::ImageLayout::eColorAttachmentOptimal,
                 .storeOp = vk::AttachmentStoreOp::eStore,
                 .finalLayout = vk::ImageLayout::eColorAttachmentOptimal},
                {.renderTarget = mDepthRenderTarget,
                 .loadOp = vk::AttachmentLoadOp::eLoad,
                 .initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                 .storeOp = vk::AttachmentStoreOp::eStore,
                 .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal},
            });
    }

    mUIRenderer->AddRenderTargets(mMainRenderTarget);
    mDDGIRenderer->AddRenderTargets(mHDRTargetRT, mDepthRenderTarget);
    mShadowRenderer->AddRenderTargets();
    mSSAORenderer->AddRenderTargets();
    mReflectionsRenderer->AddRenderTargets();
    mPostProcessingRenderer->AddRenderTargets(mMainRenderTarget);
}

void Renderer::AddPipelines() {
    // Culling resources
    for (auto& visEntry : mVisibilityManagers) {
        visEntry.second->AddPipelines();
    }

    // Base pass resources
    {
        std::unordered_map<
            Material::AlphaMode,
            std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>>>
            stages{
                {Material::AlphaMode::Opaque,
                 {
                     {vk::ShaderStageFlagBits::eVertex,
                      {Resource::Id::GeometryPassOpaqueVertexShader}},
                     {vk::ShaderStageFlagBits::eFragment,
                      {Resource::Id::GeometryPassOpaqueFragmentShader}},
                 }},
                {Material::AlphaMode::Masked,
                 {
                     {vk::ShaderStageFlagBits::eVertex,
                      {Resource::Id::GeometryPassAlphaMaskedVertexShader}},
                     {vk::ShaderStageFlagBits::eFragment,
                      {Resource::Id::GeometryPassAlphaMaskedFragmentShader}},
                 }},
                {Material::AlphaMode::Blended,
                 {
                     {vk::ShaderStageFlagBits::eVertex,
                      {Resource::Id::TransparentPassVertexShader}},
                     {vk::ShaderStageFlagBits::eFragment,
                      {Resource::Id::TransparentPassFragmentShader}},
                 }}};

        for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
            bool isTransparentPass = alphaMode == Material::AlphaMode::Blended;
            bool isAlphaMaskedPass = alphaMode == Material::AlphaMode::Masked;
            ScopedRefPtr<GraphicsPipeline>& pipeline = mGeometryPassPipelines[alphaMode];

            const VertexAttributeFlag geometryFlags =
                isTransparentPass ? VertexAttributeFlag::All
                : isAlphaMaskedPass
                    ? VertexAttributeFlag(
                          VertexAttributeFlag::Position | VertexAttributeFlag::TexCoord)
                    : VertexAttributeFlag::Position;
            const std::vector<GeometryLayout> geometryLayout =
                MeshSystem::GetGeometryLayout(geometryFlags);

            pipeline = new GraphicsPipeline(
                mContext,
                stages[alphaMode],
                isTransparentPass ? mTransparentPass : mGeometryPass,
                geometryLayout,
                {.enableCulling = !isAlphaMaskedPass, .enableBlending = isTransparentPass});
        }
    }

    // Shade pass resources
    {
        std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>> stages{
            {vk::ShaderStageFlagBits::eVertex, {Resource::Id::VisibilityBufferShadeVertexShader}},
            {vk::ShaderStageFlagBits::eFragment,
             {Resource::Id::VisibilityBufferShadeFragmentShader}},
        };

        mShadePassPipeline = new GraphicsPipeline(
            mContext,
            stages,
            mShadePass,
            std::vector<GeometryLayout>{},
            {.enableDepthTest = false});
    }

    mDDGIRenderer->AddPipelines();
    mShadowRenderer->AddPipelines();
    mSSAORenderer->AddPipelines();
    mReflectionsRenderer->AddPipelines();
    mPostProcessingRenderer->AddPipelines();
}

void Renderer::AddResources() {
    mHasResouces = true;

    // Persistent resouces
    const Scene::PackedDrawData& drawData = mScene->GetPackedDrawData();
    {
        const size_t perDrawBufferSize =
            drawData.persistentDrawData.size() * sizeof(Scene::PersistentDrawData);
        VKRT_ASSERT(perDrawBufferSize > 0);
        mScenePersistentDataBuffer = mContext->GetDevice()->CreateBuffer(
            perDrawBufferSize,
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);
    }

    // Update materials
    {
        Scene::SceneMaterials sceneMaterials = mScene->GetMaterialProxies();
        size_t materialBufferSize = sceneMaterials.materials.size() * sizeof(Scene::MaterialProxy);
        VKRT_ASSERT(materialBufferSize > 0);
        mMaterialsUniform = mContext->GetDevice()->CreateBuffer(
            materialBufferSize,
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);
    }

    // Per-frame resources
    {
        const size_t perMeshBufferSize = drawData.perMeshData.size() * sizeof(Scene::MeshData);

        const uint32_t bufferCount = mContext->GetMaxInFlightFrameCount();
        VKRT_ASSERT(perMeshBufferSize > 0);
        for (uint32_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex) {
            ScopedRefPtr<VulkanBuffer> perMeshBuffer = mContext->GetDevice()->CreateBuffer(
                perMeshBufferSize,
                vk::BufferUsageFlagBits::eStorageBuffer,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                    VMA_ALLOCATION_CREATE_MAPPED_BIT);
            mPerMeshBuffers.push_back(perMeshBuffer);
        }

        mCameraUniform = mContext->GetDevice()->CreateBuffers(
            mContext->GetMaxInFlightFrameCount(),
            sizeof(CameraData),
            vk::BufferUsageFlagBits::eUniformBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);
    }

    {
        const float anisotropy =
            mContext->GetDevice()->GetDeviceProperties().limits.maxSamplerAnisotropy;
        vk::SamplerCreateInfo materialSamplerCreateInfo =
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
        mMaterialSampler = VKRT_ASSERT_VK(
            mContext->GetDevice()->GetLogicalDevice().createSampler(materialSamplerCreateInfo));

        vk::SamplerCreateInfo frameBufferSamplerCreateInfo =
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eNearest)
                .setMinFilter(vk::Filter::eNearest)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                .setMipLodBias(0.0f)
                .setCompareOp(vk::CompareOp::eNever)
                .setMinLod(0.0f)
                .setMaxLod(0.0f)
                .setAnisotropyEnable(false);

        mFrameBufferSampler = VKRT_ASSERT_VK(
            mContext->GetDevice()->GetLogicalDevice().createSampler(frameBufferSamplerCreateInfo));

        vk::SamplerCreateInfo irradianceSamplerCreateInfo =
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
                .setMaxLod(10.0f)
                .setAnisotropyEnable(false);
        mIrradianceSampler = VKRT_ASSERT_VK(
            mContext->GetDevice()->GetLogicalDevice().createSampler(irradianceSamplerCreateInfo));
    }

    for (auto& visEntry : mVisibilityManagers) {
        visEntry.second->AddResources();
    }

    mUIRenderer->AddResources();

    mDDGIRenderer->AddResources();

    mShadowRenderer->AddResources();

    mSSAORenderer->AddResources();

    mReflectionsRenderer->AddResources();

    mPostProcessingRenderer->AddResources();
}

void Renderer::RemoveRenderTargets() {
    mShadePass = nullptr;
    mTransparentPass = nullptr;
    mVisibilityBuffer = nullptr;
    mGeometryPass = nullptr;

    mMainRenderTarget = nullptr;

    mDepthRenderTarget = nullptr;
    mDepthBuffer = nullptr;

    mDDGIRenderer->RemoveRenderTargets();

    mShadowRenderer->RemoveRenderTargets();

    mSSAORenderer->RemoveRenderTargets();

    mReflectionsRenderer->RemoveRenderTargets();

    mPostProcessingRenderer->RemoveRenderTargets();
}

void Renderer::RemovePipelines() {
    mGeometryPassPipelines.clear();
    mShadePassPipeline = nullptr;

    for (auto& visEntry : mVisibilityManagers) {
        visEntry.second->RemovePipelines();
    }

    mDDGIRenderer->RemovePipelines();

    mShadowRenderer->RemovePipelines();

    mSSAORenderer->RemovePipelines();

    mReflectionsRenderer->RemovePipelines();

    mPostProcessingRenderer->RemovePipelines();
}

void Renderer::RemoveResources() {}

void Renderer::UpdatePersistentUniforms() {
    mHasBoundResources = true;

    for (auto& visEntry : mVisibilityManagers) {
        visEntry.second->UpdatePersistentUniforms({mScenePersistentDataBuffer});
    }

    const Scene::PackedDrawData& drawData = mScene->GetPackedDrawData();
    {
        const size_t perDrawBufferSize =
            drawData.persistentDrawData.size() * sizeof(Scene::PersistentDrawData);

        {
            uint8_t* buffer = mScenePersistentDataBuffer->MapBuffer();
            std::copy_n(
                reinterpret_cast<const uint8_t*>(drawData.persistentDrawData.data()),
                drawData.persistentDrawData.size() * sizeof(Scene::PersistentDrawData),
                buffer);
            mScenePersistentDataBuffer->UnmapBuffer();
        }
    }

    // Update materials
    {
        uint32_t descriptorCount = 0;
        Scene::SceneMaterials sceneMaterials = mScene->GetMaterialProxies();
        mMaterialsUniform->Write(
            reinterpret_cast<const uint8_t* const>(sceneMaterials.materials.data()),
            size_t(mMaterialsUniform->GetBufferSize()));
        descriptorCount = sceneMaterials.textures.size();
        for (const ScopedRefPtr<Texture>& materialTexture : sceneMaterials.textures) {
            mSceneTextures.push_back(materialTexture);
        }
    }

    // DDGI
    {
        DDGIRenderer::PersistentParameters parameters{
            .mScenePersistentDataParameter = mScenePersistentDataBuffer,
            .mShadowMap = mShadowRenderer->GetShadowMap(),
            .mMaterialSampler = mMaterialSampler,
            .mFrameBufferSampler = mFrameBufferSampler,
            .mMaterialsUniform = mMaterialsUniform,
            .mIndexBufferUniform = mScene->GetMeshSystem()->GetIndexBuffer(),
            .mPositionBufferUniform = mScene->GetMeshSystem()->GetVertexBuffer(),
            .mTexCoordBufferUniform = mScene->GetMeshSystem()->GetTexCoordBuffer(),
            .mNormalBufferUniform = mScene->GetMeshSystem()->GetNormalBuffer(),
            .mTangentBufferUniform = mScene->GetMeshSystem()->GetTangentBuffer(),
            .mMaterialsTextures = mSceneTextures,
        };
        mDDGIRenderer->UpdatePersistentUniforms(parameters);
    }

    {
        GlossyReflectionsRenderer::PersistentParameters parameters{
            .mScenePersistentDataParameter = mScenePersistentDataBuffer,
            .mVisibilityBuffer = mVisibilityBuffer,
            .mShadowMap = mShadowRenderer->GetShadowMap(),
            .mDepthBuffer = mDepthBuffer,
            .mMaterialSampler = mMaterialSampler,
            .mFrameBufferSampler = mFrameBufferSampler,
            .mMaterialsUniform = mMaterialsUniform,
            .mIndexBufferUniform = mScene->GetMeshSystem()->GetIndexBuffer(),
            .mPositionBufferUniform = mScene->GetMeshSystem()->GetVertexBuffer(),
            .mTexCoordBufferUniform = mScene->GetMeshSystem()->GetTexCoordBuffer(),
            .mNormalBufferUniform = mScene->GetMeshSystem()->GetNormalBuffer(),
            .mTangentBufferUniform = mScene->GetMeshSystem()->GetTangentBuffer(),
            .mMaterialsTextures = mSceneTextures,
        };
        mReflectionsRenderer->UpdatePersistentUniforms(parameters);
    }

    // Shadow resources
    {
        ShadowRenderer::PersistentParameters parameters{
            .scenePersistentDataBuffer = mScenePersistentDataBuffer,
            .materialSampler = mMaterialSampler,
            .materialUniform = mMaterialsUniform,
            .sceneTextures = mSceneTextures,
        };
        mShadowRenderer->UpdatePersistentUniforms(parameters);
    }

    // SSAO processing
    {
        SSAORenderer::PersistentParameters parameters{
            mFrameBufferSampler = mFrameBufferSampler,
            mDepthBuffer = mDepthBuffer,
        };
        mSSAORenderer->UpdatePersistentUniforms(parameters);
    }

    // Visibility buffer geometry + shading
    for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
        bool isTransparentPass = alphaMode == Material::AlphaMode::Blended;
        bool isAlphaMaskedPass = alphaMode == Material::AlphaMode::Masked;
        ScopedRefPtr<GraphicsPipeline>& pipeline = mGeometryPassPipelines[alphaMode];

        uint32_t bindingIndex = 0;
        pipeline->Bind(bindingIndex++, mScenePersistentDataBuffer);
        pipeline->Bind(bindingIndex++, mMaterialSampler);
        pipeline->Bind(bindingIndex++, mMaterialsUniform);
        if (isTransparentPass) {
            pipeline->Bind(bindingIndex++, mShadowRenderer->GetShadowMap());
        }
        pipeline->Bind(bindingIndex++, mSceneTextures);
    }

    mShadePassPipeline->Bind(0, mScenePersistentDataBuffer);
    mShadePassPipeline->Bind(1, mMaterialSampler);
    mShadePassPipeline->Bind(2, mFrameBufferSampler);
    mShadePassPipeline->Bind(3, mIrradianceSampler);
    mShadePassPipeline->Bind(4, mMaterialsUniform);
    mShadePassPipeline->Bind(5, mShadowRenderer->GetShadowMap());
    mShadePassPipeline->Bind(6, mVisibilityBuffer);
    mShadePassPipeline->Bind(7, mSSAORenderer->GetSSAOBuffer());
    mShadePassPipeline->Bind(8, mReflectionsRenderer->GetReflectionsTexture());
    mShadePassPipeline->Bind(9, mScene->GetMeshSystem()->GetIndexBuffer());
    mShadePassPipeline->Bind(10, mScene->GetMeshSystem()->GetVertexBuffer());
    mShadePassPipeline->Bind(11, mScene->GetMeshSystem()->GetTexCoordBuffer());
    mShadePassPipeline->Bind(12, mScene->GetMeshSystem()->GetNormalBuffer());
    mShadePassPipeline->Bind(13, mScene->GetMeshSystem()->GetTangentBuffer());
    mShadePassPipeline->Bind(14, mSceneTextures);

    {
        PostProcessingRenderer::PersistentParameters parameters{
            .mFrameBufferSampler = mFrameBufferSampler,
            .mScreenTexture = mHDRTarget,
        };
        mPostProcessingRenderer->UpdatePersistentUniforms(parameters);
    }
}

void Renderer::UpdateUniforms(Camera* camera, uint32_t frameIndex) {
    // Update culling systems
    for (auto& visEntry : mVisibilityManagers) {
        const Material::AlphaMode alphaMode = visEntry.first;
        VisibilityManager::CullData cullingData{
            .ortho = 0,
            .viewDirectionOrCameraPos = camera->GetPosition(),
            .frustumPlanes = camera->GetViewFrustum().GetPlanes(),
            .globalDrawOffset = static_cast<uint32_t>(mScene->GetDrawCallOffset(alphaMode)),
            .maxDrawCount = static_cast<uint32_t>(mScene->GetDrawCallCount(alphaMode))};

        visEntry.second->UpdateUniforms(cullingData, frameIndex, mPerMeshBuffers);
    }

    for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
        bool isTransparentPass = alphaMode == Material::AlphaMode::Blended;
        ScopedRefPtr<GraphicsPipeline>& pipeline = mGeometryPassPipelines[alphaMode];

        const uint32_t drawCallCount = static_cast<uint32_t>(mScene->GetDrawCallCount(alphaMode));
        if (drawCallCount <= 0) {
            continue;
        }

        uint32_t bindingIndex = 0;
        pipeline->Bind(frameIndex, bindingIndex++, mCameraUniform[frameIndex]);
        if (isTransparentPass) {
            pipeline->Bind(
                frameIndex,
                bindingIndex++,
                mShadowRenderer->GetShadowUniform()[frameIndex]);
        }
        pipeline->Bind(frameIndex, bindingIndex++, mPerMeshBuffers[frameIndex]);
        pipeline->Bind(
            frameIndex,
            bindingIndex++,
            mVisibilityManagers[alphaMode]->GetAdditionalDrawDataBuffer(frameIndex));
    }

    mShadePassPipeline->Bind(frameIndex, 0, mCameraUniform[frameIndex]);
    mShadePassPipeline->Bind(frameIndex, 1, mShadowRenderer->GetShadowUniform()[frameIndex]);
    mShadePassPipeline->Bind(frameIndex, 2, mPerMeshBuffers[frameIndex]);
    mShadePassPipeline->Bind(frameIndex, 3, mDDGIRenderer->GetProbeData()[frameIndex]);
    mShadePassPipeline->Bind(frameIndex, 4, mDDGIRenderer->GetIrradianceBuffer());
    mShadePassPipeline->Bind(frameIndex, 5, mDDGIRenderer->GetMomentsBuffer());

    // Update content of per-draw parameters
    {
        const Scene::PackedDrawData& drawData = mScene->GetPackedDrawData();
        {
            ScopedRefPtr<VulkanBuffer> currentBuffer = mPerMeshBuffers[frameIndex];
            uint8_t* buffer = currentBuffer->MapBuffer();
            std::copy_n(
                reinterpret_cast<const uint8_t*>(drawData.perMeshData.data()),
                drawData.perMeshData.size() * sizeof(Scene::MeshData),
                buffer);
            currentBuffer->UnmapBuffer();
        }
    }

    // Update camera uniform
    {
        CameraData cameraMatrices{
            .projection = camera->GetProjectionTransform(),
            .viewProjection = camera->GetProjectionTransform() * camera->GetViewTransform(),
            .invViewProjection =
                glm::inverse(camera->GetProjectionTransform() * camera->GetViewTransform()),
            .invProjection = glm::inverse(camera->GetProjectionTransform()),
            .invView = glm::inverse(camera->GetViewTransform()),
            .cameraPos = glm::vec4(camera->GetPosition(), 0.0f),
            ._near = camera->GetNear(),
            ._far = camera->GetFar(),
            .glossyDepthBias = mSettingsManager->GetGlossyDepthBias(),
            .glossyHitDepthBias = mSettingsManager->GetGlossyHitDepthBias()};
        mCameraUniform[frameIndex]->Write(cameraMatrices);
    }

    mShadowRenderer->UpdateUniforms(frameIndex, camera, {.meshDataBuffer = mPerMeshBuffers});
    mSSAORenderer->UpdateUniforms({.mCameraUniform = mCameraUniform}, frameIndex);

    // DDGI
    {
        mDDGIRenderer->UpdateUniforms(
            {.mCameraUniform = mCameraUniform[frameIndex],
             .mShadowCameraUniform = mShadowRenderer->GetShadowUniform()[frameIndex],
             .mPerMeshParameters = mPerMeshBuffers[frameIndex]},
            frameIndex);
    }

    // Reflections
    {
        mReflectionsRenderer->UpdateUniforms(
            {.mCameraUniform = mCameraUniform[frameIndex],
             .mShadowCameraUniform = mShadowRenderer->GetShadowUniform()[frameIndex],
             .mPerMeshParameters = mPerMeshBuffers[frameIndex]},
            frameIndex);
    }

    {
        mPostProcessingRenderer->UpdateUniforms(frameIndex);
    }
}

void Renderer::Render(Camera* camera) {
    if (mShadowRenderer->GetShadowMap()->GetWidth() != mSettingsManager->GetShadowMapResolution() ||
        mDDGIRenderer->GetIrradianceBuffer()->GetWidth() !=
            mSettingsManager->GetProbeResolution() ||
        mDDGIRenderer->GetProbeRayCount() != mSettingsManager->GetProbeRayCount()) {
        mCommandRing->WaitPreviousFrame();
        RemovePipelines();
        RemoveRenderTargets();

        AddRenderTargets();
        AddPipelines();
        mHasBoundResources = false;
    }

    // TODO: Where?
    {
        mScene->GetLight().SetDirection(glm::normalize(mSettingsManager->GetLightDir()));
        mScene->GetLight().SetRadiance(mSettingsManager->GetLightRadiance());
    }

    mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mContext->GetMaxInFlightFrameCount();
    CommandRing::CommandResources command = mCommandRing->Cycle();

    mContext->GetSwapchain()->AcquireNextImage(mCurrentFrameIndex);

    mScene->Update();

    if (!mHasResouces) {
        AddResources();
    }
    if (!mHasBoundResources) {
        UpdatePersistentUniforms();
    }
    UpdateUniforms(camera, mCurrentFrameIndex);

    {
        VKRT_ASSERT_VK(command.buffer.begin(vk::CommandBufferBeginInfo{}));

        const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();

        mContext->BeginMarker(command.buffer, "Basepass culling");
        for (auto& visEntry : mVisibilityManagers) {
            visEntry.second->Dispatch(command.buffer, mCurrentFrameIndex);
        }
        mContext->EndMarker(command.buffer);

        mShadowRenderer->Render(command.buffer, mCurrentFrameIndex);

        // Base pass
        {
            mContext->BeginMarker(command.buffer, "Base pass");

            {
                std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    {{mVisibilityBuffer,
                      vk::ImageLayout::eShaderReadOnlyOptimal,
                      vk::ImageLayout::eColorAttachmentOptimal}});

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::DependencyFlags{},
                    {},
                    {},
                    imageBarriers);
            }

            const std::vector<vk::ClearValue> clearValues{
                vk::ClearColorValue(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF),
                vk::ClearDepthStencilValue(1.0f, 0),
            };
            const vk::RenderPassBeginInfo renderPassBeginInfo =
                vk::RenderPassBeginInfo()
                    .setRenderPass(mGeometryPass->GetRenderPassHandle())
                    .setFramebuffer(mGeometryPass->GetFramebufferHandle())
                    .setRenderArea({vk::Offset2D{0, 0}, imageSize})
                    .setClearValues(clearValues);
            command.buffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

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

            for (const Material::AlphaMode alphaMode : Material::AlphaModes) {
                uint32_t drawCallCount = mScene->GetDrawCallCount(alphaMode);
                if (alphaMode == Material::AlphaMode::Blended || drawCallCount == 0) {
                    continue;
                }
                mContext->BeginMarker(command.buffer, AlphaModeToStr(alphaMode));
                command.buffer.bindPipeline(
                    vk::PipelineBindPoint::eGraphics,
                    mGeometryPassPipelines[alphaMode]->GetPipelineHandle());

                std::vector<vk::DescriptorSet> descriptorSets =
                    mGeometryPassPipelines[alphaMode]->GetDescriptorSets(mCurrentFrameIndex);

                command.buffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    mGeometryPassPipelines[alphaMode]->GetPipelineLayout(),
                    0,
                    descriptorSets,
                    nullptr);

                {
                    mScene->GetMeshSystem()->BindBuffers(
                        command.buffer,
                        alphaMode == Material::AlphaMode::Opaque
                            ? VertexAttributeFlag::Position
                            : VertexAttributeFlag(
                                  VertexAttributeFlag::Position | VertexAttributeFlag::TexCoord));

                    uint32_t maxDrawIndirectCount =
                        mContext->GetDevice()->GetDeviceProperties().limits.maxDrawIndirectCount;
                    VKRT_ASSERT(maxDrawIndirectCount >= drawCallCount);

                    command.buffer.drawIndexedIndirectCount(
                        mVisibilityManagers[alphaMode]
                            ->GetIndirectDrawBuffer(mCurrentFrameIndex)
                            ->GetBufferHandle(),
                        0,
                        mVisibilityManagers[alphaMode]
                            ->GetIndirectDrawCountBuffer(mCurrentFrameIndex)
                            ->GetBufferHandle(),
                        0,
                        drawCallCount,
                        sizeof(VkDrawIndexedIndirectCommand));
                }
                mContext->EndMarker(command.buffer);
            }
            command.buffer.endRenderPass();
            mContext->EndMarker(command.buffer);
        }

        mSSAORenderer->Render(command.buffer, mCurrentFrameIndex, mDepthBuffer);

        mDDGIRenderer->Render(command.buffer, mCurrentFrameIndex);

        mReflectionsRenderer->Render(command.buffer, mCurrentFrameIndex);

        // Shade pass
        {
            mContext->BeginMarker(command.buffer, "Shade pass");

            {
                std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    {{mVisibilityBuffer,
                      vk::ImageLayout::eColorAttachmentOptimal,
                      vk::ImageLayout::eShaderReadOnlyOptimal}});

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::DependencyFlags{},
                    {},
                    {},
                    imageBarriers);
            }

            const std::vector<vk::ClearValue> clearValues{
                vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f)};
            const vk::RenderPassBeginInfo renderPassBeginInfo =
                vk::RenderPassBeginInfo()
                    .setRenderPass(mShadePass->GetRenderPassHandle())
                    .setFramebuffer(mShadePass->GetFramebufferHandle())
                    .setRenderArea({vk::Offset2D{0, 0}, imageSize})
                    .setClearValues(clearValues);
            command.buffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

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
                mShadePassPipeline->GetPipelineHandle());

            std::vector<vk::DescriptorSet> descriptorSets =
                mShadePassPipeline->GetDescriptorSets(mCurrentFrameIndex);

            command.buffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                mShadePassPipeline->GetPipelineLayout(),
                0,
                descriptorSets,
                nullptr);

            command.buffer.draw(3, 1, 0, 0);

            command.buffer.endRenderPass();
            mContext->EndMarker(command.buffer);
        }

        // Render transparencies
        {
            mContext->BeginMarker(command.buffer, "Transparent pass");

            {
                std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    {{mDepthBuffer,
                      vk::ImageLayout::eShaderReadOnlyOptimal,
                      vk::ImageLayout::eDepthAttachmentOptimal}});

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    vk::DependencyFlags{},
                    {},
                    {},
                    imageBarriers);
            }

            const std::vector<vk::ClearValue> clearValues{
                vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
                vk::ClearDepthStencilValue(1.0f, 0),
            };
            const vk::RenderPassBeginInfo renderPassBeginInfo =
                vk::RenderPassBeginInfo()
                    .setRenderPass(mTransparentPass->GetRenderPassHandle())
                    .setFramebuffer(mTransparentPass->GetFramebufferHandle())
                    .setRenderArea({vk::Offset2D{0, 0}, imageSize})
                    .setClearValues(clearValues);
            command.buffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

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

            const Material::AlphaMode alphaMode = Material::AlphaMode::Blended;
            {
                uint32_t drawCallCount = mScene->GetDrawCallCount(alphaMode);
                if (drawCallCount > 0) {
                    command.buffer.bindPipeline(
                        vk::PipelineBindPoint::eGraphics,
                        mGeometryPassPipelines[alphaMode]->GetPipelineHandle());

                    std::vector<vk::DescriptorSet> descriptorSets =
                        mGeometryPassPipelines[alphaMode]->GetDescriptorSets(mCurrentFrameIndex);

                    command.buffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics,
                        mGeometryPassPipelines[alphaMode]->GetPipelineLayout(),
                        0,
                        descriptorSets,
                        nullptr);

                    {
                        mScene->GetMeshSystem()->BindBuffers(
                            command.buffer,
                            VertexAttributeFlag::All);

                        uint32_t maxDrawIndirectCount = mContext->GetDevice()
                                                            ->GetDeviceProperties()
                                                            .limits.maxDrawIndirectCount;
                        VKRT_ASSERT(maxDrawIndirectCount >= drawCallCount);

                        command.buffer.drawIndexedIndirectCount(
                            mVisibilityManagers[alphaMode]
                                ->GetIndirectDrawBuffer(mCurrentFrameIndex)
                                ->GetBufferHandle(),
                            0,
                            mVisibilityManagers[alphaMode]
                                ->GetIndirectDrawCountBuffer(mCurrentFrameIndex)
                                ->GetBufferHandle(),
                            0,
                            drawCallCount,
                            sizeof(VkDrawIndexedIndirectCommand));
                    }
                }
            }
            command.buffer.endRenderPass();
            mContext->EndMarker(command.buffer);
        }

        if (mSettingsManager->GetRenderProbes()) {
            mDDGIRenderer->RenderProbes(command.buffer, mCurrentFrameIndex);
        }

        {
            std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::PipelineStageFlagBits::eFragmentShader,
                {{mHDRTarget,
                  vk::ImageLayout::eColorAttachmentOptimal,
                  vk::ImageLayout::eShaderReadOnlyOptimal}});

            command.buffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::DependencyFlags{},
                {},
                {},
                imageBarriers);
        }
        mPostProcessingRenderer->Render(command.buffer, mCurrentFrameIndex);

        {
            mContext->BeginMarker(command.buffer, "Render UI");
            mUIRenderer->Render(command.buffer);
            mContext->EndMarker(command.buffer);
        }

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

    mContext->GetDevice()->GetLogicalDevice().destroySampler(mMaterialSampler);
    mContext->GetDevice()->GetLogicalDevice().destroySampler(mFrameBufferSampler);
    mContext->GetDevice()->GetLogicalDevice().destroySampler(mIrradianceSampler);
}

}  // namespace VKRT