#include "DDGIRenderer.h"

#include <random>

#include "DebugUtils.h"
#include "Texture.h"

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
      } {
    AddRenderTargets();
    AddPipelines();
}

void DDGIRenderer::AddRenderTargets() {
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
                mSettingsManager->GetProbeResolution().x,
                mSettingsManager->GetProbeResolution().y,
                probeCount,
                vk::Format::eB10G11R11UfloatPack32,
                vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
                vk::ImageLayout::eShaderReadOnlyOptimal);
        }

        for (uint32_t index = 0; index < mProbeMomentsBuffers.size(); ++index) {
            mProbeMomentsBuffers[index] = new Texture(
                mContext,
                mSettingsManager->GetProbeResolution().x,
                mSettingsManager->GetProbeResolution().y,
                probeCount,
                vk::Format::eR16G16Sfloat,
                vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
                vk::ImageLayout::eShaderReadOnlyOptimal);
        }
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
}

void DDGIRenderer::AddResources() {
    mDDGIProbeData = mContext->GetDevice()->CreateBuffers(
        mContext->GetMaxInFlightFrameCount(),
        sizeof(DDGIData),
        vk::BufferUsageFlagBits::eUniformBuffer,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
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

        mDDGIData.randomRotation = randomRotation();
        if (mDDGIData.probeGridOrigin != mSettingsManager->GetProbeGridOrigin() ||
            mDDGIData.probeSpacing != mSettingsManager->GetProbeSpacing()) {
            mDDGIData.frameIndex = 0;
        }
        mDDGIProbeData[frameIndex]->Write(reinterpret_cast<uint8_t*>(&mDDGIData), sizeof(DDGIData));

        mDDGIData.frameIndex++;
    }
}

void DDGIRenderer::Render(
    Camera* camera,
    vk::CommandBuffer commandBuffer,
    const uint32_t frameIndex) {
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
                    mSettingsManager->GetProbeResolution().x % 8 == 0 &&
                    mSettingsManager->GetProbeResolution().y % 8 == 0);
                commandBuffer.dispatch(
                    mSettingsManager->GetProbeResolution().x / 8,
                    mSettingsManager->GetProbeResolution().y / 8,
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

DDGIRenderer::~DDGIRenderer() {}

}  // namespace VKRT