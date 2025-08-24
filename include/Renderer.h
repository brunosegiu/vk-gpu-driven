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
    bool mFreezeCulling;
    bool mHasResouces;
    bool mHasBoundResources;

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
    ScopedRefPtr<VulkanBuffer> mScenePersistentDataBuffer;
    std::vector<ScopedRefPtr<VulkanBuffer>> mPerMeshBuffers;
    ScopedRefPtr<ShaderParameterBuffer> mCameraUniform;
    ScopedRefPtr<ShaderParameterBuffer> mMaterialsUniform;
    ScopedRefPtr<ShaderParameterImage> mReadOnlyProbeIrradianceParameter;
    ScopedRefPtr<ShaderParameterImage> mReadOnlyProbeDepthParameter;
    ScopedRefPtr<ShaderParameterSampler> mMaterialSampler;
    ScopedRefPtr<ShaderParameterSampler> mFrameBufferSampler;
    ScopedRefPtr<ShaderParameterSampler> mIrradianceSampler;
    ScopedRefPtr<ShaderParameterImage> mMaterialsTextures;
    ScopedRefPtr<ShaderParameterImage> mShadowMapUniform;
    ScopedRefPtr<ShaderParameterBuffer> mScenePersistentDataParameter;
    ScopedRefPtr<ShaderParameterBuffer> mPerMeshParameters;
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

    // UI rendering
    ScopedRefPtr<UIRenderer> mUIRenderer;

    // Probe rendering
    ScopedRefPtr<RaytracingPipeline> mProbeRaytracingPipeline;
    ScopedRefPtr<ShaderParameterBuffer> mDDGIProbeDataParameter;
    ScopedRefPtr<ShaderParameterAccelerationStructure> mASParamater;
    ScopedRefPtr<ShaderParameterImage> mWriteProbeRayRadianceParameter;
    ScopedRefPtr<ShaderParameterImage> mWriteProbeRayDirectionDepthParameter;
    ScopedRefPtr<ShaderParameterCollection> mProbeRaytracingParameters;
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


    void BeginMarker(const vk::CommandBuffer& commandBuffer, const std::string& name);
    void EndMarker(const vk::CommandBuffer& commandBuffer);
};
}  // namespace VKRT