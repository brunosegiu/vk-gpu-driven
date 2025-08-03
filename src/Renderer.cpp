#include "Renderer.h"

#include <random>

#include "DebugUtils.h"
#include "Texture.h"

namespace VKRT {

struct CameraData {
    glm::mat4 viewProjection;
    glm::vec4 cameraForwardDir;
};

struct LightData {
    glm::vec3 radiance;
    glm::vec3 direction;
    glm::mat4 viewProjection;
};

struct CullData {
    std::array<glm::vec4, 6> frustumPlanes;
    uint32_t globalDrawOffset;
    uint32_t maxDrawCount;
};

Renderer::Renderer(ScopedRefPtr<Context> context, ScopedRefPtr<Scene> scene)
    : mContext(context),
      mScene(scene),
      mCurrentFrameIndex(0),
      mMaterialsBuffer(nullptr),
      mPerDrawBuffers() {
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
    mBasePass = new RenderPass(
        context,
        {{.renderTarget = mRenderTarget,
          .loadOp = vk::AttachmentLoadOp::eClear,
          .initialLayout = vk::ImageLayout::eUndefined,
          .storeOp = vk::AttachmentStoreOp::eStore,
          .finalLayout = vk::ImageLayout::eColorAttachmentOptimal},
         {.renderTarget = mDepthRenderTarget,
          .loadOp = vk::AttachmentLoadOp::eClear,
          .initialLayout = vk::ImageLayout::eUndefined,
          .storeOp = vk::AttachmentStoreOp::eStore,
          .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal}});
    mTransparentPass = new RenderPass(
        context,
        {
            {.renderTarget = mRenderTarget,
             .loadOp = vk::AttachmentLoadOp::eLoad,
             .initialLayout = vk::ImageLayout::eColorAttachmentOptimal,
             .storeOp = vk::AttachmentStoreOp::eStore,
             .finalLayout = vk::ImageLayout::ePresentSrcKHR},
            {.renderTarget = mDepthRenderTarget,
             .loadOp = vk::AttachmentLoadOp::eLoad,
             .initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
             .storeOp = vk::AttachmentStoreOp::eStore,
             .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal},
        });

    {
        mShadowMap = new Texture(
            mContext,
            4096,
            4096,
            vk::Format::eD16Unorm,
            vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);
        mDepthOnlyPassRenderTarget = new RenderTarget(mContext, mShadowMap);
        mDepthOnlyPass = new RenderPass(
            context,
            {.renderTarget = mDepthOnlyPassRenderTarget,
             .loadOp = vk::AttachmentLoadOp::eClear,
             .initialLayout = vk::ImageLayout::eUndefined,
             .storeOp = vk::AttachmentStoreOp::eStore,
             .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal});
    }

    // Global resources
    {
        mPerDrawParameters = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eStorageBuffer,
            ShaderParameter::UpdateFrequency::PerFrame,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment |
                vk::ShaderStageFlagBits::eCompute);
    }

