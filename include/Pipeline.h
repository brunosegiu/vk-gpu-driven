
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Macros.h"
#include "RefCountPtr.h"
#include "RenderPass.h"
#include "ResourceLoader.h"
#include "ShaderParameterCollection.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"

namespace VKRT {

class Context;

class Pipeline : public RefCountPtr {
public:
    Pipeline(
        ScopedRefPtr<Context> context,
        const ScopedRefPtr<ShaderParameterCollection>& parameters,
        const std::unordered_map<vk::ShaderStageFlagBits, Resource::Id>& shaderResourcesMap,
        ScopedRefPtr<RenderPass> renderPass);

    const vk::PipelineLayout& GetPipelineLayout() const { return mLayout; }
    const vk::Pipeline& GetPipelineHandle() const { return mPipeline; }

    ~Pipeline();

private:
    vk::ShaderModule LoadShader(Resource::Id shaderId);

    ScopedRefPtr<Context> mContext;
    vk::PipelineLayout mLayout;
    vk::Pipeline mPipeline;
    std::unordered_map<vk::ShaderStageFlagBits, vk::ShaderModule> mShaders;
};
}  // namespace VKRT