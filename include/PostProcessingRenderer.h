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

class PostProcessingRenderer : public RefCountPtr {
public:
    PostProcessingRenderer(
        ScopedRefPtr<Context> context,
        ScopedRefPtr<Scene> scene,
        ScopedRefPtr<SettingsManager> settingsManager);

    void Render(vk::CommandBuffer commandBuffer, const uint32_t frameIndex);

    void AddRenderTargets(ScopedRefPtr<RenderTarget> dstTarget);
    void AddPipelines();
    void AddResources();

    void RemoveRenderTargets();
    void RemovePipelines();
    void RemoveResources();

    struct PersistentParameters {
        vk::Sampler mFrameBufferSampler;
        ScopedRefPtr<Texture> mScreenTexture;
    };
    void UpdatePersistentUniforms(const PersistentParameters& parameters);
    void UpdateUniforms(uint32_t frameIndex);

    ~PostProcessingRenderer();

private:
    // Shared resources
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<SettingsManager> mSettingsManager;

    // SSAO resources
    ScopedRefPtr<RenderPass> mPostProcessingPass;
    ScopedRefPtr<GraphicsPipeline> mPostProcessingPipeline;
    std::vector<ScopedRefPtr<VulkanBuffer>> mPostProcessingControlBuffer;
};
}  // namespace VKRT