    // Culling resources
    {
        // Shadow culling resources
        auto createCullingResources = [&](Renderer::CullingPipelineResources& resources) {
            resources.cullingParameters = new ShaderParameterCollection(mContext);
            resources.cullingDataUniform = new ShaderParameterBuffer(
                mContext,
                vk::DescriptorType::eUniformBuffer,
                ShaderParameter::UpdateFrequency::PerFrame,
                vk::ShaderStageFlagBits::eCompute,
                sizeof(CullData),
                vk::BufferUsageFlagBits::eUniformBuffer,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                    VMA_ALLOCATION_CREATE_MAPPED_BIT);

            resources.indirectDrawBufferParameter = new ShaderParameterBuffer(
                mContext,
                vk::DescriptorType::eStorageBuffer,
                ShaderParameter::UpdateFrequency::PerFrame,
                vk::ShaderStageFlagBits::eCompute);

            resources.cullingParameters->AddParameter(resources.cullingDataUniform);
            resources.cullingParameters->AddParameter(mPerDrawParameters);
            resources.cullingParameters->AddParameter(resources.indirectDrawBufferParameter);

            resources.cullingPipeline = new ComputePipeline(
                context,
                resources.cullingParameters,
                {vk::ShaderStageFlagBits::eCompute, Resource::Id::CullingShader});

            ScopedRefPtr<ComputePipeline>;

            resources.compactionParameters = new ShaderParameterCollection(mContext);

            resources.compactIndirectDrawBufferParameter = new ShaderParameterBuffer(
                mContext,
                vk::DescriptorType::eStorageBuffer,
                ShaderParameter::UpdateFrequency::PerFrame,
                vk::ShaderStageFlagBits::eCompute);

            resources.additionalDrawDataBufferParameter = new ShaderParameterBuffer(
                mContext,
                vk::DescriptorType::eStorageBuffer,
                ShaderParameter::UpdateFrequency::PerFrame,
                vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex |
                    vk::ShaderStageFlagBits::eFragment);

            resources.drawCallCountBufferParameter = new ShaderParameterBuffer(
                mContext,
                vk::DescriptorType::eStorageBuffer,
                ShaderParameter::UpdateFrequency::PerFrame,
                vk::ShaderStageFlagBits::eCompute);

            resources.compactionParameters->AddParameter(resources.cullingDataUniform);
            resources.compactionParameters->AddParameter(resources.indirectDrawBufferParameter);
            resources.compactionParameters->AddParameter(
                resources.compactIndirectDrawBufferParameter);
            resources.compactionParameters->AddParameter(
                resources.additionalDrawDataBufferParameter);
            resources.compactionParameters->AddParameter(resources.drawCallCountBufferParameter);

            resources.compactionPipeline = new ComputePipeline(
                context,
                resources.compactionParameters,
                {vk::ShaderStageFlagBits::eCompute, Resource::Id::CompactionShader});
        };

        createCullingResources(mShadowPassCulling[Material::AlphaMode::Opaque]);
        createCullingResources(mShadowPassCulling[Material::AlphaMode::Masked]);
        createCullingResources(mBasePassCulling[Material::AlphaMode::Opaque]);
        createCullingResources(mBasePassCulling[Material::AlphaMode::Masked]);
        createCullingResources(mBasePassCulling[Material::AlphaMode::Blended]);
    }

    // Shadow pass parameters
    {
        mShadowCameraUniform = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eUniformBuffer,
            ShaderParameter::UpdateFrequency::PerFrame,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex,
            sizeof(LightData),
            vk::BufferUsageFlagBits::eUniformBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);

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

