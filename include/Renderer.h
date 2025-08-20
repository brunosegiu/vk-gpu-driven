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
        ScopedRefPtr<ShaderParameterBuffer> additionalDrawDataBufferParameter;
        ScopedRefPtr<ShaderParameterBuffer> drawCallCountBufferParameter;
        std::vector<ScopedRefPtr<VulkanBuffer>> indirectDrawBuffers;
        std::vector<ScopedRefPtr<VulkanBuffer>> additionalDrawDataBuffers;
        std::vector<ScopedRefPtr<VulkanBuffer>> drawCallCountBuffer;
        ScopedRefPtr<ComputePipeline> cullingPipeline;
    };
    std::unordered_map<Material::AlphaMode, CullingPipelineResources> mShadowPassCulling;
    std::unordered_map<Material::AlphaMode, CullingPipelineResources> mBasePassCulling;

    struct MaterialDomainPipeline {
        ScopedRefPtr<ShaderParameterCollection> parameters;
        ScopedRefPtr<GraphicsPipeline> pipeline;
    };

    // Shadow pass resources
    ScopedRefPtr<RenderPass> mDepthOnlyPass;
    ScopedRefPtr<RenderTarget> mDepthOnlyPassRenderTarget;
    ScopedRefPtr<Texture> mShadowMap;
    std::unordered_map<Material::AlphaMode, MaterialDomainPipeline> mShadowPassPipeline;

    ScopedRefPtr<ShaderParameterBuffer> mShadowCameraUniform;

    // Geometry pass resources
    ScopedRefPtr<Texture> mVisibilityBuffer;
    ScopedRefPtr<RenderTarget> mVisibilityBufferRT;

    // Base pass resources
    std::unordered_map<Material::AlphaMode, MaterialDomainPipeline> mGeometryPassPipeline;
    ScopedRefPtr<VulkanBuffer> mMaterialsBuffer;
    std::vector<ScopedRefPtr<VulkanBuffer>> mPerDrawBuffers;
    ScopedRefPtr<ShaderParameterBuffer> mCameraUniform;
    ScopedRefPtr<ShaderParameterBuffer> mMaterialsUniform;
    ScopedRefPtr<ShaderParameterSampler> mMaterialSampler;
    ScopedRefPtr<ShaderParameterSampler> mFrameBufferSampler;
    ScopedRefPtr<ShaderParameterImage> mMaterialsTextures;
    ScopedRefPtr<ShaderParameterImage> mShadowMapUniform;
    ScopedRefPtr<ShaderParameterBuffer> mPerDrawParameters;
    ScopedRefPtr<RenderTarget> mMainRenderTarget;
    ScopedRefPtr<Texture> mDepthBuffer;
    ScopedRefPtr<RenderTarget> mDepthRenderTarget;
    ScopedRefPtr<RenderPass> mGeometryPass;

    // Shading pass resources
    ScopedRefPtr<RenderPass> mShadePass;
    ScopedRefPtr<ShaderParameterCollection> mShadePassParameters;
    ScopedRefPtr<ShaderParameterImage> mVisibilityBufferUniform;
    ScopedRefPtr<ShaderParameterImage> mSSAOTextureParameter;
    ScopedRefPtr<ShaderParameterBuffer> mIndexBufferUniform;
    ScopedRefPtr<ShaderParameterBuffer> mPositionBufferUniform;
    ScopedRefPtr<ShaderParameterBuffer> mTexCoordBufferUniform;
    ScopedRefPtr<ShaderParameterBuffer> mNormalBufferUniform;
    ScopedRefPtr<ShaderParameterBuffer> mTangentBufferUniform;
    ScopedRefPtr<GraphicsPipeline> mShadePassPipeline;

    // Tranparency resources
    ScopedRefPtr<RenderPass> mTransparentPass;

    // SSAO resources
    ScopedRefPtr<RenderPass> mSSAOPass;
    ScopedRefPtr<ShaderParameterCollection> mSSAOParameters;
    ScopedRefPtr<ShaderParameterImage> mDepthBufferParameter;
    ScopedRefPtr<ShaderParameterBuffer> mSSAOControlParameter;
    ScopedRefPtr<GraphicsPipeline> mSSAOPipeline;
    ScopedRefPtr<Texture> mSSAOBuffer;
    ScopedRefPtr<RenderTarget> mSSAORenderTarget;

    // SSAO blur resources
    ScopedRefPtr<RenderPass> mSSAOBlurPass;
    ScopedRefPtr<ShaderParameterCollection> mSSAOBlurParameters;
    ScopedRefPtr<ShaderParameterImage> mSSAOBufferParameter;
    ScopedRefPtr<GraphicsPipeline> mSSAOBlurPipeline;
    ScopedRefPtr<Texture> mSSAOBlurredBuffer;
    ScopedRefPtr<RenderTarget> mSSAOBlurredRenderTarget;

    bool mFreezeCulling;

    // UI rendering
    ScopedRefPtr<UIRenderer> mUIRenderer;

    // Probe rendering
    ScopedRefPtr<RaytracingPipeline> mProbeRaytracingPipeline;
    ScopedRefPtr<ShaderParameterAccelerationStructure> mASParamater;
    ScopedRefPtr<ShaderParameterCollection> mProbeRaytracingParameters;

    void BeginMarker(const vk::CommandBuffer& commandBuffer, const std::string& name);
    void EndMarker(const vk::CommandBuffer& commandBuffer);
};
}  // namespace VKRT