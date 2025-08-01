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
    uint32_t maxDrawCount;
};

struct DrawData {
    uint32_t indexCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    glm::mat4 transform;
    uint32_t materialId;
    glm::mat3 normalTransform;
    struct {
        glm::vec3 minBounds;
        glm::vec3 maxBounds;
    } aabb;
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
    mBasePass = new RenderPass(context, {mRenderTarget, mDepthRenderTarget});

    {
        mShadowMap = new Texture(
            mContext,
            4096,
            4096,
            vk::Format::eD16Unorm,
            vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);
        mDepthOnlyPassRenderTarget = new RenderTarget(mContext, mShadowMap);
        mDepthOnlyPass = new RenderPass(context, {mDepthOnlyPassRenderTarget});
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

        createCullingResources(mShadowPassCulling);
        createCullingResources(mBasePassCulling);
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

        mDepthOnlyParameters = new ShaderParameterCollection(mContext);
        mDepthOnlyParameters->AddParameter(mShadowCameraUniform);
        mDepthOnlyParameters->AddParameter(mPerDrawParameters);
        mDepthOnlyParameters->AddParameter(mShadowPassCulling.additionalDrawDataBufferParameter);
        mDepthOnlyParameters->AddParameter(mMaterialSampler);
        mDepthOnlyParameters->AddParameter(mMaterialsUniform);
        mDepthOnlyParameters->AddParameter(mMaterialsTextures);

        std::unordered_map<vk::ShaderStageFlagBits, Resource::Id> stages{
            {vk::ShaderStageFlagBits::eVertex, Resource::Id::DepthOnlyVertexShader},
            {vk::ShaderStageFlagBits::eFragment, Resource::Id::DepthOnlyFragmentShader},
        };

        const std::vector<GeometryLayout> geometryLayout{
            {.format = vk::Format::eR32G32B32A32Sfloat, .stride = sizeof(glm::vec3)},
            {.format = vk::Format::eR32Uint, .stride = sizeof(uint32_t)},
            {.format = vk::Format::eR32Uint, .stride = sizeof(uint32_t)},
        };

        mDepthOnlyPipeline = new GraphicsPipeline(
            context,
            mDepthOnlyParameters,
            stages,
            mDepthOnlyPass,
            geometryLayout,
            GraphicsPipelineOptionals{
                .enableDepthBias = true,
                .depthBias = 1.0f,
                .depthSlope = 1.0f * (2 + 1)});
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

        mBasePassParameters = new ShaderParameterCollection(mContext);
        mBasePassParameters->AddParameter(mCameraUniform);
        mBasePassParameters->AddParameter(mShadowCameraUniform);
        mBasePassParameters->AddParameter(mPerDrawParameters);
        mBasePassParameters->AddParameter(mBasePassCulling.additionalDrawDataBufferParameter);
        mBasePassParameters->AddParameter(mMaterialSampler);
        mBasePassParameters->AddParameter(mMaterialsUniform);
        mBasePassParameters->AddParameter(mShadowMapUniform);
        mBasePassParameters->AddParameter(mMaterialsTextures);

        std::unordered_map<vk::ShaderStageFlagBits, Resource::Id> stages{
            {vk::ShaderStageFlagBits::eVertex, Resource::Id::VertexShader},
            {vk::ShaderStageFlagBits::eFragment, Resource::Id::FragmentShader},
        };

        const std::vector<GeometryLayout> geometryLayout = MeshSystem::GetGeometryLayout();

        mBasePassPipeline =
            new GraphicsPipeline(context, mBasePassParameters, stages, mBasePass, geometryLayout);
    }
}

