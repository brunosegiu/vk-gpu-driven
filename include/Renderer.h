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
    struct CameraProperties {
        glm::mat4 viewProjection;
        glm::vec4 cameraForwardDir;
        std::array<glm::vec4, 6> frustumPlanes;
        uint32_t maxDrawCount;
    };
    void UpdateCameraUniforms(Camera* camera, uint32_t imageIndex);
    struct DrawData {
        uint32_t indexCount;
        uint32_t firstIndex;
        int32_t vertexOffset;
        glm::mat4 transform;
        uint32_t materialId;
        glm::mat3 normalTransform;
        struct {
            glm::vec3 minBounds;
            glm::vec3 maxBounds;
        } aabb;
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
    std::vector<ScopedRefPtr<VulkanBuffer>> mPerDrawBuffers;
    ScopedRefPtr<GraphicsPipeline> mMainPassPipeline;
    ScopedRefPtr<CommandRing> mCommandRing;

    ScopedRefPtr<ShaderParameterCollection> mCullingParameters;
    ScopedRefPtr<ShaderParameterBuffer> mIndirectDrawBufferParameter;
    ScopedRefPtr<ComputePipeline> mCullingPipeline;
    std::vector<ScopedRefPtr<VulkanBuffer>> mIndirectDrawBuffers;

    ScopedRefPtr<RenderTarget> mRenderTarget;
    ScopedRefPtr<Texture> mDepthBuffer;
    ScopedRefPtr<RenderTarget> mDepthRenderTarget;
    ScopedRefPtr<RenderPass> mRenderPass;

    uint32_t mCurrentFrameIndex;
};

}  // namespace VKRT