#include "GlossyReflectionsRenderer.h"

#include <random>

#include "DebugUtils.h"
#include "Utils.h"

namespace VKRT {

struct BlurControlData {
    float sigmaSpatial;
    float sigmaDepth;
};

GlossyReflectionsRenderer::GlossyReflectionsRenderer(
    ScopedRefPtr<Context> context,
    ScopedRefPtr<Scene> scene,
    ScopedRefPtr<SettingsManager> settingsManager)
    : mContext(context), mScene(scene), mSettingsManager(settingsManager) {}

constexpr uint32_t MaxReflectionMips = 4u;

void GlossyReflectionsRenderer::AddRenderTargets() {
    const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();
    const uint32_t blurredMipLevels = std::min(
        MaxReflectionMips,
        static_cast<uint32_t>(floor(log2(std::max(imageSize.width, imageSize.height))) + 1));

    mReflectionsBuffer = new Texture(
        mContext,
        imageSize.width,
        imageSize.height,
        1,
        blurredMipLevels,
        vk::Format::eB10G11R11UfloatPack32,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
        vk::ImageLayout::eShaderReadOnlyOptimal);

    mReflectionHitDepthBuffer = new Texture(
        mContext,
        imageSize.width,
        imageSize.height,
        1,
        1,
        vk::Format::eR16Sfloat,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
        vk::ImageLayout::eShaderReadOnlyOptimal);
}

void GlossyReflectionsRenderer::AddPipelines() {
    // Raytracing resources
    const std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>> raytracingStages{
        {vk::ShaderStageFlagBits::eRaygenKHR, {Resource::Id::GlossyReflectionsGenShader}},
        {vk::ShaderStageFlagBits::eClosestHitKHR, {Resource::Id::GlossyReflectionsHitShader}},
        {vk::ShaderStageFlagBits::eMissKHR, {Resource::Id::GlossyReflectionsMissShader}}};

    mTraceReflectionsPipeline = new RaytracingPipeline(mContext, raytracingStages);

    for (uint32_t mipIndex = 0; mipIndex < mReflectionsBuffer->GetMipLevels(); ++mipIndex) {
        ScopedRefPtr<ComputePipeline> pipeline = new ComputePipeline(
            mContext,
            {{vk::ShaderStageFlagBits::eCompute, {Resource::Id::BlurReflectionsShader}}});
        mBlurReflectionsPipeline.push_back(pipeline);
    }
}

void GlossyReflectionsRenderer::AddResources() {
    mBlurControlParameters = mContext->GetDevice()->CreateBuffers(
        mContext->GetMaxInFlightFrameCount(),
        sizeof(BlurControlData),
        vk::BufferUsageFlagBits::eUniformBuffer,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
}

void GlossyReflectionsRenderer::RemoveRenderTargets() {
    mReflectionsBuffer = nullptr;
    mReflectionHitDepthBuffer = nullptr;
}

void GlossyReflectionsRenderer::RemovePipelines() {
    mTraceReflectionsPipeline = nullptr;
    mBlurReflectionsPipeline.clear();
}

void GlossyReflectionsRenderer::RemoveResources() {}

void GlossyReflectionsRenderer::UpdatePersistentUniforms(const PersistentParameters& parameters) {
    mTraceReflectionsPipeline->Bind(0, parameters.mScenePersistentDataParameter);
    mTraceReflectionsPipeline->Bind(1, mScene->GetTLAS());
    mTraceReflectionsPipeline->Bind(2, parameters.mVisibilityBuffer);
    mTraceReflectionsPipeline->Bind(3, parameters.mShadowMap);
    mTraceReflectionsPipeline->Bind(4, mReflectionsBuffer);
    mTraceReflectionsPipeline->Bind(5, mReflectionHitDepthBuffer);
    mTraceReflectionsPipeline->Bind(6, parameters.mMaterialSampler);
    mTraceReflectionsPipeline->Bind(7, parameters.mFrameBufferSampler);
    mTraceReflectionsPipeline->Bind(8, parameters.mMaterialsUniform);
    mTraceReflectionsPipeline->Bind(9, parameters.mIndexBufferUniform);
    mTraceReflectionsPipeline->Bind(10, parameters.mPositionBufferUniform);
    mTraceReflectionsPipeline->Bind(11, parameters.mTexCoordBufferUniform);
    mTraceReflectionsPipeline->Bind(12, parameters.mNormalBufferUniform);
    mTraceReflectionsPipeline->Bind(13, parameters.mTangentBufferUniform);
    mTraceReflectionsPipeline->Bind(14, parameters.mMaterialsTextures);

    for (uint32_t mipIndex = 1; mipIndex < mReflectionsBuffer->GetMipLevels(); ++mipIndex) {
        ScopedRefPtr<ComputePipeline>& blurPipeline = mBlurReflectionsPipeline[mipIndex];
        uint32_t sourceMip = mipIndex - 1;
        uint32_t targetMip = mipIndex;
        blurPipeline->Bind(0, mReflectionsBuffer, sourceMip);
        blurPipeline->Bind(1, mReflectionHitDepthBuffer);
        blurPipeline->Bind(2, parameters.mDepthBuffer);
        blurPipeline->Bind(3, parameters.mFrameBufferSampler);
        blurPipeline->Bind(4, mReflectionsBuffer, targetMip);
    }
}

void GlossyReflectionsRenderer::UpdateUniforms(
    const PerFrameParameters& parameters,
    uint32_t frameIndex) {
    mTraceReflectionsPipeline->Bind(frameIndex, 0, parameters.mCameraUniform);
    mTraceReflectionsPipeline->Bind(frameIndex, 1, parameters.mShadowCameraUniform);
    mTraceReflectionsPipeline->Bind(frameIndex, 2, parameters.mPerMeshParameters);

    for (uint32_t mipIndex = 1; mipIndex < mReflectionsBuffer->GetMipLevels(); ++mipIndex) {
        ScopedRefPtr<ComputePipeline>& blurPipeline = mBlurReflectionsPipeline[mipIndex];
        blurPipeline->Bind(frameIndex, 0, parameters.mCameraUniform);
        blurPipeline->Bind(frameIndex, 1, mBlurControlParameters[frameIndex]);
    }

    mBlurControlParameters[frameIndex]->Write(BlurControlData{
        .sigmaSpatial = mSettingsManager->GetSpatialSigma(),
        .sigmaDepth = mSettingsManager->GetDepthSigma()});
}

void GlossyReflectionsRenderer::Render(vk::CommandBuffer commandBuffer, const uint32_t frameIndex) {
    mContext->BeginMarker(commandBuffer, "Reflections");
    {
        mContext->BeginMarker(commandBuffer, "Trace rays");
        {
            {
                std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                    {
                        {mReflectionsBuffer,
                         vk::ImageLayout::eShaderReadOnlyOptimal,
                         vk::ImageLayout::eGeneral},
                        {mReflectionHitDepthBuffer,
                         vk::ImageLayout::eShaderReadOnlyOptimal,
                         vk::ImageLayout::eGeneral},
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
                mTraceReflectionsPipeline->GetPipelineHandle());
            std::vector<vk::DescriptorSet> descriptorSets =
                mTraceReflectionsPipeline->GetDescriptorSets(frameIndex);
            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eRayTracingKHR,
                mTraceReflectionsPipeline->GetPipelineLayout(),
                0,
                descriptorSets,
                nullptr);

            const RaytracingPipeline::RayTracingTablesRef& tableRef =
                mTraceReflectionsPipeline->GetTablesRef();
            commandBuffer.traceRaysKHR(
                tableRef.rayGen,
                tableRef.rayMiss,
                tableRef.rayHit,
                tableRef.callable,
                mReflectionsBuffer->GetWidth(),
                mReflectionsBuffer->GetHeight(),
                1,
                mContext->GetDevice()->GetDispatcher());
        }
        mContext->EndMarker(commandBuffer);

        {
            std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                vk::PipelineStageFlagBits::eComputeShader,
                {
                    {mReflectionHitDepthBuffer,
                     vk::ImageLayout::eGeneral,
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

        mContext->BeginMarker(commandBuffer, "Blur reflections");
        {
            for (uint32_t mipIndex = 1; mipIndex < mReflectionsBuffer->GetMipLevels(); ++mipIndex) {
                {
                    vk::PipelineStageFlags srcStage =
                        (mipIndex == 1) ? vk::PipelineStageFlagBits::eRayTracingShaderKHR
                                        : vk::PipelineStageFlagBits::eComputeShader;
                    std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                        srcStage,
                        vk::PipelineStageFlagBits::eComputeShader |
                            vk::PipelineStageFlagBits::eFragmentShader,
                        {
                            {mReflectionsBuffer,
                             vk::ImageLayout::eGeneral,
                             vk::ImageLayout::eShaderReadOnlyOptimal,
                             int32_t(mipIndex) - 1},
                            {mReflectionsBuffer,
                             vk::ImageLayout::eShaderReadOnlyOptimal,
                             vk::ImageLayout::eGeneral,
                             int32_t(mipIndex)},
                        });

                    commandBuffer.pipelineBarrier(
                        srcStage,
                        vk::PipelineStageFlagBits::eComputeShader |
                            vk::PipelineStageFlagBits::eFragmentShader,
                        vk::DependencyFlags{},
                        {},
                        {},
                        imageBarriers);
                }

                {
                    ScopedRefPtr<ComputePipeline>& blurPipeline =
                        mBlurReflectionsPipeline[mipIndex];

                    commandBuffer.bindPipeline(
                        vk::PipelineBindPoint::eCompute,
                        blurPipeline->GetPipelineHandle());

                    std::vector<vk::DescriptorSet> updateProbeDescriptors =
                        blurPipeline->GetDescriptorSets(frameIndex);

                    commandBuffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eCompute,
                        blurPipeline->GetPipelineLayout(),
                        0,
                        updateProbeDescriptors,
                        nullptr);

                    const uint32_t mipWidth = mReflectionsBuffer->GetWidth() >> mipIndex;
                    const uint32_t mipHeight = mReflectionsBuffer->GetHeight() >> mipIndex;

                    const uint32_t dispatchX = (mipWidth + 7) / 8;
                    const uint32_t dispatchY = (mipHeight + 7) / 8;
                    commandBuffer.dispatch(dispatchX, dispatchY, 1);
                }
            }

            if (mReflectionsBuffer->GetMipLevels() > 1) {
                std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eComputeShader |
                        vk::PipelineStageFlagBits::eFragmentShader,
                    {
                        {mReflectionsBuffer,
                         vk::ImageLayout::eGeneral,
                         vk::ImageLayout::eShaderReadOnlyOptimal,
                         int32_t(mReflectionsBuffer->GetMipLevels()) - 1},
                    });

                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eComputeShader |
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

GlossyReflectionsRenderer::~GlossyReflectionsRenderer() {}

}  // namespace VKRT