        mMaterialsUniform = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eStorageBuffer,
            ShaderParameter::UpdateFrequency::Once,
            vk::ShaderStageFlagBits::eFragment);

        mMaterialsTextures = new ShaderParameterImage(
            mContext,
            vk::DescriptorType::eSampledImage,
            vk::ShaderStageFlagBits::eFragment,
            4096,
            true);

        std::unordered_map<
            Material::AlphaMode,
            std::unordered_map<vk::ShaderStageFlagBits, Resource::Id>>
            stages{
                {Material::AlphaMode::Opaque,
                 {
                     {vk::ShaderStageFlagBits::eVertex, Resource::Id::DepthOnlyOpaqueVertexShader},
                     {vk::ShaderStageFlagBits::eFragment,
                      Resource::Id::DepthOnlyOpaqueFragmentShader},
                 }},
                {Material::AlphaMode::Masked,
                 {
                     {vk::ShaderStageFlagBits::eVertex,
                      Resource::Id::DepthOnlyAlphaMaskedVertexShader},
                     {vk::ShaderStageFlagBits::eFragment,
                      Resource::Id::DepthOnlyAlphaMaskedFragmentShader},
                 }}};

        for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
            if (alphaMode !=
                Material::AlphaMode::Blended) {  // Blended materials aren't shadow casters
                ScopedRefPtr<ShaderParameterCollection>& parameters =
                    mShadowPassPipeline[alphaMode].parameters;
                ScopedRefPtr<GraphicsPipeline>& pipeline = mShadowPassPipeline[alphaMode].pipeline;

                parameters = new ShaderParameterCollection(mContext);
                parameters->AddParameter(mShadowCameraUniform);
                parameters->AddParameter(mPerDrawParameters);
                parameters->AddParameter(
                    mShadowPassCulling[alphaMode].additionalDrawDataBufferParameter);
                parameters->AddParameter(mMaterialSampler);
                parameters->AddParameter(mMaterialsUniform);
                parameters->AddParameter(mMaterialsTextures);

                const std::vector<GeometryLayout> geometryLayout = MeshSystem::GetGeometryLayout();

                pipeline = new GraphicsPipeline(
                    context,
                    parameters,
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

    // Main pass resources
    {
        mCameraUniform = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eUniformBuffer,
            ShaderParameter::UpdateFrequency::PerFrame,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex,
            sizeof(CameraData),
            vk::BufferUsageFlagBits::eUniformBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);

        mShadowMapUniform = new ShaderParameterImage(
            mContext,
            vk::DescriptorType::eSampledImage,
            vk::ShaderStageFlagBits::eFragment);

        std::unordered_map<
            Material::AlphaMode,
            std::unordered_map<vk::ShaderStageFlagBits, Resource::Id>>
            stages{
                {Material::AlphaMode::Opaque,
                 {
                     {vk::ShaderStageFlagBits::eVertex, Resource::Id::UberShaderOpaqueVertexShader},
                     {vk::ShaderStageFlagBits::eFragment,
                      Resource::Id::UberShaderOpaqueFragmentShader},
                 }},
                {Material::AlphaMode::Masked,
                 {
                     {vk::ShaderStageFlagBits::eVertex,
                      Resource::Id::UberShaderAlphaMaskedVertexShader},
                     {vk::ShaderStageFlagBits::eFragment,
                      Resource::Id::UberShaderAlphaMaskedFragmentShader},
                 }},
                {Material::AlphaMode::Blended,
                 {
                     {vk::ShaderStageFlagBits::eVertex, Resource::Id::UberShaderOpaqueVertexShader},
                     {vk::ShaderStageFlagBits::eFragment,
                      Resource::Id::UberShaderOpaqueFragmentShader},
                 }}};

        for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
            bool isTransparentPass = alphaMode == Material::AlphaMode::Blended;
            ScopedRefPtr<ShaderParameterCollection>& parameters =
                mBasePassPipeline[alphaMode].parameters;
            ScopedRefPtr<GraphicsPipeline>& pipeline = mBasePassPipeline[alphaMode].pipeline;

            parameters = new ShaderParameterCollection(mContext);
            parameters->AddParameter(mCameraUniform);
            parameters->AddParameter(mShadowCameraUniform);
            parameters->AddParameter(mPerDrawParameters);
            parameters->AddParameter(mBasePassCulling[alphaMode].additionalDrawDataBufferParameter);
            parameters->AddParameter(mMaterialSampler);
            parameters->AddParameter(mMaterialsUniform);
            parameters->AddParameter(mShadowMapUniform);
            parameters->AddParameter(mMaterialsTextures);

            const std::vector<GeometryLayout> geometryLayout = MeshSystem::GetGeometryLayout();

            pipeline = new GraphicsPipeline(
                context,
                parameters,
                stages[alphaMode],
                isTransparentPass ? mTransparentPass : mBasePass,
                geometryLayout,
                {.enableBlending = isTransparentPass});
        }
    }
}

