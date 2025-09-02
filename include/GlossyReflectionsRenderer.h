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

class GlossyReflectionsRenderer : public RefCountPtr {
public:
    GlossyReflectionsRenderer(
        ScopedRefPtr<Context> context,
        ScopedRefPtr<Scene> scene,
        ScopedRefPtr<SettingsManager> settingsManager);

    void AddRenderTargets();
    void AddPipelines();
    void AddResources();

    void RemoveRenderTargets();
    void RemovePipelines();
    void RemoveResources();

    void Render(vk::CommandBuffer commandBuffer, const uint32_t frameIndex);

    struct PersistentParameters {
        ScopedRefPtr<VulkanBuffer> mScenePersistentDataParameter;
        ScopedRefPtr<Texture> mVisibilityBuffer;
        ScopedRefPtr<Texture> mShadowMap;
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
        ScopedRefPtr<VulkanBuffer> mCameraUniform;
        ScopedRefPtr<VulkanBuffer> mShadowCameraUniform;
        ScopedRefPtr<VulkanBuffer> mPerMeshParameters;
    };
    void UpdateUniforms(const PerFrameParameters& parameters, uint32_t imageIndex);

    const ScopedRefPtr<Texture>& GetReflectionsTexture() { return mReflectionsBuffer; }

    ~GlossyReflectionsRenderer();

private:
    // Shared resources
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<Scene> mScene;
    ScopedRefPtr<SettingsManager> mSettingsManager;

    // Probe rendering
    ScopedRefPtr<RaytracingPipeline> mTraceReflectionsPipeline;
    ScopedRefPtr<Texture> mReflectionsBuffer;
};
}  // namespace VKRT