#include "DDGIRenderer.h"

#include <random>

#include "DebugUtils.h"
#include "Texture.h"

namespace VKRT {

constexpr uint32_t raysPerProbe = 32;
constexpr glm::uvec3 probeGridCount(4u, 4u, 4u);
constexpr glm::uvec2 probeResolution(32u, 32u);
constexpr glm::vec3 probeSpacing(1.0f, 1.0f, 2.0f);
const glm::vec3 probeOrigin =
    glm::vec3(0.0f, 0.5f, 0.0f) - glm::vec3(probeGridCount) * probeSpacing * 0.5f;
constexpr float probeMaxRayLength = 1000.0f;
constexpr float probeMinRayLength = 0.01f;
constexpr float hysteresis = 0.75f;

DDGIData ddgiData{
    .probeGridCount = probeGridCount,
    .probeGridOrigin = probeOrigin,
    .probeSpacing = probeSpacing,
    .minRayLength = probeMinRayLength,
    .maxRayLength = probeMaxRayLength,
    .randomRotation = glm::mat3(1.0f),
    .hysteresis = hysteresis,
    .frameIndex = 0,
};

DDGIRenderer::DDGIRenderer(ScopedRefPtr<Context> context, ScopedRefPtr<Scene> scene)
    : mContext(context), mScene(scene), mCurrentFrame(0) {
    AddRenderTargets();
    AddPipelines();
}

void DDGIRenderer::AddRenderTargets() {
    const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();
    // Probes
    {
        const uint32_t probeCount =
            ddgiData.probeGridCount.x * ddgiData.probeGridCount.y * ddgiData.probeGridCount.z;
        mProbeRayRadianceBuffer = new Texture(
            mContext,
            raysPerProbe,
            probeCount,
            1,
            vk::Format::eB10G11R11UfloatPack32,
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        mProbeRayDirectionDepthBuffer = new Texture(
            mContext,
            raysPerProbe,
            probeCount,
            1,
            vk::Format::eR16G16B16A16Sfloat,
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        for (uint32_t index = 0; index < mProbeIrradianceBuffers.size(); ++index) {
            mProbeIrradianceBuffers[index] = new Texture(
                mContext,
                probeResolution.x,
                probeResolution.y,
                probeCount,
                vk::Format::eB10G11R11UfloatPack32,
                vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
                vk::ImageLayout::eShaderReadOnlyOptimal);
        }

        for (uint32_t index = 0; index < mProbeMomentsBuffers.size(); ++index) {
            mProbeMomentsBuffers[index] = new Texture(
                mContext,
                probeResolution.x,
                probeResolution.y,
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
        auto randomRotation = []() {
            static std::mt19937 gen{std::random_device{}()};
            static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            float u1 = dist(gen);
            float u2 = dist(gen);
            float u3 = dist(gen);
            glm::quat quat = glm::quat(glm::vec3(u1, u2, u3) * 2.0f * glm::pi<float>());
            return glm::toMat3(quat);
        };

        ddgiData.randomRotation = randomRotation();
        mDDGIProbeData[frameIndex]->Write(reinterpret_cast<uint8_t*>(&ddgiData), sizeof(DDGIData));
        ddgiData.frameIndex++;
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

            const uint32_t probeCount =
                ddgiData.probeGridCount.x * ddgiData.probeGridCount.y * ddgiData.probeGridCount.z;

            const RaytracingPipeline::RayTracingTablesRef& tableRef =
                mProbeRaytracingPipeline->GetTablesRef();
            commandBuffer.traceRaysKHR(
                tableRef.rayGen,
                tableRef.rayMiss,
                tableRef.rayHit,
                tableRef.callable,
                raysPerProbe,
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
                        {GetPreviousIrradianceBuffer(),
                         vk::ImageLayout::eShaderReadOnlyOptimal,
                         vk::ImageLayout::eGeneral},
                        {GetPreviousMomentsBuffer(),
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
            const uint32_t probeCount =
                ddgiData.probeGridCount.x * ddgiData.probeGridCount.y * ddgiData.probeGridCount.z;
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

                static_assert(probeResolution.x % 8 == 0 && probeResolution.y % 8 == 0);
                commandBuffer.dispatch(probeResolution.x / 8, probeResolution.y / 8, probeCount);
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
                        {GetPreviousIrradianceBuffer(),
                         vk::ImageLayout::eGeneral,
                         vk::ImageLayout::eShaderReadOnlyOptimal},
                        {GetPreviousMomentsBuffer(),
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