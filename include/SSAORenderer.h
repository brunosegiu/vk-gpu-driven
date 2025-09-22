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

class SSAORenderer : public RefCountPtr {
public:
    SSAORenderer(
        ScopedRefPtr<Context> context,
        ScopedRefPtr<Scene> scene,
        ScopedRefPtr<SettingsManager> settingsManager);

    void Render(
        vk::CommandBuffer commandBuffer,
        const uint32_t frameIndex,
        const ScopedRefPtr<Texture>& depthBuffer);

    void AddRenderTargets();
    void AddPipelines();
    void AddResources();

    void RemoveRenderTargets();
    void RemovePipelines();
    void RemoveResources();

    struct PersistentParameters {
        vk::Sampler mFrameBufferSampler;
        ScopedRefPtr<Texture> mDepthBuffer;
    };
    void UpdatePersistentUniforms(const PersistentParameters& parameters);

    struct PerFrameParameters {
        std::vector<ScopedRefPtr<VulkanBuffer>> mCameraUniform;
    };
    void UpdateUniforms(const PerFrameParameters& parameters, uint32_t frameIndex);

    const ScopedRefPtr<Texture>& GetSSAOBuffer() { return mSSAOBlurredBuffer; }

    ~SSAORenderer();

private:
    // Shared resources
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<SettingsManager> mSettingsManager;

    // SSAO resources
    ScopedRefPtr<RenderPass> mSSAOPass;
    std::vector<ScopedRefPtr<VulkanBuffer>> mSSAOControlBuffer;
    ScopedRefPtr<GraphicsPipeline> mSSAOPipeline;
    ScopedRefPtr<Texture> mSSAOBuffer;
    ScopedRefPtr<RenderTarget> mSSAORenderTarget;

    // SSAO blur resources
    ScopedRefPtr<RenderPass> mSSAOBlurPass;
    ScopedRefPtr<GraphicsPipeline> mSSAOBlurPipeline;
    ScopedRefPtr<Texture> mSSAOBlurredBuffer;
    ScopedRefPtr<RenderTarget> mSSAOBlurredRenderTarget;
};
}  // namespace VKRT