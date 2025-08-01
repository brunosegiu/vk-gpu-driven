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

    // Culling resources
    struct CullingPipelineResources {
        ScopedRefPtr<ShaderParameterCollection> cullingParameters;
        ScopedRefPtr<ShaderParameterBuffer> cullingDataUniform;
        ScopedRefPtr<ShaderParameterBuffer> indirectDrawBufferParameter;
        std::vector<ScopedRefPtr<VulkanBuffer>> indirectDrawBuffers;
        ScopedRefPtr<ComputePipeline> cullingPipeline;

        ScopedRefPtr<ShaderParameterCollection> compactionParameters;
        ScopedRefPtr<ShaderParameterBuffer> compactIndirectDrawBufferParameter;
        ScopedRefPtr<ShaderParameterBuffer> additionalDrawDataBufferParameter;
        ScopedRefPtr<ShaderParameterBuffer> drawCallCountBufferParameter;
        std::vector<ScopedRefPtr<VulkanBuffer>> compactIndirectDrawBuffers;
        std::vector<ScopedRefPtr<VulkanBuffer>> additionalDrawDataBuffers;
        std::vector<ScopedRefPtr<VulkanBuffer>> drawCallCountBuffer;
        ScopedRefPtr<ComputePipeline> compactionPipeline;
    };
    CullingPipelineResources mShadowPassCulling;
    CullingPipelineResources mBasePassCulling;

    // Shadow pass resources
    ScopedRefPtr<RenderPass> mDepthOnlyPass;
    ScopedRefPtr<RenderTarget> mDepthOnlyPassRenderTarget;
    ScopedRefPtr<Texture> mShadowMap;
    ScopedRefPtr<ShaderParameterCollection> mDepthOnlyParameters;
    ScopedRefPtr<GraphicsPipeline> mDepthOnlyPipeline;
    ScopedRefPtr<ShaderParameterBuffer> mShadowCameraUniform;

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
};

}  // namespace VKRT