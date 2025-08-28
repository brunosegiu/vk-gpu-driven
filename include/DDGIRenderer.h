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
    struct Parameters {
        std::vector<ScopedRefPtr<VulkanBuffer>> mCameraUniform;
        std::vector<ScopedRefPtr<VulkanBuffer>> mShadowCameraUniform;
        std::vector<ScopedRefPtr<VulkanBuffer>> mPerMeshParameters;
        std::vector<ScopedRefPtr<VulkanBuffer>> mDDGIProbeDataParameter;

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
    DDGIRenderer(ScopedRefPtr<Context> context, ScopedRefPtr<Scene> scene, Parameters parameters);

    void Render(Camera* camera, vk::CommandBuffer commandBuffer, const uint32_t frameIndex);

    void UpdatePersistentUniforms();
    void UpdateUniforms(Camera* camera, uint32_t imageIndex);

    ScopedRefPtr<Texture> GetIrradianceBuffer() { return mProbeIrradianceBuffer; }
    ScopedRefPtr<Texture> GetDepthBuffer() { return mProbeDepthBuffer; }

    ~DDGIRenderer();

private:
    void AddRenderTargets();
    void AddPipelines();
    void AddResources();

    void RemoveRenderTargets();
    void RemovePipelines();
    void RemoveResources();

    // Shared resources
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<Scene> mScene;
    Parameters mParameters;

    // Probe rendering
    ScopedRefPtr<RaytracingPipeline> mProbeRaytracingPipeline;
    ScopedRefPtr<Texture> mProbeRayRadianceBuffer;
    ScopedRefPtr<Texture> mProbeRayDirectionDepthBuffer;

    ScopedRefPtr<ComputePipeline> mUpdateProbePipeline;
    ScopedRefPtr<ShaderParameterCollection> mUpdateProbeParameters;
    ScopedRefPtr<ShaderParameterImage> mWriteProbeIrradianceParameter;
    ScopedRefPtr<ShaderParameterImage> mWriteProbeDepthParameter;
    ScopedRefPtr<ShaderParameterImage> mReadOnlyProbeRadianceParameter;
    ScopedRefPtr<ShaderParameterImage> mReadOnlyProbeDirectionDepthParameter;
    ScopedRefPtr<ShaderParameterImage> mProbeRadianceWriteParam;
    ScopedRefPtr<ShaderParameterImage> mProbeDepthDirectionWriteParam;
    ScopedRefPtr<Texture> mProbeIrradianceBuffer;
    ScopedRefPtr<Texture> mProbeDepthBuffer;

    ScopedRefPtr<ShaderParameterBuffer> mDDGIProbeDataParameter;
    ScopedRefPtr<ShaderParameterSampler> mFrameBufferSampler;
};
}  // namespace VKRT