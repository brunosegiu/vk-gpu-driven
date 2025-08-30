#pragma once

#include "Camera.h"
#include "CommandRing.h"
#include "ComputePipeline.h"
#include "Context.h"
#include "RefCountPtr.h"
#include "Scene.h"

namespace VKRT {

class VisibilityManager : public RefCountPtr {
public:
    struct CullData {
        uint32_t ortho;
        glm::vec3 viewDirectionOrCameraPos;
        std::array<glm::vec4, 6> frustumPlanes;
        uint32_t globalDrawOffset;
        uint32_t maxDrawCount;
    };

    VisibilityManager(
        ScopedRefPtr<Context> context,
        ScopedRefPtr<Scene> scene,
        Material::AlphaMode alphaMode);

    void AddPipelines();
    void AddResources();

    void UpdatePersistentUniforms(const ScopedRefPtr<VulkanBuffer>& scenePersistentDataBuffer);
    void UpdateUniforms(
        const CullData& cullData,
        uint32_t frameIndex,
        const std::vector<ScopedRefPtr<VulkanBuffer>>& meshDataBuffer);

    ScopedRefPtr<VulkanBuffer> GetIndirectDrawBuffer(const uint32_t frameIndex) {
        return mIndirectDrawBuffers[frameIndex];
    }

    ScopedRefPtr<VulkanBuffer> GetIndirectDrawCountBuffer(const uint32_t frameIndex) {
        return mDrawCallCountBuffer[frameIndex];
    }

    ScopedRefPtr<VulkanBuffer> GetAdditionalDrawDataBuffer(const uint32_t frameIndex) {
        return mAdditionalDrawDataBuffers[frameIndex];
    }

    const std::vector<ScopedRefPtr<VulkanBuffer>>& GetAdditionalDrawDataBuffers() {
        return mAdditionalDrawDataBuffers;
    }

    void Dispatch(vk::CommandBuffer commandBuffer, uint32_t frameIndex);

    ~VisibilityManager();

private:
    // Shared resources
    ScopedRefPtr<Context> mContext;
    ScopedRefPtr<Scene> mScene;
    Material::AlphaMode mAlphaMode;

    ScopedRefPtr<ComputePipeline> mCullingPipeline;
    std::vector<ScopedRefPtr<VulkanBuffer>> mCullingDataBuffers;
    std::vector<ScopedRefPtr<VulkanBuffer>> mIndirectDrawBuffers;
    std::vector<ScopedRefPtr<VulkanBuffer>> mAdditionalDrawDataBuffers;
    std::vector<ScopedRefPtr<VulkanBuffer>> mDrawCallCountBuffer;
};
}  // namespace VKRT