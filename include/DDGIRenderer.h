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
};

class DDGIRenderer : public RefCountPtr {
public:
    DDGIRenderer(ScopedRefPtr<Context> context, ScopedRefPtr<Scene> scene);

    void AddRenderTargets();
    void AddPipelines();
    void AddResources();

    void RemoveRenderTargets();
    void RemovePipelines();
    void RemoveResources();

    void Render(Camera* camera, vk::CommandBuffer commandBuffer, const uint32_t frameIndex);

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

    ScopedRefPtr<Texture> GetIrradianceBuffer() { return mProbeIrradianceBuffer; }
    ScopedRefPtr<Texture> GetDepthBuffer() { return mProbeDepthBuffer; }

    ~DDGIRenderer();

private:
    // Shared resources
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<Scene> mScene;

    // Probe rendering
    ScopedRefPtr<RaytracingPipeline> mProbeRaytracingPipeline;
    ScopedRefPtr<Texture> mProbeRayRadianceBuffer;
    ScopedRefPtr<Texture> mProbeRayDirectionDepthBuffer;

    ScopedRefPtr<ComputePipeline> mUpdateProbePipeline;
    ScopedRefPtr<Texture> mProbeIrradianceBuffer;
    ScopedRefPtr<Texture> mProbeDepthBuffer;

    std::vector<ScopedRefPtr<VulkanBuffer>> mDDGIProbeData;
};
}  // namespace VKRT