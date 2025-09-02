#pragma once

#include "Camera.h"
#include "CommandRing.h"
#include "ComputePipeline.h"
#include "Context.h"
#include "DDGIRenderer.h"
#include "GlossyReflectionsRenderer.h"
#include "GraphicsPipeline.h"
#include "PostProcessingRenderer.h"
#include "RefCountPtr.h"
#include "RenderPass.h"
#include "Scene.h"
#include "SettingsManager.h"
#include "ShadowRenderer.h"
#include "UIRenderer.h"
#include "VisibilityManager.h"

namespace VKRT {
class Renderer : public RefCountPtr, public InputEventListener {
public:
    Renderer(
        ScopedRefPtr<Context> context,
        ScopedRefPtr<Scene> scene,
        ScopedRefPtr<SettingsManager> settingsManager);

    void Render(Camera* camera);

    ~Renderer();

private:
    void UpdatePersistentUniforms();
    void UpdateUniforms(Camera* camera, uint32_t imageIndex);

    void AddRenderTargets();
    void AddPipelines();
    void AddResources();

    void RemoveRenderTargets();
    void RemovePipelines();
    void RemoveResources();

    void OnKeyPressed(int key) override;
    void OnKeyReleased(int key) override;
    void OnMouseMoved(glm::vec2 newPos) override;
    void OnLeftMouseButtonPressed() override;
    void OnLeftMouseButtonReleased() override;
    void OnRightMouseButtonPressed() override;
    void OnRightMouseButtonReleased() override;

    // Renderer state flags
    bool mHasResouces;
    bool mHasBoundResources;

    // Shared resources
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<Scene> mScene;
    ScopedRefPtr<SettingsManager> mSettingsManager;
    uint32_t mCurrentFrameIndex;
    ScopedRefPtr<CommandRing> mCommandRing;

    // Culling
    std::unordered_map<Material::AlphaMode, ScopedRefPtr<VisibilityManager>> mVisibilityManagers;

    // Geometry pass resources
    ScopedRefPtr<Texture> mVisibilityBuffer;
    ScopedRefPtr<RenderTarget> mVisibilityBufferRT;
    std::unordered_map<Material::AlphaMode, ScopedRefPtr<GraphicsPipeline>> mGeometryPassPipelines;
    ScopedRefPtr<VulkanBuffer> mMaterialsUniform;
    ScopedRefPtr<VulkanBuffer> mScenePersistentDataBuffer;
    std::vector<ScopedRefPtr<VulkanBuffer>> mPerMeshBuffers;
    std::vector<ScopedRefPtr<VulkanBuffer>> mCameraUniform;
    vk::Sampler mMaterialSampler;
    vk::Sampler mFrameBufferSampler;
    vk::Sampler mIrradianceSampler;
    ScopedRefPtr<RenderTarget> mMainRenderTarget;
    ScopedRefPtr<Texture> mDepthBuffer;
    ScopedRefPtr<RenderTarget> mDepthRenderTarget;
    ScopedRefPtr<RenderPass> mGeometryPass;
    std::vector<ScopedRefPtr<Texture>> mSceneTextures;

    // Shading pass resources
    ScopedRefPtr<RenderPass> mShadePass;
    ScopedRefPtr<GraphicsPipeline> mShadePassPipeline;
    ScopedRefPtr<RenderPass> mTransparentPass;

    ScopedRefPtr<UIRenderer> mUIRenderer;
    ScopedRefPtr<DDGIRenderer> mDDGIRenderer;
    ScopedRefPtr<ShadowRenderer> mShadowRenderer;
    ScopedRefPtr<PostProcessingRenderer> mPostProcessingRenderer;
    ScopedRefPtr<GlossyReflectionsRenderer> mReflectionsRenderer;
};
}  // namespace VKRT