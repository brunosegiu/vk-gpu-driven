#include "DDGIRenderer.h"

#include <random>

#include "DebugUtils.h"
#include "Utils.h"

namespace VKRT {

DDGIRenderer::DDGIRenderer(
    ScopedRefPtr<Context> context,
    ScopedRefPtr<Scene> scene,
    ScopedRefPtr<SettingsManager> settingsManager)
    : mContext(context),
      mScene(scene),
      mSettingsManager(settingsManager),
      mCurrentFrame(0),
      mDDGIData{
          .probeGridCount = mSettingsManager->GetProbeGridCount(),
          .probeGridOrigin = mSettingsManager->GetProbeGridOrigin(),
          .probeSpacing = mSettingsManager->GetProbeSpacing(),
          .minRayLength = mSettingsManager->GetProbeMinRayLength(),
          .maxRayLength = mSettingsManager->GetProbeMaxRayLength(),
          .randomRotation = glm::mat3(1.0f),
          .hysteresis = mSettingsManager->GetHysteresis(),
          .frameIndex = 0,
      } {}

void DDGIRenderer::AddRenderTargets(
    const ScopedRefPtr<RenderTarget>& mainRenderTarget,
    const ScopedRefPtr<RenderTarget>& depthRenderTarget) {
    const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();
    // Probes
    {
        const uint32_t probeCount =
            mDDGIData.probeGridCount.x * mDDGIData.probeGridCount.y * mDDGIData.probeGridCount.z;
        mProbeRayRadianceBuffer = new Texture(
            mContext,
            mSettingsManager->GetProbeRayCount(),
            probeCount,
            1,
            vk::Format::eB10G11R11UfloatPack32,
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        mProbeRayDirectionDepthBuffer = new Texture(
            mContext,
            mSettingsManager->GetProbeRayCount(),
            probeCount,
            1,
            vk::Format::eR16G16B16A16Sfloat,
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        for (uint32_t index = 0; index < mProbeIrradianceBuffers.size(); ++index) {
            mProbeIrradianceBuffers[index] = new Texture(
                mContext,
                mSettingsManager->GetProbeResolution(),
                mSettingsManager->GetProbeResolution(),
                probeCount,
                vk::Format::eB10G11R11UfloatPack32,
                vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
                vk::ImageLayout::eShaderReadOnlyOptimal);
        }

        for (uint32_t index = 0; index < mProbeMomentsBuffers.size(); ++index) {
            mProbeMomentsBuffers[index] = new Texture(
                mContext,
                mSettingsManager->GetProbeResolution(),
                mSettingsManager->GetProbeResolution(),
                probeCount,
                vk::Format::eR16G16Sfloat,
                vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
                vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    }

    // Visualization
    {
        mVisualizeProbesPass = new RenderPass(
            mContext,
            {
                {.renderTarget = mainRenderTarget,
                 .loadOp = vk::AttachmentLoadOp::eLoad,
                 .initialLayout = vk::ImageLayout::eColorAttachmentOptimal,
                 .storeOp = vk::AttachmentStoreOp::eStore,
                 .finalLayout = vk::ImageLayout::eColorAttachmentOptimal},
                {.renderTarget = depthRenderTarget,
                 .loadOp = vk::AttachmentLoadOp::eLoad,
                 .initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                 .storeOp = vk::AttachmentStoreOp::eStore,
                 .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal},
            });
    }
}

void DDGIRenderer::AddPipelines() {
    // Raytracing resources
    const std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>> raytracingStages{
        {vk::ShaderStageFlagBits::eRaygenKHR, {Resource::Id::RaytraceProbeGenShader}},
        {vk::ShaderStageFlagBits::eClosestHitKHR, {Resource::Id::RaytraceProbeHitShader}},
        {vk::ShaderStageFlagBits::eMissKHR,
         std::vector<Resource::Id>{
             Resource::Id::RaytraceProbeMissShader,
             Resource::Id::RaytraceProbeShadowMissShader}}};

    mProbeRaytracingPipeline = new RaytracingPipeline(mContext, raytracingStages);

    mUpdateProbePipeline = new ComputePipeline(
        mContext,
        {{vk::ShaderStageFlagBits::eCompute, {Resource::Id::UpdateProbesShader}}});

    // Visualization
    mVisualizeProbesPipeline = new GraphicsPipeline(
        mContext,
        {
            {vk::ShaderStageFlagBits::eVertex, {Resource::Id::ProbeVertexShader}},
            {vk::ShaderStageFlagBits::eFragment, {Resource::Id::ProbeFragmentShader}},
        },
        mVisualizeProbesPass,
        {{.format = vk::Format::eR32G32B32A32Sfloat, .stride = sizeof(glm::vec3)}},
        {.reverseWindingOrder = true});
}

void DDGIRenderer::AddResources() {
    mDDGIProbeData = mContext->GetDevice()->CreateBuffers(
        mContext->GetMaxInFlightFrameCount(),
        sizeof(DDGIData),
        vk::BufferUsageFlagBits::eUniformBuffer,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    const Utils::Geometry sphere = Utils::BuildSphere(8, 8);
    Utils::UploadBuffer(
        mContext,
        mSpherePositions,
        sphere.positions,
        vk::BufferUsageFlags{vk::BufferUsageFlagBits::eVertexBuffer});
    Utils::UploadBuffer(
        mContext,
        mSphereIndices,
        sphere.indices,
        vk::BufferUsageFlags{vk::BufferUsageFlagBits::eIndexBuffer});
}

void DDGIRenderer::RemoveRenderTargets() {
    mProbeRayRadianceBuffer = nullptr;
}

void DDGIRenderer::RemovePipelines() {}

void DDGIRenderer::RemoveResources() {}

void DDGIRenderer::UpdatePersistentUniforms(const PersistentParameters& parameters) {
    // DDGI
    {
        {
            mProbeRaytracingPipeline->Bind(0, parameters.mScenePersistentDataParameter);
            mProbeRaytracingPipeline->Bind(1, mScene->GetTLAS());
            mProbeRaytracingPipeline->Bind(2, mProbeRayRadianceBuffer);
            mProbeRaytracingPipeline->Bind(3, mProbeRayDirectionDepthBuffer);
            mProbeRaytracingPipeline->Bind(4, parameters.mMaterialSampler);
            mProbeRaytracingPipeline->Bind(5, parameters.mFrameBufferSampler);
            mProbeRaytracingPipeline->Bind(6, parameters.mMaterialsUniform);
            mProbeRaytracingPipeline->Bind(7, parameters.mIndexBufferUniform);
            mProbeRaytracingPipeline->Bind(8, parameters.mPositionBufferUniform);
            mProbeRaytracingPipeline->Bind(9, parameters.mTexCoordBufferUniform);
            mProbeRaytracingPipeline->Bind(10, parameters.mNormalBufferUniform);
            mProbeRaytracingPipeline->Bind(11, parameters.mTangentBufferUniform);
            mProbeRaytracingPipeline->Bind(12, parameters.mMaterialsTextures);
        }

        // Compute
        {
            mUpdateProbePipeline->Bind(0, parameters.mFrameBufferSampler);
            mUpdateProbePipeline->Bind(1, mProbeRayRadianceBuffer);
            mUpdateProbePipeline->Bind(2, mProbeRayDirectionDepthBuffer);
        }
    }

    // Visualization
    {
        mVisualizeProbesPipeline->Bind(0, parameters.mFrameBufferSampler);
    }
}

void DDGIRenderer::UpdateUniforms(const PerFrameParameters& parameters, uint32_t frameIndex) {
    mCurrentFrame = (mCurrentFrame + 1) % mProbeMomentsBuffers.size();

    mProbeRaytracingPipeline->Bind(frameIndex, 0, parameters.mCameraUniform[frameIndex]);
    mProbeRaytracingPipeline->Bind(frameIndex, 1, parameters.mShadowCameraUniform[frameIndex]);
    mProbeRaytracingPipeline->Bind(frameIndex, 2, parameters.mPerMeshParameters[frameIndex]);
    mProbeRaytracingPipeline->Bind(frameIndex, 3, mDDGIProbeData[frameIndex]);
    mProbeRaytracingPipeline->Bind(frameIndex, 4, GetPreviousIrradianceBuffer());
    mProbeRaytracingPipeline->Bind(frameIndex, 5, GetPreviousMomentsBuffer());

    mUpdateProbePipeline->Bind(frameIndex, 0, mDDGIProbeData[frameIndex]);
    mUpdateProbePipeline->Bind(frameIndex, 1, GetPreviousIrradianceBuffer());
    mUpdateProbePipeline->Bind(frameIndex, 2, GetPreviousMomentsBuffer());
    mUpdateProbePipeline->Bind(frameIndex, 3, GetIrradianceBuffer());
    mUpdateProbePipeline->Bind(frameIndex, 4, GetMomentsBuffer());

    // DDGI
    {
        auto randomRotation = [&]() {
            float angle =
                2.0f * glm::pi<float>() * glm::fract(0.61803398875 * float(mDDGIData.frameIndex));
            glm::vec3 axis = glm::normalize(glm::vec3(1.0, 1.0, 1.0));
            return glm::toMat3(glm::angleAxis(angle, axis));
        };

        mDDGIData.probeGridCount = mSettingsManager->GetProbeGridCount();
        mDDGIData.probeGridOrigin = mSettingsManager->GetProbeGridOrigin();
        mDDGIData.probeSpacing = mSettingsManager->GetProbeSpacing();
        mDDGIData.minRayLength = mSettingsManager->GetProbeMinRayLength();
        mDDGIData.maxRayLength = mSettingsManager->GetProbeMaxRayLength();
        mDDGIData.hysteresis = mSettingsManager->GetHysteresis();
        mDDGIData.probeRadius = mSettingsManager->GetProbeRadius();

        mDDGIData.randomRotation = randomRotation();
        if (mDDGIData.probeGridOrigin != mSettingsManager->GetProbeGridOrigin() ||
            mDDGIData.probeSpacing != mSettingsManager->GetProbeSpacing()) {
            mDDGIData.frameIndex = 0;
        }
        mDDGIProbeData[frameIndex]->Write(reinterpret_cast<uint8_t*>(&mDDGIData), sizeof(DDGIData));

        mDDGIData.frameIndex++;
    }

    // Visualization
    mVisualizeProbesPipeline->Bind(frameIndex, 0, parameters.mCameraUniform[frameIndex]);
    mVisualizeProbesPipeline->Bind(frameIndex, 1, mDDGIProbeData[frameIndex]);
    mVisualizeProbesPipeline->Bind(frameIndex, 2, GetIrradianceBuffer());
}

void DDGIRenderer::Render(vk::CommandBuffer commandBuffer, const uint32_t frameIndex) {
    mContext->BeginMarker(commandBuffer, "DDGI");
    {
        mContext->BeginMarker(commandBuffer, "Trace rays");
        {
            {
                std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                    {
                        {mProbeRayRadianceBuffer,
                         vk::ImageLayout::eShaderReadOnlyOptimal,
                         vk::ImageLayout::eGeneral},
                        {mProbeRayDirectionDepthBuffer,
                         vk::ImageLayout::eShaderReadOnlyOptimal,
                         vk::ImageLayout::eGeneral},
                        {GetPreviousIrradianceBuffer(),
                         vk::ImageLayout::eShaderReadOnlyOptimal,
                         vk::ImageLayout::eShaderReadOnlyOptimal},
                        {GetPreviousMomentsBuffer(),
                         vk::ImageLayout::eShaderReadOnlyOptimal,
                         vk::ImageLayout::eShaderReadOnlyOptimal},
                    });

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                    vk::DependencyFlags{},
                    {},
                    {},
                    imageBarriers);
            }

            commandBuffer.bindPipeline(
                vk::PipelineBindPoint::eRayTracingKHR,
                mProbeRaytracingPipeline->GetPipelineHandle());
            std::vector<vk::DescriptorSet> descriptorSets =
                mProbeRaytracingPipeline->GetDescriptorSets(frameIndex);
            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eRayTracingKHR,
                mProbeRaytracingPipeline->GetPipelineLayout(),
                0,
                descriptorSets,
                nullptr);

            const uint32_t probeCount = mDDGIData.probeGridCount.x * mDDGIData.probeGridCount.y *
                                        mDDGIData.probeGridCount.z;

            const RaytracingPipeline::RayTracingTablesRef& tableRef =
                mProbeRaytracingPipeline->GetTablesRef();
            commandBuffer.traceRaysKHR(
                tableRef.rayGen,
                tableRef.rayMiss,
                tableRef.rayHit,
                tableRef.callable,
                mSettingsManager->GetProbeRayCount(),
                probeCount,
                1,
                mContext->GetDevice()->GetDispatcher());

            {
                std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                    vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                    vk::PipelineStageFlagBits::eComputeShader,
                    {
                        {mProbeRayRadianceBuffer,
                         vk::ImageLayout::eGeneral,
                         vk::ImageLayout::eShaderReadOnlyOptimal},
                        {mProbeRayDirectionDepthBuffer,
                         vk::ImageLayout::eGeneral,
                         vk::ImageLayout::eShaderReadOnlyOptimal},
                    });

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::DependencyFlags{},
                    {},
                    {},
                    imageBarriers);
            }
        }
        mContext->EndMarker(commandBuffer);

        mContext->BeginMarker(commandBuffer, "Update probes");
        {
            {
                std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eComputeShader,
                    {
                        {GetIrradianceBuffer(),
                         vk::ImageLayout::eShaderReadOnlyOptimal,
                         vk::ImageLayout::eGeneral},
                        {GetMomentsBuffer(),
                         vk::ImageLayout::eShaderReadOnlyOptimal,
                         vk::ImageLayout::eGeneral},
                    });

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::DependencyFlags{},
                    {},
                    {},
                    imageBarriers);
            }
            const uint32_t probeCount = mDDGIData.probeGridCount.x * mDDGIData.probeGridCount.y *
                                        mDDGIData.probeGridCount.z;
            {
                commandBuffer.bindPipeline(
                    vk::PipelineBindPoint::eCompute,
                    mUpdateProbePipeline->GetPipelineHandle());

                std::vector<vk::DescriptorSet> updateProbeDescriptors =
                    mUpdateProbePipeline->GetDescriptorSets(frameIndex);

                commandBuffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eCompute,
                    mUpdateProbePipeline->GetPipelineLayout(),
                    0,
                    updateProbeDescriptors,
                    nullptr);

                VKRT_ASSERT(
                    mSettingsManager->GetProbeResolution() % 8 == 0 &&
                    mSettingsManager->GetProbeResolution() % 8 == 0);
                commandBuffer.dispatch(
                    mSettingsManager->GetProbeResolution() / 8,
                    mSettingsManager->GetProbeResolution() / 8,
                    probeCount);
            }

