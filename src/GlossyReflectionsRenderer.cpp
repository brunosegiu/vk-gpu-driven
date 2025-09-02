#include "GlossyReflectionsRenderer.h"

#include <random>

#include "DebugUtils.h"
#include "Utils.h"

namespace VKRT {

GlossyReflectionsRenderer::GlossyReflectionsRenderer(
    ScopedRefPtr<Context> context,
    ScopedRefPtr<Scene> scene,
    ScopedRefPtr<SettingsManager> settingsManager)
    : mContext(context), mScene(scene), mSettingsManager(settingsManager) {}

void GlossyReflectionsRenderer::AddRenderTargets() {
    const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();
    mReflectionsBuffer = new Texture(
        mContext,
        imageSize.width,
        imageSize.height,
        1,
        vk::Format::eB10G11R11UfloatPack32,
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
}

void GlossyReflectionsRenderer::AddResources() {}

void GlossyReflectionsRenderer::RemoveRenderTargets() {
    mReflectionsBuffer = nullptr;
}

void GlossyReflectionsRenderer::RemovePipelines() {}

void GlossyReflectionsRenderer::RemoveResources() {}

void GlossyReflectionsRenderer::UpdatePersistentUniforms(const PersistentParameters& parameters) {
    mTraceReflectionsPipeline->Bind(0, parameters.mScenePersistentDataParameter);
    mTraceReflectionsPipeline->Bind(1, mScene->GetTLAS());
    mTraceReflectionsPipeline->Bind(2, parameters.mVisibilityBuffer);
    mTraceReflectionsPipeline->Bind(3, parameters.mShadowMap);
    mTraceReflectionsPipeline->Bind(4, mReflectionsBuffer);
    mTraceReflectionsPipeline->Bind(5, parameters.mMaterialSampler);
    mTraceReflectionsPipeline->Bind(6, parameters.mFrameBufferSampler);
    mTraceReflectionsPipeline->Bind(7, parameters.mMaterialsUniform);
    mTraceReflectionsPipeline->Bind(8, parameters.mIndexBufferUniform);
    mTraceReflectionsPipeline->Bind(9, parameters.mPositionBufferUniform);
    mTraceReflectionsPipeline->Bind(10, parameters.mTexCoordBufferUniform);
    mTraceReflectionsPipeline->Bind(11, parameters.mNormalBufferUniform);
    mTraceReflectionsPipeline->Bind(12, parameters.mTangentBufferUniform);
    mTraceReflectionsPipeline->Bind(13, parameters.mMaterialsTextures);
}

void GlossyReflectionsRenderer::UpdateUniforms(
    const PerFrameParameters& parameters,
    uint32_t frameIndex) {
    mTraceReflectionsPipeline->Bind(frameIndex, 0, parameters.mCameraUniform);
    mTraceReflectionsPipeline->Bind(frameIndex, 1, parameters.mShadowCameraUniform);
    mTraceReflectionsPipeline->Bind(frameIndex, 2, parameters.mPerMeshParameters);
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

            {
                std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                    vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                    vk::PipelineStageFlagBits::eComputeShader,
                    {
                        {mReflectionsBuffer,
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
    }
    mContext->EndMarker(commandBuffer);
}

GlossyReflectionsRenderer::~GlossyReflectionsRenderer() {}

}  // namespace VKRT