void Renderer::UpdateUniforms(Camera* camera, uint32_t imageIndex) {
    // Create and update per-draw parameters
    if (mPerDrawBuffers.empty()) {
        uint32_t descriptorCount = 0;
        const std::vector<Scene::DrawData>& drawData = mScene->GetPackedDrawData();
        const size_t perDrawBufferSize = drawData.size() * sizeof(Scene::DrawData);

        const uint32_t bufferCount = mContext->GetMaxInFlightFrameCount();
        VKRT_ASSERT(perDrawBufferSize > 0);
        for (uint32_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex) {
            ScopedRefPtr<VulkanBuffer> perDrawBuffer = mContext->GetDevice()->CreateBuffer(
                perDrawBufferSize,
                vk::BufferUsageFlagBits::eStorageBuffer,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                    VMA_ALLOCATION_CREATE_MAPPED_BIT);
            mPerDrawBuffers.push_back(perDrawBuffer);
        }

        mPerDrawParameters->BindBuffers(mPerDrawBuffers);
    }

    // Update content of per-draw parameters
    {
        ScopedRefPtr<VulkanBuffer> currentBuffer = mPerDrawBuffers[imageIndex];
        const std::vector<Scene::DrawData>& drawData = mScene->GetPackedDrawData();
        uint8_t* buffer = currentBuffer->MapBuffer();
        std::copy_n(
            reinterpret_cast<const uint8_t*>(drawData.data()),
            drawData.size() * sizeof(Scene::DrawData),
            buffer);
        currentBuffer->UnmapBuffer();
    }

    // Update culling parameters
    auto updateCullingResources = [&](Renderer::CullingPipelineResources& resources,
                                      Material::AlphaMode alphaMode,
                                      CullData cullData) {
        const uint32_t drawCallCount = mScene->GetDrawCallCount(alphaMode);
        if (drawCallCount == 0) {
            return;
        }
        resources.cullingDataUniform->Write(
            imageIndex,
            reinterpret_cast<uint8_t*>(&cullData),
            sizeof(CullData));

        const uint32_t bufferCount = mContext->GetMaxInFlightFrameCount();

        if (resources.indirectDrawBuffers.empty()) {
            resources.indirectDrawBuffers = mContext->GetDevice()->CreateBuffers(
                bufferCount,
                drawCallCount * sizeof(vk::DrawIndexedIndirectCommand),
                vk::BufferUsageFlagBits::eStorageBuffer,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
            resources.indirectDrawBufferParameter->BindBuffers(resources.indirectDrawBuffers);
        }

        if (resources.compactIndirectDrawBuffers.empty()) {
            resources.compactIndirectDrawBuffers = mContext->GetDevice()->CreateBuffers(
                bufferCount,
                drawCallCount * sizeof(vk::DrawIndexedIndirectCommand),
                vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
            resources.compactIndirectDrawBufferParameter->BindBuffers(
                resources.compactIndirectDrawBuffers);
        }

        if (resources.additionalDrawDataBuffers.empty()) {
            resources.additionalDrawDataBuffers = mContext->GetDevice()->CreateBuffers(
                bufferCount,
                drawCallCount * sizeof(uint32_t),
                vk::BufferUsageFlagBits::eStorageBuffer,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
            resources.additionalDrawDataBufferParameter->BindBuffers(
                resources.additionalDrawDataBuffers);
        }

        if (resources.drawCallCountBuffer.empty()) {
            resources.drawCallCountBuffer = mContext->GetDevice()->CreateBuffers(
                bufferCount,
                sizeof(uint32_t),
                vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
                    vk::BufferUsageFlagBits::eTransferDst,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
            resources.drawCallCountBufferParameter->BindBuffers(resources.drawCallCountBuffer);
        }
    };

    for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
        if (alphaMode != Material::AlphaMode::Blended) {  // Blended materials aren't shadow casters
            CullData shadowCullingData{
                .frustumPlanes = ViewFrustum(mScene->GetLight().ComputeShadowMatrix()).GetPlanes(),
                .globalDrawOffset = static_cast<uint32_t>(mScene->GetDrawCallOffset(alphaMode)),
                .maxDrawCount = static_cast<uint32_t>(mScene->GetDrawCallCount(alphaMode))};
            updateCullingResources(mShadowPassCulling[alphaMode], alphaMode, shadowCullingData);
        }

        CullData mainCameraCullingData{
            .frustumPlanes = camera->GetViewFrustum().GetPlanes(),
            .globalDrawOffset = static_cast<uint32_t>(mScene->GetDrawCallOffset(alphaMode)),
            .maxDrawCount = static_cast<uint32_t>(mScene->GetDrawCallCount(alphaMode))};
        updateCullingResources(mBasePassCulling[alphaMode], alphaMode, mainCameraCullingData);
    }

    // Shadow camera parameters
    {
        DirectionalLight& light = mScene->GetLight();
        glm::mat4 shadowMatrix = light.ComputeShadowMatrix();
        LightData cameraMatrices{
            .radiance = light.GetRadiance(),
            .direction = light.GetDirection(),
            .viewProjection = shadowMatrix};
        mShadowCameraUniform->Write(
            imageIndex,
            reinterpret_cast<uint8_t*>(&cameraMatrices),
            sizeof(LightData));
    }

    // Update camera uniform
    {
        CameraData cameraMatrices{
            .viewProjection = camera->GetProjectionTransform() * camera->GetViewTransform(),
            .cameraForwardDir = glm::vec4(camera->GetForwardDir(), 0.0f)};
        mCameraUniform->Write(
            imageIndex,
            reinterpret_cast<uint8_t*>(&cameraMatrices),
            sizeof(CameraData));
    }

    // Update materials
    if (mMaterialsBuffer == nullptr) {
        uint32_t descriptorCount = 0;
        Scene::SceneMaterials sceneMaterials = mScene->GetMaterialProxies();
        size_t materialBufferSize = sceneMaterials.materials.size() * sizeof(Scene::MaterialProxy);
        VKRT_ASSERT(materialBufferSize > 0);
        mMaterialsBuffer = mContext->GetDevice()->CreateBuffer(
            materialBufferSize,
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);
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
        mMaterialsUniform->BindBuffer(mMaterialsBuffer);

        mShadowMapUniform->Bind(mShadowMap);
    }
}

void Renderer::BeginMarker(const vk::CommandBuffer& commandBuffer, const std::string& name) {
#ifdef VKRT_ENABLE_VALIDATION
    commandBuffer.beginDebugUtilsLabelEXT(
        vk::DebugUtilsLabelEXT().setPLabelName(name.c_str()),
        mContext->GetDevice()->GetDispatcher());
#endif
}

void Renderer::EndMarker(const vk::CommandBuffer& commandBuffer) {
#ifdef VKRT_ENABLE_VALIDATION
    commandBuffer.endDebugUtilsLabelEXT(mContext->GetDevice()->GetDispatcher());
#endif
}

std::string AlphaModeToStr(const Material::AlphaMode& alphaMode) {
    switch (alphaMode) {
        case Material::AlphaMode::Opaque:
            return "Opaque";
        case Material::AlphaMode::Masked:
            return "Masked";
        case Material::AlphaMode::Blended:
            return "Blended";
    }
    return "";
}

void Renderer::Render(Camera* camera) {
    mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mContext->GetMaxInFlightFrameCount();
    CommandRing::CommandResources command = mCommandRing->Cycle();

    mScene->Lock();
    mContext->GetSwapchain()->AcquireNextImage(mCurrentFrameIndex);

    mScene->Update();

    UpdateUniforms(camera, mCurrentFrameIndex);

    {
        VKRT_ASSERT_VK(command.buffer.begin(vk::CommandBufferBeginInfo{}));

        const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();

        const auto dispatchCulling = [&](CullingPipelineResources& resources,
                                         uint32_t maxDrawCallCount) {
            if (maxDrawCallCount == 0) {
                return;
            }
            ScopedRefPtr<VulkanBuffer> cullingIndirectBuffer =
                resources.indirectDrawBuffers[mCurrentFrameIndex];
            {
                vk::BufferMemoryBarrier bufferBarrier =
                    vk::BufferMemoryBarrier()
                        .setBuffer(cullingIndirectBuffer->GetBufferHandle())
                        .setSize(cullingIndirectBuffer->GetBufferSize())
                        .setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
                        .setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::DependencyFlags{},
                    {},
                    bufferBarrier,
                    {});
            }

            command.buffer.bindPipeline(
                vk::PipelineBindPoint::eCompute,
                resources.cullingPipeline->GetPipelineHandle());

            std::vector<vk::DescriptorSet> cullingDescriptorSets =
                resources.cullingParameters->GetDescriptorSets(mCurrentFrameIndex);

            command.buffer.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute,
                resources.cullingPipeline->GetPipelineLayout(),
                0,
                cullingDescriptorSets,
                nullptr);

            command.buffer.dispatch((maxDrawCallCount / 64) + 1, 1, 1);

            ScopedRefPtr<VulkanBuffer> compactIndirectDrawBuffer =
                resources.compactIndirectDrawBuffers[mCurrentFrameIndex];
            ScopedRefPtr<VulkanBuffer> additionalDrawDataBuffer =
                resources.additionalDrawDataBuffers[mCurrentFrameIndex];
            ScopedRefPtr<VulkanBuffer> drawCallCountBuffer =
                resources.drawCallCountBuffer[mCurrentFrameIndex];

            // Write 0 in drawCallCountBuffer
            {
                std::vector<vk::BufferMemoryBarrier> bufferBarriers{
                    drawCallCountBuffer->GetBufferBarrierInfo(
                        vk::PipelineStageFlagBits::eDrawIndirect,
                        vk::PipelineStageFlagBits::eTransfer),
                };

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eDrawIndirect,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::DependencyFlags{},
                    {},
                    bufferBarriers,
                    {});

                command.buffer
                    .fillBuffer(drawCallCountBuffer->GetBufferHandle(), 0, sizeof(uint32_t), 0);
            }

            {
                std::vector<vk::BufferMemoryBarrier> bufferBarriers{
                    drawCallCountBuffer->GetBufferBarrierInfo(
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eComputeShader)};

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::DependencyFlags{},
                    {},
                    bufferBarriers,
                    {});
            }

            {
                std::vector<vk::BufferMemoryBarrier> bufferBarriers{
                    compactIndirectDrawBuffer->GetBufferBarrierInfo(
                        vk::PipelineStageFlagBits::eDrawIndirect,
                        vk::PipelineStageFlagBits::eComputeShader),
                    additionalDrawDataBuffer->GetBufferBarrierInfo(
                        vk::PipelineStageFlagBits::eDrawIndirect,
                        vk::PipelineStageFlagBits::eComputeShader)};

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eDrawIndirect,
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::DependencyFlags{},
                    {},
                    bufferBarriers,
                    {});
            }

            command.buffer.bindPipeline(
                vk::PipelineBindPoint::eCompute,
                resources.compactionPipeline->GetPipelineHandle());

            std::vector<vk::DescriptorSet> compactionDescriptorSets =
                resources.compactionParameters->GetDescriptorSets(mCurrentFrameIndex);

            command.buffer.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute,
                resources.compactionPipeline->GetPipelineLayout(),
                0,
                compactionDescriptorSets,
                nullptr);

            command.buffer.dispatch((maxDrawCallCount / 64) + 1, 1, 1);

            // TODO: Wait until before actual draws are dispatched, this is too early
            {
                std::vector<vk::BufferMemoryBarrier> bufferBarriers{
                    compactIndirectDrawBuffer->GetBufferBarrierInfo(
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::PipelineStageFlagBits::eAllGraphics),
                    additionalDrawDataBuffer->GetBufferBarrierInfo(
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::PipelineStageFlagBits::eAllGraphics),
                    drawCallCountBuffer->GetBufferBarrierInfo(
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::PipelineStageFlagBits::eAllGraphics)};

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eAllGraphics,
                    vk::DependencyFlags{},
                    {},
                    bufferBarriers,
                    {});
            }
        };

        BeginMarker(command.buffer, "Shadow culling");
        for (const Material::AlphaMode alphaMode : Material::AlphaModes) {
            if (alphaMode !=
                Material::AlphaMode::Blended) {  // Blended materials aren't shadow casters
                BeginMarker(command.buffer, AlphaModeToStr(alphaMode));
                dispatchCulling(mShadowPassCulling[alphaMode], mScene->GetDrawCallCount(alphaMode));
                EndMarker(command.buffer);
            }
        }
        EndMarker(command.buffer);

        BeginMarker(command.buffer, "Basepass culling");
        for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
            BeginMarker(command.buffer, AlphaModeToStr(alphaMode));
            dispatchCulling(mBasePassCulling[alphaMode], mScene->GetDrawCallCount(alphaMode));
            EndMarker(command.buffer);
        }
        EndMarker(command.buffer);

        // Shadow pass
        {
            BeginMarker(command.buffer, "Shadowmap Rendering");
            {
                // Transition shadow map to depth attachment
                vk::ImageMemoryBarrier imageBarrier =
                    vk::ImageMemoryBarrier()
                        .setImage(mShadowMap->GetImage())
                        .setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                        .setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal)
                        .setSrcAccessMask(vk::AccessFlagBits::eUniformRead)
                        .setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
                        .setSubresourceRange(vk::ImageSubresourceRange{}
                                                 .setAspectMask(vk::ImageAspectFlagBits::eDepth)
                                                 .setLevelCount(1)
                                                 .setLayerCount(1));

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eVertexShader,
                    vk::DependencyFlags{},
                    {},
                    {},
                    imageBarrier);
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
            command.buffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

            {
                const vk::Viewport viewport{
                    0.0f,
                    0.0f,
                    static_cast<float>(mShadowMap->GetWidth()),
                    static_cast<float>(mShadowMap->GetHeight()),
                    0.0f,
                    1.0f};
                command.buffer.setViewport(0, viewport);

                const vk::Rect2D scissor =
                    vk::Rect2D().setOffset(0).setExtent(mShadowMap->GetExtent());
                command.buffer.setScissor(0, scissor);
            }

            for (const Material::AlphaMode alphaMode : Material::AlphaModes) {
                // Skip transparent objects while rendering shadows
                uint32_t drawCallCount = mScene->GetDrawCallCount(alphaMode);
                if (alphaMode == Material::AlphaMode::Blended || drawCallCount == 0) {
                    continue;
                }
                BeginMarker(command.buffer, AlphaModeToStr(alphaMode));

                command.buffer.bindPipeline(
                    vk::PipelineBindPoint::eGraphics,
                    mShadowPassPipeline[alphaMode].pipeline->GetPipelineHandle());

                std::vector<vk::DescriptorSet> descriptorSets =
                    mShadowPassPipeline[alphaMode].parameters->GetDescriptorSets(
                        mCurrentFrameIndex);

                command.buffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    mShadowPassPipeline[alphaMode].pipeline->GetPipelineLayout(),
                    0,
                    descriptorSets,
                    nullptr);

                {
                    mScene->GetMeshSystem()->BindBuffers(command.buffer);

                    uint32_t maxDrawIndirectCount =
                        mContext->GetDevice()->GetDeviceProperties().limits.maxDrawIndirectCount;
                    VKRT_ASSERT(maxDrawIndirectCount >= drawCallCount);

                    command.buffer.drawIndexedIndirectCount(
                        mShadowPassCulling[alphaMode]
                            .compactIndirectDrawBuffers[mCurrentFrameIndex]
                            ->GetBufferHandle(),
                        0,
                        mShadowPassCulling[alphaMode]
                            .drawCallCountBuffer[mCurrentFrameIndex]
                            ->GetBufferHandle(),
                        0,
                        drawCallCount,
                        sizeof(VkDrawIndexedIndirectCommand));
                }
                EndMarker(command.buffer);
            }

            command.buffer.endRenderPass();
            EndMarker(command.buffer);
        }

        // Base pass
        {
            BeginMarker(command.buffer, "Base pass");
            {
                vk::ImageMemoryBarrier imageBarrier =
                    vk::ImageMemoryBarrier()
                        .setImage(mShadowMap->GetImage())
                        .setOldLayout(vk::ImageLayout::eDepthAttachmentOptimal)
                        .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                        .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                        .setDstAccessMask(vk::AccessFlagBits::eUniformRead)
                        .setSubresourceRange(vk::ImageSubresourceRange{}
                                                 .setAspectMask(vk::ImageAspectFlagBits::eDepth)
                                                 .setLevelCount(1)
                                                 .setLayerCount(1));

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::DependencyFlags{},
                    {},
                    {},
                    imageBarrier);
            }

            const std::vector<vk::ClearValue> clearValues{
                vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
                vk::ClearDepthStencilValue(1.0f, 0),
            };
            const vk::RenderPassBeginInfo renderPassBeginInfo =
                vk::RenderPassBeginInfo()
                    .setRenderPass(mBasePass->GetRenderPassHandle())
                    .setFramebuffer(mBasePass->GetFramebufferHandle(
                        mContext->GetSwapchain()->GetCurrentIndex()))
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
                BeginMarker(command.buffer, AlphaModeToStr(alphaMode));
                command.buffer.bindPipeline(
                    vk::PipelineBindPoint::eGraphics,
                    mBasePassPipeline[alphaMode].pipeline->GetPipelineHandle());

                std::vector<vk::DescriptorSet> descriptorSets =
                    mBasePassPipeline[alphaMode].parameters->GetDescriptorSets(mCurrentFrameIndex);

                command.buffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    mBasePassPipeline[alphaMode].pipeline->GetPipelineLayout(),
                    0,
                    descriptorSets,
                    nullptr);

                {
                    mScene->GetMeshSystem()->BindBuffers(command.buffer);

                    uint32_t maxDrawIndirectCount =
                        mContext->GetDevice()->GetDeviceProperties().limits.maxDrawIndirectCount;
                    VKRT_ASSERT(maxDrawIndirectCount >= drawCallCount);

                    command.buffer.drawIndexedIndirectCount(
                        mBasePassCulling[alphaMode]
                            .compactIndirectDrawBuffers[mCurrentFrameIndex]
                            ->GetBufferHandle(),
                        0,
                        mBasePassCulling[alphaMode]
                            .drawCallCountBuffer[mCurrentFrameIndex]
                            ->GetBufferHandle(),
                        0,
                        drawCallCount,
                        sizeof(VkDrawIndexedIndirectCommand));
                }
                EndMarker(command.buffer);
            }
            command.buffer.endRenderPass();
            EndMarker(command.buffer);
        }

        // Render transparencies
        {
            BeginMarker(command.buffer, "Translucency pass");
            const std::vector<vk::ClearValue> clearValues{
                vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
                vk::ClearDepthStencilValue(1.0f, 0),
            };
            const vk::RenderPassBeginInfo renderPassBeginInfo =
                vk::RenderPassBeginInfo()
                    .setRenderPass(mTransparentPass->GetRenderPassHandle())
                    .setFramebuffer(mTransparentPass->GetFramebufferHandle(
                        mContext->GetSwapchain()->GetCurrentIndex()))
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
                        mBasePassPipeline[alphaMode].pipeline->GetPipelineHandle());

                    std::vector<vk::DescriptorSet> descriptorSets =
                        mBasePassPipeline[alphaMode].parameters->GetDescriptorSets(
                            mCurrentFrameIndex);

                    command.buffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics,
                        mBasePassPipeline[alphaMode].pipeline->GetPipelineLayout(),
                        0,
                        descriptorSets,
                        nullptr);

                    {
                        mScene->GetMeshSystem()->BindBuffers(command.buffer);

                        uint32_t maxDrawIndirectCount = mContext->GetDevice()
                                                            ->GetDeviceProperties()
                                                            .limits.maxDrawIndirectCount;
                        VKRT_ASSERT(maxDrawIndirectCount >= drawCallCount);

                        command.buffer.drawIndexedIndirectCount(
                            mBasePassCulling[alphaMode]
                                .compactIndirectDrawBuffers[mCurrentFrameIndex]
                                ->GetBufferHandle(),
                            0,
                            mBasePassCulling[alphaMode]
                                .drawCallCountBuffer[mCurrentFrameIndex]
                                ->GetBufferHandle(),
                            0,
                            drawCallCount,
                            sizeof(VkDrawIndexedIndirectCommand));
                    }
                }
            }
            command.buffer.endRenderPass();
            EndMarker(command.buffer);
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
}

}  // namespace VKRT