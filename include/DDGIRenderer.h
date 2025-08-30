#pragma once

#include "Camera.h"
#include "CommandRing.h"
#include "ComputePipeline.h"
#include "Context.h"
#include "GraphicsPipeline.h"
#include "RaytracingPipeline.h"
#include "RefCountPtr.h"
#include "RenderPass.h"
#include "Scene.h"
#include "UIRenderer.h"

namespace VKRT {

struct DDGIData {
    glm::uvec3 probeGridCount;
    glm::vec3 probeGridOrigin;
    glm::vec3 probeSpacing;
    float minRayLength;
    float maxRayLength;
    glm::mat3 randomRotation;
    float hysteresis;
    uint32_t frameIndex;
    float probeRadius;
};

class DDGIRenderer : public RefCountPtr {
public:
    DDGIRenderer(
        ScopedRefPtr<Context> context,
        ScopedRefPtr<Scene> scene,
        ScopedRefPtr<SettingsManager> settingsManager);

    void AddRenderTargets(
        const ScopedRefPtr<RenderTarget>& mainRenderTarget,
        const ScopedRefPtr<RenderTarget>& depthRenderTarget);
    void AddPipelines();
    void AddResources();

    void RemoveRenderTargets();
    void RemovePipelines();
    void RemoveResources();

    void Render(vk::CommandBuffer commandBuffer, const uint32_t frameIndex);

    struct PersistentParameters {
        ScopedRefPtr<VulkanBuffer> mScenePersistentDataParameter;
        vk::Sampler mMaterialSampler;
        vk::Sampler mFrameBufferSampler;
        ScopedRefPtr<VulkanBuffer> mMaterialsUniform;
        ScopedRefPtr<VulkanBuffer> mIndexBufferUniform;
        ScopedRefPtr<VulkanBuffer> mPositionBufferUniform;
        ScopedRefPtr<VulkanBuffer> mTexCoordBufferUniform;
        ScopedRefPtr<VulkanBuffer> mNormalBufferUniform;
        ScopedRefPtr<VulkanBuffer> mTangentBufferUniform;
        std::vector<ScopedRefPtr<Texture>> mMaterialsTextures;
    };
    void UpdatePersistentUniforms(const PersistentParameters& parameters);

    struct PerFrameParameters {
        std::vector<ScopedRefPtr<VulkanBuffer>> mCameraUniform;
        std::vector<ScopedRefPtr<VulkanBuffer>> mShadowCameraUniform;
        std::vector<ScopedRefPtr<VulkanBuffer>> mPerMeshParameters;
    };
    void UpdateUniforms(const PerFrameParameters& parameters, uint32_t imageIndex);

    const std::vector<ScopedRefPtr<VulkanBuffer>>& GetProbeData() { return mDDGIProbeData; }

    const ScopedRefPtr<Texture>& GetIrradianceBuffer() {
        return mProbeIrradianceBuffers[mCurrentFrame % mProbeMomentsBuffers.size()];
    }
    const ScopedRefPtr<Texture>& GetMomentsBuffer() {
        return mProbeMomentsBuffers[mCurrentFrame % mProbeMomentsBuffers.size()];
    }

    const ScopedRefPtr<Texture>& GetPreviousIrradianceBuffer() {
        return mProbeIrradianceBuffers[(mCurrentFrame + 1) % mProbeMomentsBuffers.size()];
    }
    const ScopedRefPtr<Texture>& GetPreviousMomentsBuffer() {
        return mProbeMomentsBuffers[(mCurrentFrame + 1) % mProbeMomentsBuffers.size()];
    }

    void RenderProbes(vk::CommandBuffer commandBuffer, const uint32_t frameIndex);

    ~DDGIRenderer();

private:
    // Shared resources
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<Scene> mScene;
    ScopedRefPtr<SettingsManager> mSettingsManager;

    // Probe rendering
    ScopedRefPtr<RaytracingPipeline> mProbeRaytracingPipeline;
    ScopedRefPtr<Texture> mProbeRayRadianceBuffer;
    ScopedRefPtr<Texture> mProbeRayDirectionDepthBuffer;
    ScopedRefPtr<ComputePipeline> mUpdateProbePipeline;
    std::array<ScopedRefPtr<Texture>, 2> mProbeIrradianceBuffers;
    std::array<ScopedRefPtr<Texture>, 2> mProbeMomentsBuffers;
    std::vector<ScopedRefPtr<VulkanBuffer>> mDDGIProbeData;
    DDGIData mDDGIData;
    uint32_t mCurrentFrame;

    // Debug visualization
    ScopedRefPtr<RenderPass> mVisualizeProbesPass;
    ScopedRefPtr<GraphicsPipeline> mVisualizeProbesPipeline;
    ScopedRefPtr<VulkanBuffer> mSpherePositions;
    ScopedRefPtr<VulkanBuffer> mSphereIndices;
};
}  // namespace VKRT