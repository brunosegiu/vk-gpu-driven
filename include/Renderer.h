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
        glm::mat4 view;
        glm::mat4 projection;
    };
    void UpdateCameraUniforms(Camera* camera, uint32_t imageIndex);

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
    ScopedRefPtr<ShaderParameterSampler> mMaterialSampler;
    ScopedRefPtr<Pipeline> mMainPassPipeline;
    ScopedRefPtr<CommandRing> mCommandRing;

    ScopedRefPtr<RenderTarget> mRenderTarget;
    ScopedRefPtr<Texture> mDepthBuffer;
    ScopedRefPtr<RenderTarget> mDepthRenderTarget;
    ScopedRefPtr<RenderPass> mRenderPass;

};

}  // namespace VKRT