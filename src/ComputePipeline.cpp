#include "ComputePipeline.h"

#include <unordered_map>

#include "Context.h"
#include "DebugUtils.h"

namespace VKRT {
ComputePipeline::ComputePipeline(
    ScopedRefPtr<Context> context,
    const ScopedRefPtr<ShaderParameterCollection>& parameters,
    const std::pair<vk::ShaderStageFlagBits, Resource::Id>& shaderResourcesMap)
    : Pipeline(context) {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();

    const vk::ShaderModule shaderModule = LoadShader(shaderResourcesMap.second);
    mShaders.emplace(shaderResourcesMap.first, std::vector<vk::ShaderModule>{shaderModule});
    const vk::PipelineShaderStageCreateInfo stageCreateInfo =
        vk::PipelineShaderStageCreateInfo()
            .setStage(shaderResourcesMap.first)
            .setModule(shaderModule)
            .setPName("main");

    std::vector<vk::PushConstantRange> pushConstants = parameters->GetPushConstants();

    const std::vector<vk::DescriptorSetLayout> layouts = parameters->GetLayouts();
    vk::PipelineLayoutCreateInfo layoutCreateInfo =
        vk::PipelineLayoutCreateInfo().setSetLayouts(layouts).setPushConstantRanges(pushConstants);
    mLayout = VKRT_ASSERT_VK(logicalDevice.createPipelineLayout(layoutCreateInfo));

    const vk::ComputePipelineCreateInfo pipelineCreateInfo =
        vk::ComputePipelineCreateInfo().setStage(stageCreateInfo).setLayout(mLayout);

    mPipeline = VKRT_ASSERT_VK(logicalDevice.createComputePipeline({}, pipelineCreateInfo));
}

ComputePipeline::~ComputePipeline() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    for (auto& entry : mShaders) {
        for (vk::ShaderModule& module : entry.second) {
            logicalDevice.destroyShaderModule(module);
        }
    }
    logicalDevice.destroyPipeline(mPipeline);
    logicalDevice.destroyPipelineLayout(mLayout);
}

}  // namespace VKRT