void Renderer::UpdateUniforms(Camera* camera, uint32_t imageIndex) {
    // Create and update per-draw parameters
    if (mPerDrawBuffers.empty()) {
        uint32_t descriptorCount = 0;
        const std::vector<ScopedRefPtr<Object>>& objects = mScene->GetFlattenedObjects();
        size_t perDrawBufferSize = 0;
        for (const ScopedRefPtr<Object>& object : objects) {
            std::vector<ScopedRefPtr<Mesh>> meshes = object->GetMeshes();
            for (ScopedRefPtr<Mesh>& mesh : meshes) {
                perDrawBufferSize += sizeof(DrawData);
            }
        }
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
        const std::vector<ScopedRefPtr<Object>>& objects = mScene->GetFlattenedObjects();
        ScopedRefPtr<VulkanBuffer> currentBuffer = mPerDrawBuffers[imageIndex];
        std::vector<DrawData> parameters;
        parameters.reserve(mScene->GetDrawCallCount());
        for (const ScopedRefPtr<Object>& object : objects) {
            const std::vector<ScopedRefPtr<Mesh>>& meshes = object->GetMeshes();
            for (const ScopedRefPtr<Mesh>& mesh : meshes) {
                DrawData meshParameters{
                    .indexCount = mesh->GetIndexCount(),
                    .firstIndex = mesh->GetFirstIndex(),
                    .vertexOffset = static_cast<int32_t>(mesh->GetVertexOffset()),
                    .transform = object->GetAbsoluteTransform(),
                    .materialId = mesh->GetMaterial()->GetMaterialId(),
                    .normalTransform = glm::mat4(
                        glm::transpose(glm::inverse(glm::mat3(object->GetAbsoluteTransform())))),
                    .aabb = {
                        .minBounds = mesh->GetAABB().GetMin(),
                        .maxBounds = mesh->GetAABB().GetMax()}};
                parameters.push_back(meshParameters);
            }
        }
        uint8_t* buffer = currentBuffer->MapBuffer();
        std::copy_n(
            reinterpret_cast<const uint8_t*>(parameters.data()),
            parameters.size() * sizeof(DrawData),
            buffer);
        currentBuffer->UnmapBuffer();
    }

    // Update culling parameters
    auto updateCullingResources = [&](Renderer::CullingPipelineResources& resources,
                                      CullData cullData) {
        resources.cullingDataUniform->Write(
            imageIndex,
            reinterpret_cast<uint8_t*>(&cullData),
            sizeof(CullData));

        const uint32_t bufferCount = mContext->GetMaxInFlightFrameCount();

        if (resources.indirectDrawBuffers.empty()) {
            resources.indirectDrawBuffers = mContext->GetDevice()->CreateBuffers(
                bufferCount,
                mScene->GetDrawCallCount() * sizeof(vk::DrawIndexedIndirectCommand),
                vk::BufferUsageFlagBits::eStorageBuffer,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
            resources.indirectDrawBufferParameter->BindBuffers(resources.indirectDrawBuffers);
        }

        if (resources.compactIndirectDrawBuffers.empty()) {
            resources.compactIndirectDrawBuffers = mContext->GetDevice()->CreateBuffers(
                bufferCount,
                mScene->GetDrawCallCount() * sizeof(vk::DrawIndexedIndirectCommand),
                vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
            resources.compactIndirectDrawBufferParameter->BindBuffers(
                resources.compactIndirectDrawBuffers);
        }

        if (resources.additionalDrawDataBuffers.empty()) {
            resources.additionalDrawDataBuffers = mContext->GetDevice()->CreateBuffers(
                bufferCount,
                mScene->GetDrawCallCount() * sizeof(uint32_t),
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

    CullData shadowCullingData{
        .frustumPlanes = ViewFrustum(mScene->GetLight().ComputeShadowMatrix()).GetPlanes(),
        .maxDrawCount = static_cast<uint32_t>(mScene->GetDrawCallCount())};
    updateCullingResources(mShadowPassCulling, shadowCullingData);

    CullData mainCameraCullingData{
        .frustumPlanes = camera->GetViewFrustum().GetPlanes(),
        .maxDrawCount = static_cast<uint32_t>(mScene->GetDrawCallCount())};
    updateCullingResources(mBasePassCulling, mainCameraCullingData);

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
        const std::vector<ScopedRefPtr<Object>>& objects = mScene->GetFlattenedObjects();
        size_t drawCallCount = mScene->GetDrawCallCount();

        const auto dispatchCulling = [&](CullingPipelineResources& resources) {
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

            command.buffer.dispatch((drawCallCount / 64) + 1, 1, 1);

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

            command.buffer.dispatch((drawCallCount / 64) + 1, 1, 1);

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

        dispatchCulling(mShadowPassCulling);
        dispatchCulling(mBasePassCulling);

        // Shadow pass
        {
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

            command.buffer.bindPipeline(
                vk::PipelineBindPoint::eGraphics,
                mDepthOnlyPipeline->GetPipelineHandle());

            std::vector<vk::DescriptorSet> descriptorSets =
                mDepthOnlyParameters->GetDescriptorSets(mCurrentFrameIndex);

            command.buffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                mDepthOnlyPipeline->GetPipelineLayout(),
                0,
                descriptorSets,
                nullptr);

            {
                mScene->GetMeshSystem()->BindBuffers(command.buffer);

                uint32_t maxDrawIndirectCount =
                    mContext->GetDevice()->GetDeviceProperties().limits.maxDrawIndirectCount;
                VKRT_ASSERT(maxDrawIndirectCount >= drawCallCount);

                command.buffer.drawIndexedIndirectCount(
                    mShadowPassCulling.compactIndirectDrawBuffers[mCurrentFrameIndex]
                        ->GetBufferHandle(),
                    0,
                    mShadowPassCulling.drawCallCountBuffer[mCurrentFrameIndex]->GetBufferHandle(),
                    0,
                    drawCallCount,
                    sizeof(VkDrawIndexedIndirectCommand));
            }

            command.buffer.endRenderPass();
        }

        // Base pass
        {
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
            mBasePassPipeline->GetPipelineHandle());

        std::vector<vk::DescriptorSet> descriptorSets =
            mBasePassParameters->GetDescriptorSets(mCurrentFrameIndex);

        command.buffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mBasePassPipeline->GetPipelineLayout(),
            0,
            descriptorSets,
            nullptr);

        {
            mScene->GetMeshSystem()->BindBuffers(command.buffer);

            uint32_t maxDrawIndirectCount =
                mContext->GetDevice()->GetDeviceProperties().limits.maxDrawIndirectCount;
            VKRT_ASSERT(maxDrawIndirectCount >= drawCallCount);

            command.buffer.drawIndexedIndirectCount(
                mBasePassCulling.compactIndirectDrawBuffers[mCurrentFrameIndex]->GetBufferHandle(),
                0,
                mBasePassCulling.drawCallCountBuffer[mCurrentFrameIndex]->GetBufferHandle(),
                0,
                drawCallCount,
                sizeof(VkDrawIndexedIndirectCommand));
        }

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