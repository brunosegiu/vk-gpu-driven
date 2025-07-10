#include "Pipeline.h"

#include <unordered_map>

#include "Context.h"
#include "DebugUtils.h"

namespace VKRT {
Pipeline::Pipeline(ScopedRefPtr<Context> context) : mContext(context) {}

vk::ShaderModule Pipeline::LoadShader(Resource::Id shaderId) {
    Resource shaderResource = ResourceLoader::Load(shaderId);
    vk::ShaderModuleCreateInfo shaderCreateInfo =
        vk::ShaderModuleCreateInfo()
            .setCodeSize(shaderResource.size * sizeof(uint8_t))
            .setPCode(reinterpret_cast<const uint32_t*>(shaderResource.buffer));
    vk::ShaderModule shaderModule = VKRT_ASSERT_VK(
        mContext->GetDevice()->GetLogicalDevice().createShaderModule(shaderCreateInfo));
    ResourceLoader::CleanUp(shaderResource);
    return shaderModule;
}

Pipeline::~Pipeline() {}

}  // namespace VKRT