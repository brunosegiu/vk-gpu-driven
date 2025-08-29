#include "ComputePipeline.h"

#include <unordered_map>

#include "Context.h"
#include "DebugUtils.h"

namespace VKRT {
ComputePipeline::ComputePipeline(
    ScopedRefPtr<Context> context,
    const std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>>&
        shaderResourcesMap)
    : Pipeline(context) {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();

    LoadShaders(shaderResourcesMap);

    const vk::PipelineShaderStageCreateInfo stageCreateInfo =
        vk::PipelineShaderStageCreateInfo()
            .setStage(vk::ShaderStageFlagBits::eCompute)
            .setModule(mShaders.begin()->second.front())
            .setPName("main");

    const std::vector<vk::DescriptorSetLayout> layouts = GetDescriptorLayouts();
    vk::PipelineLayoutCreateInfo layoutCreateInfo =
        vk::PipelineLayoutCreateInfo().setSetLayouts(layouts);
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