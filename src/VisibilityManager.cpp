#include "VisibilityManager.h"

#include "DebugUtils.h"

namespace VKRT {

VisibilityManager::VisibilityManager(
    ScopedRefPtr<Context> context,
    ScopedRefPtr<Scene> scene,
    Material::AlphaMode alphaMode)
    : mContext(context), mScene(scene), mAlphaMode(alphaMode), mFreezeCulling(false) {
    AddPipelines();
}

void VisibilityManager::AddPipelines() {
    mCullingPipeline = new ComputePipeline(
        mContext,
        {{vk::ShaderStageFlagBits::eCompute, {Resource::Id::CullingShader}}});
}

void VisibilityManager::AddResources() {
    const uint32_t bufferCount = mContext->GetMaxInFlightFrameCount();
    const uint32_t drawCallCount = static_cast<uint32_t>(mScene->GetDrawCallCount(mAlphaMode));

    if (drawCallCount <= 0) {
        return;
    }

    mCullingDataBuffers = mContext->GetDevice()->CreateBuffers(
        mContext->GetMaxInFlightFrameCount(),
        sizeof(CullData),
        vk::BufferUsageFlagBits::eUniformBuffer,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    mIndirectDrawBuffers = mContext->GetDevice()->CreateBuffers(
        bufferCount,
        drawCallCount * sizeof(vk::DrawIndexedIndirectCommand),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

    mAdditionalDrawDataBuffers = mContext->GetDevice()->CreateBuffers(
        bufferCount,
        drawCallCount * sizeof(uint32_t),
        vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

    mDrawCallCountBuffer = mContext->GetDevice()->CreateBuffers(
        bufferCount,
        sizeof(uint32_t),
        vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eTransferDst,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
}

void VisibilityManager::UpdatePersistentUniforms(
    const ScopedRefPtr<VulkanBuffer>& scenePersistentDataBuffer) {
    mCullingPipeline->Bind(ParameterUpdateFrequency::Once, 0, scenePersistentDataBuffer);
}

void VisibilityManager::UpdateUniforms(
    const CullData& cullData,
    uint32_t frameIndex,
    const std::vector<ScopedRefPtr<VulkanBuffer>>& meshDataBuffer) {

    const uint32_t drawCallCount = static_cast<uint32_t>(mScene->GetDrawCallCount(mAlphaMode));
    if (drawCallCount <= 0) {
        return;
    }

    mCullingPipeline->Bind(ParameterUpdateFrequency::PerFrame, 0, mCullingDataBuffers);
    mCullingPipeline->Bind(ParameterUpdateFrequency::PerFrame, 1, meshDataBuffer);
    mCullingPipeline->Bind(ParameterUpdateFrequency::PerFrame, 2, mIndirectDrawBuffers);
    mCullingPipeline->Bind(ParameterUpdateFrequency::PerFrame, 3, mDrawCallCountBuffer);
    mCullingPipeline->Bind(ParameterUpdateFrequency::PerFrame, 4, mAdditionalDrawDataBuffers);

    if (!mFreezeCulling) {
        mCullingDataBuffers[frameIndex]->Write(cullData);
    }
}

void VisibilityManager::Dispatch(vk::CommandBuffer commandBuffer, uint32_t frameIndex) {
    const uint32_t maxDrawCallCount = mScene->GetDrawCallCount(mAlphaMode);
    if (maxDrawCallCount == 0) {
        return;
    }
    mContext->BeginMarker(commandBuffer, AlphaModeToStr(mAlphaMode));
    {
        ScopedRefPtr<VulkanBuffer> drawCallCountBuffer = mDrawCallCountBuffer[frameIndex];
        ScopedRefPtr<VulkanBuffer> indirectDrawBuffer = mIndirectDrawBuffers[frameIndex];
        ScopedRefPtr<VulkanBuffer> additionalDrawDataBuffer =
            mAdditionalDrawDataBuffers[frameIndex];
        // Write 0 in drawCallCountBuffer
        {
            std::vector<vk::BufferMemoryBarrier> bufferBarriers = VulkanBuffer::GetBarriers(
                {drawCallCountBuffer},
                vk::PipelineStageFlagBits::eDrawIndirect,
                vk::PipelineStageFlagBits::eTransfer);

            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eDrawIndirect,
                vk::PipelineStageFlagBits::eTransfer,
                vk::DependencyFlags{},
                {},
                bufferBarriers,
                {});

            commandBuffer
                .fillBuffer(drawCallCountBuffer->GetBufferHandle(), 0, sizeof(uint32_t), 0);
        }

        {
            std::vector<vk::BufferMemoryBarrier> bufferBarriers = VulkanBuffer::GetBarriers(
                {drawCallCountBuffer},
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eComputeShader);

            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eComputeShader,
                vk::DependencyFlags{},
                {},
                bufferBarriers,
                {});
        }

        {
            std::vector<vk::BufferMemoryBarrier> bufferBarriers = VulkanBuffer::GetBarriers(
                {indirectDrawBuffer, additionalDrawDataBuffer},
                vk::PipelineStageFlagBits::eDrawIndirect,
                vk::PipelineStageFlagBits::eComputeShader);

            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eDrawIndirect,
                vk::PipelineStageFlagBits::eComputeShader,
                vk::DependencyFlags{},
                {},
                bufferBarriers,
                {});
        }

        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            mCullingPipeline->GetPipelineHandle());

        std::vector<vk::DescriptorSet> cullingDescriptorSets =
            mCullingPipeline->GetDescriptorSets(frameIndex);

        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            mCullingPipeline->GetPipelineLayout(),
            0,
            cullingDescriptorSets,
            nullptr);

        commandBuffer.dispatch((maxDrawCallCount / 64) + 1, 1, 1);

        // TODO: Wait until before actual draws are dispatched, this is too early
        {
            std::vector<vk::BufferMemoryBarrier> bufferBarriers = VulkanBuffer::GetBarriers(
                {indirectDrawBuffer, additionalDrawDataBuffer, drawCallCountBuffer},
                vk::PipelineStageFlagBits::eComputeShader,
                vk::PipelineStageFlagBits::eComputeShader);

            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader,
                vk::PipelineStageFlagBits::eAllGraphics,
                vk::DependencyFlags{},
                {},
                bufferBarriers,
                {});
        }
    }
    mContext->EndMarker(commandBuffer);
}

VisibilityManager::~VisibilityManager() {}

}  // namespace VKRT
