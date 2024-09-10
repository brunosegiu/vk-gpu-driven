#pragma once

#include "Camera.h"
#include "Context.h"
#include "Pipeline.h"
#include "RefCountPtr.h"
#include "Scene.h"
#include "RenderPass.h"
#include "CommandRing.h"

namespace VKRT {
class Renderer : public RefCountPtr, public InputEventListener {
public:
    Renderer(ScopedRefPtr<Context> context, ScopedRefPtr<Scene> scene);

    void Render(Camera* camera);

    ~Renderer();

private:
    struct CameraProperties {
        glm::mat4 viewProjection;
        glm::vec4 cameraForwardDir;
    };
    void UpdateCameraUniforms(Camera* camera, uint32_t imageIndex);
    struct SceneData {
        glm::mat4 transform;
        uint32_t materialId;
        glm::mat3 normalTransform;
    };
    struct PerDrawParameters {
        uint32_t drawId;
    };
    void UpdatePerDrawBuffer(uint32_t imageIndex);
    void UpdateMaterialUniform();

    void OnKeyPressed(int key) override;
    void OnKeyReleased(int key) override;
    void OnMouseMoved(glm::vec2 newPos) override;
    void OnLeftMouseButtonPressed() override;
    void OnLeftMouseButtonReleased() override;
    void OnRightMouseButtonPressed() override;
    void OnRightMouseButtonReleased() override;

    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<Scene> mScene;

    ScopedRefPtr<ShaderParameterCollection> mMainPassParameters;
    ScopedRefPtr<ShaderParameterBuffer> mCameraUniform;
    ScopedRefPtr<ShaderParameterBuffer> mMaterialsUniform;
    ScopedRefPtr<VulkanBuffer> mMaterialsBuffer;
    ScopedRefPtr<ShaderParameterSampler> mMaterialSampler;
    ScopedRefPtr<ShaderParameterImage> mMaterialsTextures;
    ScopedRefPtr<ShaderParameterBuffer> mPerDrawParameters;
    std::vector<ScopedRefPtr<VulkanBuffer>> mPerDrawBuffer;
    ScopedRefPtr<ShaderParameterPushConstant> mPushConstant;
    ScopedRefPtr<Pipeline> mMainPassPipeline;
    ScopedRefPtr<CommandRing> mCommandRing;

    ScopedRefPtr<RenderTarget> mRenderTarget;
    ScopedRefPtr<Texture> mDepthBuffer;
    ScopedRefPtr<RenderTarget> mDepthRenderTarget;
    ScopedRefPtr<RenderPass> mRenderPass;

    uint32_t mCurrentFrameIndex;

};

}  // namespace VKRT