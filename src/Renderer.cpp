#include "Renderer.h"

#include <random>

#include "DebugUtils.h"
#include "Texture.h"

namespace VKRT {
struct CameraProperties {
    glm::mat4 viewProjection;
    glm::vec4 cameraForwardDir;
    std::array<glm::vec4, 6> frustumPlanes;
    uint32_t maxDrawCount;
};

struct LightProperties {
    glm::vec3 radiance;
    glm::vec3 direction;
    struct {
        glm::mat4 viewProjection;
        std::array<glm::vec4, 6> frustumPlanes;
        uint32_t maxDrawCount;
    } shadowParameters;
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

    // Shared parameters
    {
        mPerDrawParameters = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eStorageBuffer,
            ShaderParameter::UpdateFrequency::PerFrame,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment |
                vk::ShaderStageFlagBits::eCompute);

        mCameraUniform = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eUniformBuffer,
            ShaderParameter::UpdateFrequency::PerFrame,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex |
                vk::ShaderStageFlagBits::eCompute,
            sizeof(CameraProperties),
            vk::BufferUsageFlagBits::eUniformBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);

        mShadowCameraUniform = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eUniformBuffer,
            ShaderParameter::UpdateFrequency::PerFrame,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex |
                vk::ShaderStageFlagBits::eCompute,
            sizeof(LightProperties),
            vk::BufferUsageFlagBits::eUniformBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);

        mShadowMapUniform = new ShaderParameterImage(
            mContext,
            vk::DescriptorType::eSampledImage,
            vk::ShaderStageFlagBits::eFragment);
    }

    // Culling resources
    {
        mIndirectDrawBufferParameter = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eStorageBuffer,
            ShaderParameter::UpdateFrequency::PerFrame,
            vk::ShaderStageFlagBits::eCompute);

        mBasePassCullingParameters = new ShaderParameterCollection(mContext);
        mBasePassCullingParameters->AddParameter(mCameraUniform);
        mBasePassCullingParameters->AddParameter(mPerDrawParameters);
        mBasePassCullingParameters->AddParameter(mIndirectDrawBufferParameter);

        mCullingPipeline = new ComputePipeline(
            context,
            mBasePassCullingParameters,
            {vk::ShaderStageFlagBits::eCompute, Resource::Id::CullingShader});
    }

    // Main pass resources
    {
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

        mBasePassParameters = new ShaderParameterCollection(mContext);
        mBasePassParameters->AddParameter(mCameraUniform);
        mBasePassParameters->AddParameter(mShadowCameraUniform);
        mBasePassParameters->AddParameter(mPerDrawParameters);
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

    // Shadow pass parameters
    {
        mDepthOnlyParameters = new ShaderParameterCollection(mContext);
        mDepthOnlyParameters->AddParameter(mShadowCameraUniform);
        mDepthOnlyParameters->AddParameter(mPerDrawParameters);
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

        // Shadow culling resources
        {
            mShadowIndirectDrawBufferParameter = new ShaderParameterBuffer(
                mContext,
                vk::DescriptorType::eStorageBuffer,
                ShaderParameter::UpdateFrequency::PerFrame,
                vk::ShaderStageFlagBits::eCompute);

            mShadowPassCullingParameters = new ShaderParameterCollection(mContext);
            mShadowPassCullingParameters->AddParameter(mShadowCameraUniform);
            mShadowPassCullingParameters->AddParameter(mPerDrawParameters);
            mShadowPassCullingParameters->AddParameter(mShadowIndirectDrawBufferParameter);

            mShadowCullingPipeline = new ComputePipeline(
                context,
                mShadowPassCullingParameters,
                {vk::ShaderStageFlagBits::eCompute, Resource::Id::ShadowCullingShader});
        }
    }
}