            {
                std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    {
                        {GetIrradianceBuffer(),
                         vk::ImageLayout::eGeneral,
                         vk::ImageLayout::eShaderReadOnlyOptimal},
                        {GetMomentsBuffer(),
                         vk::ImageLayout::eGeneral,
                         vk::ImageLayout::eShaderReadOnlyOptimal},
                    });

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::DependencyFlags{},
                    {},
                    {},
                    imageBarriers);
            }
        }
        mContext->EndMarker(commandBuffer);
    }
    mContext->EndMarker(commandBuffer);
}

void DDGIRenderer::RenderProbes(vk::CommandBuffer commandBuffer, const uint32_t frameIndex) {
    mContext->BeginMarker(commandBuffer, "Probe visualization pass");

    const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();

    const std::vector<vk::ClearValue> clearValues{
        vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
        vk::ClearDepthStencilValue(1.0f, 0),
    };
    const vk::RenderPassBeginInfo renderPassBeginInfo =
        vk::RenderPassBeginInfo()
            .setRenderPass(mVisualizeProbesPass->GetRenderPassHandle())
            .setFramebuffer(mVisualizeProbesPass->GetFramebufferHandle(
                mContext->GetSwapchain()->GetCurrentIndex()))
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

        const vk::Rect2D scissor =
            vk::Rect2D().setOffset(0).setExtent(vk::Extent2D{imageSize.width, imageSize.height});
        commandBuffer.setScissor(0, scissor);
    }

    commandBuffer.bindPipeline(
        vk::PipelineBindPoint::eGraphics,
        mVisualizeProbesPipeline->GetPipelineHandle());

    std::vector<vk::DescriptorSet> descriptorSets =
        mVisualizeProbesPipeline->GetDescriptorSets(frameIndex);

    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        mVisualizeProbesPipeline->GetPipelineLayout(),
        0,
        descriptorSets,
        nullptr);

    {
        commandBuffer.bindVertexBuffers(0, mSpherePositions->GetBufferHandle(), {0});
        commandBuffer.bindIndexBuffer(
            mSphereIndices->GetBufferHandle(),
            {0},
            vk::IndexType::eUint32);
        const uint32_t probeCount =
            mDDGIData.probeGridCount.x * mDDGIData.probeGridCount.y * mDDGIData.probeGridCount.z;
        commandBuffer
            .drawIndexed(mSphereIndices->GetBufferSize() / sizeof(uint32_t), probeCount, 0, 0, 0);
    }

    commandBuffer.endRenderPass();

    mContext->EndMarker(commandBuffer);
}

DDGIRenderer::~DDGIRenderer() {}

}  // namespace VKRT