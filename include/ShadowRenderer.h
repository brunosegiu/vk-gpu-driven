#pragma once

#include "Context.h"
#include "GraphicsPipeline.h"
#include "RaytracingPipeline.h"
#include "RefCountPtr.h"
#include "RenderPass.h"
#include "Scene.h"
#include "VisibilityManager.h"
#include "SettingsManager.h"

namespace VKRT {

class ShadowRenderer : public RefCountPtr {
public:
    ShadowRenderer(
        ScopedRefPtr<Context> context,
        ScopedRefPtr<Scene> scene,
        ScopedRefPtr<SettingsManager> settingsManager);

    void AddRenderTargets();
    void AddPipelines();
    void AddResources();

    struct PersistentParameters {
        ScopedRefPtr<VulkanBuffer> scenePersistentDataBuffer;
        vk::Sampler materialSampler;
        ScopedRefPtr<VulkanBuffer> materialUniform;
        std::vector<ScopedRefPtr<Texture>> sceneTextures;
    };
    void UpdatePersistentUniforms(const PersistentParameters& paramaters);

    struct PerFrameParameters {
        std::vector<ScopedRefPtr<VulkanBuffer>> meshDataBuffer;
    };
    void UpdateUniforms(const uint32_t frameIndex, const PerFrameParameters& parameters);

    const std::vector<ScopedRefPtr<VulkanBuffer>>& GetShadowUniform() {
        return mShadowCameraUniform;
    }

    const ScopedRefPtr<Texture>& GetShadowMap() { return mShadowMap; }

    void Render(vk::CommandBuffer commandBuffer, const uint32_t frameIndex);

    ~ShadowRenderer();

private:
    // Shared resources
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<Scene> mScene;
    ScopedRefPtr<SettingsManager> mSettingsManager;

    // Shadow pass resources
    ScopedRefPtr<RenderPass> mDepthOnlyPass;
    ScopedRefPtr<RenderTarget> mDepthOnlyPassRenderTarget;
    ScopedRefPtr<Texture> mShadowMap;
    std::unordered_map<Material::AlphaMode, ScopedRefPtr<GraphicsPipeline>> mShadowPassPipelines;
    ScopedRefPtr<GraphicsPipeline> mShadowPassAlphaMaskedPipeline;
    std::vector<ScopedRefPtr<VulkanBuffer>> mShadowCameraUniform;
    std::unordered_map<Material::AlphaMode, ScopedRefPtr<VisibilityManager>> mVisibilityManagers;
    vk::Sampler mShadowSampler;
};
}  // namespace VKRT