void Renderer::UpdateUniforms(Camera* camera, uint32_t imageIndex) {
    // Update camera uniform
    {
        ScopedRefPtr<VulkanBuffer> buffer = mCameraUniform->GetBuffer(imageIndex);
        uint8_t* mappedBuffer = buffer->MapBuffer();

        CameraProperties cameraMatrices{
            .viewProjection = camera->GetProjectionTransform() * camera->GetViewTransform(),
            .cameraForwardDir = glm::vec4(camera->GetForwardDir(), 0.0f),
            .frustumPlanes = camera->GetViewFrustum().GetPlanes(),
            .maxDrawCount = static_cast<uint32_t>(mScene->GetDrawCallCount())};
        std::copy_n(
            reinterpret_cast<uint8_t*>(&cameraMatrices),
            sizeof(CameraProperties),
            mappedBuffer);
        buffer->UnmapBuffer();
    }

    // Create per-draw parameters
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

    // Create indirect draw buffer
    {
        if (mIndirectDrawBuffers.empty()) {
            const uint32_t bufferCount = mContext->GetMaxInFlightFrameCount();
            for (uint32_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex) {
                ScopedRefPtr<VulkanBuffer> indirectBuffer = mContext->GetDevice()->CreateBuffer(
                    mScene->GetDrawCallCount() * sizeof(vk::DrawIndexedIndirectCommand),
                    vk::BufferUsageFlagBits::eIndirectBuffer |
                        vk::BufferUsageFlagBits::eStorageBuffer,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                        VMA_ALLOCATION_CREATE_MAPPED_BIT);
                mIndirectDrawBuffers.push_back(indirectBuffer);
            }

            mIndirectDrawBufferParameter->BindBuffers(mIndirectDrawBuffers);
        }
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

    // Create shadow indirect draw buffer
    {
        if (mShadowIndirectDrawBuffers.empty()) {
            const uint32_t bufferCount = mContext->GetMaxInFlightFrameCount();
            for (uint32_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex) {
                ScopedRefPtr<VulkanBuffer> indirectBuffer = mContext->GetDevice()->CreateBuffer(
                    mScene->GetDrawCallCount() * sizeof(vk::DrawIndexedIndirectCommand),
                    vk::BufferUsageFlagBits::eIndirectBuffer |
                        vk::BufferUsageFlagBits::eStorageBuffer,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                        VMA_ALLOCATION_CREATE_MAPPED_BIT);
                mShadowIndirectDrawBuffers.push_back(indirectBuffer);
            }

            mShadowIndirectDrawBufferParameter->BindBuffers(mShadowIndirectDrawBuffers);
        }
    }

    // Shadow camera parameters
    {
        ScopedRefPtr<VulkanBuffer> buffer = mShadowCameraUniform->GetBuffer(imageIndex);
        uint8_t* mappedBuffer = buffer->MapBuffer();
        DirectionalLight& light = mScene->GetLight();
        glm::mat4 shadowMatrix = light.ComputeShadowMatrix();
        LightProperties cameraMatrices{
            .radiance = light.GetRadiance(),
            .direction = light.GetDirection(),
            .shadowParameters = {
                .viewProjection = shadowMatrix,
                .frustumPlanes = ViewFrustum(shadowMatrix).GetPlanes(),
                .maxDrawCount = static_cast<uint32_t>(mScene->GetDrawCallCount())}};
        std::copy_n(
            reinterpret_cast<uint8_t*>(&cameraMatrices),
            sizeof(LightProperties),
            mappedBuffer);
        buffer->UnmapBuffer();
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

        // Shadow pass culling
        {
            {
                ScopedRefPtr<VulkanBuffer> indirectBuffer =
                    mShadowIndirectDrawBuffers[mCurrentFrameIndex];
                vk::BufferMemoryBarrier bufferBarrier =
                    vk::BufferMemoryBarrier()
                        .setBuffer(indirectBuffer->GetBufferHandle())
                        .setSize(indirectBuffer->GetBufferSize())
                        .setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead)
                        .setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eDrawIndirect,
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::DependencyFlags{},
                    {},
                    bufferBarrier,
                    {});
            }

            command.buffer.bindPipeline(
                vk::PipelineBindPoint::eCompute,
                mShadowCullingPipeline->GetPipelineHandle());

            std::vector<vk::DescriptorSet> descriptorSets =
                mShadowPassCullingParameters->GetDescriptorSets(mCurrentFrameIndex);

            command.buffer.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute,
                mShadowCullingPipeline->GetPipelineLayout(),
                0,
                descriptorSets,
                nullptr);

            command.buffer.dispatch((drawCallCount / 64) + 1, 1, 1);
        }

        // Draw call GPU culling pass
        {
            {
                ScopedRefPtr<VulkanBuffer> indirectBuffer =
                    mIndirectDrawBuffers[mCurrentFrameIndex];

                vk::BufferMemoryBarrier bufferBarrier =
                    vk::BufferMemoryBarrier()
                        .setBuffer(indirectBuffer->GetBufferHandle())
                        .setSize(indirectBuffer->GetBufferSize())
                        .setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead)
                        .setDstAccessMask(vk::AccessFlagBits::eShaderWrite);

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eDrawIndirect,
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::DependencyFlags{},
                    {},
                    bufferBarrier,
                    {});
            }

            command.buffer.bindPipeline(
                vk::PipelineBindPoint::eCompute,
                mCullingPipeline->GetPipelineHandle());

            std::vector<vk::DescriptorSet> descriptorSets =
                mBasePassCullingParameters->GetDescriptorSets(mCurrentFrameIndex);

            command.buffer.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute,
                mCullingPipeline->GetPipelineLayout(),
                0,
                descriptorSets,
                nullptr);

            command.buffer.dispatch((drawCallCount / 64) + 1, 1, 1);
        }

        // Shadow pass
        {
            {
                ScopedRefPtr<VulkanBuffer> indirectBuffer =
                    mShadowIndirectDrawBuffers[mCurrentFrameIndex];

                vk::BufferMemoryBarrier bufferBarrier =
                    vk::BufferMemoryBarrier()
                        .setBuffer(indirectBuffer->GetBufferHandle())
                        .setSize(indirectBuffer->GetBufferSize())
                        .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                        .setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eDrawIndirect,
                    vk::DependencyFlags{},
                    {},
                    bufferBarrier,
                    {});

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

                ScopedRefPtr<VulkanBuffer> indirectBuffer =
                    mShadowIndirectDrawBuffers[mCurrentFrameIndex];

                VKRT_ASSERT(
                    indirectBuffer->GetBufferSize() ==
                    drawCallCount * sizeof(VkDrawIndexedIndirectCommand));

                uint32_t maxDrawIndirectCount =
                    mContext->GetDevice()->GetDeviceProperties().limits.maxDrawIndirectCount;
                VKRT_ASSERT(maxDrawIndirectCount >= drawCallCount);

                command.buffer.drawIndexedIndirect(
                    indirectBuffer->GetBufferHandle(),
                    0,
                    drawCallCount,
                    sizeof(VkDrawIndexedIndirectCommand));
            }

            command.buffer.endRenderPass();
        }

        // Base pass
        {
            {
                ScopedRefPtr<VulkanBuffer> indirectBuffer =
                    mIndirectDrawBuffers[mCurrentFrameIndex];

                vk::BufferMemoryBarrier bufferBarrier =
                    vk::BufferMemoryBarrier()
                        .setBuffer(indirectBuffer->GetBufferHandle())
                        .setSize(indirectBuffer->GetBufferSize())
                        .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                        .setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eDrawIndirect,
                    vk::DependencyFlags{},
                    {},
                    bufferBarrier,
                    {});

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

            ScopedRefPtr<VulkanBuffer> indirectBuffer = mIndirectDrawBuffers[mCurrentFrameIndex];

            VKRT_ASSERT(
                indirectBuffer->GetBufferSize() ==
                drawCallCount * sizeof(VkDrawIndexedIndirectCommand));

            uint32_t maxDrawIndirectCount =
                mContext->GetDevice()->GetDeviceProperties().limits.maxDrawIndirectCount;
            VKRT_ASSERT(maxDrawIndirectCount >= drawCallCount);

            command.buffer.drawIndexedIndirect(
                indirectBuffer->GetBufferHandle(),
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