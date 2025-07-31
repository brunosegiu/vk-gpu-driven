#pragma once

#include "Camera.h"
#include "CommandRing.h"
#include "ComputePipeline.h"
#include "Context.h"
#include "GraphicsPipeline.h"
#include "RefCountPtr.h"
#include "RenderPass.h"
#include "Scene.h"

namespace VKRT {
class Renderer : public RefCountPtr, public InputEventListener {
public:
    Renderer(ScopedRefPtr<Context> context, ScopedRefPtr<Scene> scene);

    void Render(Camera* camera);

    ~Renderer();

private:
    void UpdateUniforms(Camera* camera, uint32_t imageIndex);

    void OnKeyPressed(int key) override;
    void OnKeyReleased(int key) override;
    void OnMouseMoved(glm::vec2 newPos) override;
    void OnLeftMouseButtonPressed() override;
    void OnLeftMouseButtonReleased() override;
    void OnRightMouseButtonPressed() override;
    void OnRightMouseButtonReleased() override;

    // Shared resources
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<Scene> mScene;
    uint32_t mCurrentFrameIndex;
    ScopedRefPtr<CommandRing> mCommandRing;

    // Base pass resources
    ScopedRefPtr<ShaderParameterCollection> mBasePassParameters;
    ScopedRefPtr<GraphicsPipeline> mBasePassPipeline;
    ScopedRefPtr<VulkanBuffer> mMaterialsBuffer;
    std::vector<ScopedRefPtr<VulkanBuffer>> mPerDrawBuffers;
    ScopedRefPtr<ShaderParameterBuffer> mCameraUniform;
    ScopedRefPtr<ShaderParameterBuffer> mMaterialsUniform;
    ScopedRefPtr<ShaderParameterSampler> mMaterialSampler;
    ScopedRefPtr<ShaderParameterImage> mMaterialsTextures;
    ScopedRefPtr<ShaderParameterImage> mShadowMapUniform;
    ScopedRefPtr<ShaderParameterBuffer> mPerDrawParameters;
    ScopedRefPtr<RenderTarget> mRenderTarget;
    ScopedRefPtr<Texture> mDepthBuffer;
    ScopedRefPtr<RenderTarget> mDepthRenderTarget;
    ScopedRefPtr<RenderPass> mBasePass;

    // Base pass culling
    ScopedRefPtr<ShaderParameterCollection> mBasePassCullingParameters;
    ScopedRefPtr<ShaderParameterBuffer> mIndirectDrawBufferParameter;
    ScopedRefPtr<ComputePipeline> mCullingPipeline;
    std::vector<ScopedRefPtr<VulkanBuffer>> mIndirectDrawBuffers;

    // Shadow pass resources
    ScopedRefPtr<RenderPass> mDepthOnlyPass;
    ScopedRefPtr<RenderTarget> mDepthOnlyPassRenderTarget;
    ScopedRefPtr<Texture> mShadowMap;
    ScopedRefPtr<ShaderParameterCollection> mDepthOnlyParameters;
    ScopedRefPtr<GraphicsPipeline> mDepthOnlyPipeline;
    ScopedRefPtr<ShaderParameterBuffer> mShadowCameraUniform;

    // Shadow pass culling
    ScopedRefPtr<ShaderParameterCollection> mShadowPassCullingParameters;
    ScopedRefPtr<ShaderParameterBuffer> mShadowIndirectDrawBufferParameter;
    ScopedRefPtr<ComputePipeline> mShadowCullingPipeline;
    std::vector<ScopedRefPtr<VulkanBuffer>> mShadowIndirectDrawBuffers;
};

}  // namespace VKRT