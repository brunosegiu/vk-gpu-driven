#include "Renderer.h"

#include <random>

#include "DebugUtils.h"
#include "Texture.h"

namespace VKRT {
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
    mRenderPass = new RenderPass(context, {mRenderTarget, mDepthRenderTarget});

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
    }

    // Culling resources
    {
        mCullingParameters = new ShaderParameterCollection(mContext);
        mCullingParameters->AddParameter(mCameraUniform);
        mCullingParameters->AddParameter(mPerDrawParameters);

        mIndirectDrawBufferParameter = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eStorageBuffer,
            ShaderParameter::UpdateFrequency::PerFrame,
            vk::ShaderStageFlagBits::eCompute);
        mCullingParameters->AddParameter(mIndirectDrawBufferParameter);

        mCullingPipeline = new ComputePipeline(
            context,
            mCullingParameters,
            {vk::ShaderStageFlagBits::eCompute, Resource::Id::CullingShader});
    }

    // Main pass resources
    {
        mMainPassParameters = new ShaderParameterCollection(mContext);

        mMainPassParameters->AddParameter(mCameraUniform);
        mMainPassParameters->AddParameter(mPerDrawParameters);

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
            new GraphicsPipeline(context, mMainPassParameters, stages, mRenderPass, geometryLayout);
    }
}

void Renderer::UpdateCameraUniforms(Camera* camera, uint32_t imageIndex) {
    ScopedRefPtr<VulkanBuffer> buffer = mCameraUniform->GetBuffer(imageIndex);
    uint8_t* mappedBuffer = buffer->MapBuffer();

    const std::vector<ScopedRefPtr<Object>>& objects = mScene->GetFlattenedObjects();
    size_t drawCallCount = 0;
    for (const ScopedRefPtr<Object>& object : objects) {
        const std::vector<ScopedRefPtr<Mesh>>& meshes = object->GetMeshes();
        drawCallCount += meshes.size();
    }
    CameraProperties cameraMatrices{
        .viewProjection = camera->GetProjectionTransform() * camera->GetViewTransform(),
        .cameraForwardDir = glm::vec4(camera->GetForwardDir(), 0.0f),
        .maxDrawCount = static_cast<uint32_t>(drawCallCount)};
    std::copy_n(
        reinterpret_cast<uint8_t*>(&cameraMatrices),
        sizeof(CameraProperties),
        mappedBuffer);
    buffer->UnmapBuffer();
}

void Renderer::UpdatePerDrawBuffer(uint32_t imageIndex) {
    mScene->Update();
    if (mPerDrawBuffers.empty()) {
        uint32_t descriptorCount = 0;
        const std::vector<ScopedRefPtr<Object>>& objects = mScene->GetFlattenedObjects();
        size_t perDrawBufferSize = 0;
        for (const ScopedRefPtr<Object>& object : objects) {
            std::vector<ScopedRefPtr<Mesh>> meshes = object->GetMeshes();
            for (ScopedRefPtr<Mesh>& mesh : meshes) {
                perDrawBufferSize += sizeof(SceneData);
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

    const std::vector<ScopedRefPtr<Object>>& objects = mScene->GetFlattenedObjects();
    ScopedRefPtr<VulkanBuffer> currentBuffer = mPerDrawBuffers[imageIndex];
    size_t drawCallCount = 0;
    for (const ScopedRefPtr<Object>& object : objects) {
        const std::vector<ScopedRefPtr<Mesh>>& meshes = object->GetMeshes();
        drawCallCount += meshes.size();
    }
    std::vector<SceneData> parameters;
    parameters.reserve(drawCallCount);
    for (const ScopedRefPtr<Object>& object : objects) {
        const std::vector<ScopedRefPtr<Mesh>>& meshes = object->GetMeshes();
        for (const ScopedRefPtr<Mesh>& mesh : meshes) {
            SceneData meshParameters{
                .indexCount = mesh->GetIndexCount(),
                .firstIndex = mesh->GetFirstIndex(),
                .vertexOffset = static_cast<int32_t>(mesh->GetVertexOffset()),
                .transform = object->GetAbsoluteTransform(),
                .materialId = mesh->GetMaterial()->GetMaterialId(),
                .normalTransform = glm::mat4(
                    glm::transpose(glm::inverse(glm::mat3(object->GetAbsoluteTransform()))))};
            parameters.push_back(meshParameters);
        }
    }
    uint8_t* buffer = currentBuffer->MapBuffer();
    std::copy_n(
        reinterpret_cast<const uint8_t*>(parameters.data()),
        parameters.size() * sizeof(SceneData),
        buffer);
    currentBuffer->UnmapBuffer();

    // Create indirect draw buffer
    {
        if (mIndirectDrawBuffers.empty()) {
            const uint32_t bufferCount = mContext->GetMaxInFlightFrameCount();
            for (uint32_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex) {
                ScopedRefPtr<VulkanBuffer> indirectBuffer = mContext->GetDevice()->CreateBuffer(
                    drawCallCount * sizeof(vk::DrawIndexedIndirectCommand),
                    vk::BufferUsageFlagBits::eIndirectBuffer |
                        vk::BufferUsageFlagBits::eStorageBuffer,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                        VMA_ALLOCATION_CREATE_MAPPED_BIT);
                mIndirectDrawBuffers.push_back(indirectBuffer);
            }

            mIndirectDrawBufferParameter->BindBuffers(mIndirectDrawBuffers);
        }
    }
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
            UpdatePerDrawBuffer(mCurrentFrameIndex);
            UpdateMaterialUniform();
            // TODO: Move to getter
            mMainPassParameters->CreateDescriptorSets();
            mMainPassParameters->UpdateDescriptors(mCurrentFrameIndex);

            mCullingParameters->CreateDescriptorSets();
            mCullingParameters->UpdateDescriptors(mCurrentFrameIndex);
        }

        const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();
        const std::vector<ScopedRefPtr<Object>>& objects = mScene->GetFlattenedObjects();
        size_t drawCallCount = 0;
        for (const ScopedRefPtr<Object>& object : objects) {
            const std::vector<ScopedRefPtr<Mesh>>& meshes = object->GetMeshes();
            drawCallCount += meshes.size();
        }

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
                mCullingParameters->GetDescriptorSets(mCurrentFrameIndex);

            command.buffer.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute,
                mCullingPipeline->GetPipelineLayout(),
                0,
                descriptorSets,
                nullptr);

            command.buffer.dispatch((drawCallCount / 64) + 1, 1, 1);
        }

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
            }

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

        {
            command.buffer.bindVertexBuffers(
                0,
                mScene->GetMeshSystem()->GetVertexBuffer()->GetBufferHandle(),
                {0});
            command.buffer.bindVertexBuffers(
                1,
                mScene->GetMeshSystem()->GetTexCoordBuffer()->GetBufferHandle(),
                {0});
            command.buffer.bindVertexBuffers(
                2,
                mScene->GetMeshSystem()->GetNormalBuffer()->GetBufferHandle(),
                {0});
            command.buffer.bindIndexBuffer(
                mScene->GetMeshSystem()->GetIndexBuffer()->GetBufferHandle(),
                {0},
                vk::IndexType::eUint32);

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