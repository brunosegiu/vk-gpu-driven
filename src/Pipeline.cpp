#include "Pipeline.h"

#include <unordered_map>

#include "Context.h"
#include "DebugUtils.h"

namespace VKRT {
Pipeline::Pipeline(
    ScopedRefPtr<Context> context,
    const ScopedRefPtr<ShaderParameterCollection>& parameters,
    const std::unordered_map<vk::ShaderStageFlagBits, Resource::Id>& shaderResourcesMap,
    ScopedRefPtr<RenderPass> renderPass)
    : mContext(context) {
    const vk::VertexInputBindingDescription vertexInputBinding =
        vk::VertexInputBindingDescription()
            .setBinding(0)
            .setStride(sizeof(glm::vec3))
            .setInputRate(vk::VertexInputRate::eVertex);

    const vk::VertexInputAttributeDescription attributeDescription =
        vk::VertexInputAttributeDescription().setBinding(0).setLocation(0).setFormat(
            vk::Format::eR32G32B32Sfloat);

    vk::PipelineVertexInputStateCreateInfo vertexInputCreateInfo =
        vk::PipelineVertexInputStateCreateInfo()
            .setVertexBindingDescriptions(vertexInputBinding)
            .setVertexAttributeDescriptions(attributeDescription);

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo =
        vk::PipelineInputAssemblyStateCreateInfo()
            .setTopology(vk::PrimitiveTopology::eTriangleList)
            .setPrimitiveRestartEnable(false);

    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();

    std::vector<vk::PipelineShaderStageCreateInfo> stageCreateInfos;
    mShaders = std::unordered_map<vk::ShaderStageFlagBits, vk::ShaderModule>{};
    for (const auto& entry : shaderResourcesMap) {
        const vk::ShaderModule shaderModule = LoadShader(entry.second);
        mShaders.emplace(entry.first, shaderModule);
        const vk::PipelineShaderStageCreateInfo stageCreateInfo =
            vk::PipelineShaderStageCreateInfo()
                .setStage(entry.first)
                .setModule(shaderModule)
                .setPName("main");
        stageCreateInfos.emplace_back(stageCreateInfo);
    }

    std::vector<vk::PushConstantRange> pushConstants = parameters->GetPushConstants();

    const std::vector<vk::DescriptorSetLayout> layouts = parameters->GetLayouts();
    vk::PipelineLayoutCreateInfo layoutCreateInfo =
        vk::PipelineLayoutCreateInfo().setSetLayouts(layouts).setPushConstantRanges(pushConstants);
    mLayout = VKRT_ASSERT_VK(logicalDevice.createPipelineLayout(layoutCreateInfo));

    // Rasterization setup
    const vk::PipelineRasterizationStateCreateInfo rasterizerCreateInfo =
        vk::PipelineRasterizationStateCreateInfo()
            .setDepthClampEnable(false)
            .setRasterizerDiscardEnable(false)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setLineWidth(1.0f)
            .setCullMode(vk::CullModeFlagBits::eBack)
            .setFrontFace(vk::FrontFace::eCounterClockwise)
            .setDepthBiasEnable(false)
            .setDepthBiasConstantFactor(0.0f)
            .setDepthBiasSlopeFactor(1.0f);

    // Depth testing
    vk::PipelineDepthStencilStateCreateInfo depthStencilState =
        vk::PipelineDepthStencilStateCreateInfo{}
            .setDepthTestEnable(true)
            .setDepthWriteEnable(true)
            .setDepthCompareOp(vk::CompareOp::eLess)
            .setDepthBoundsTestEnable(false)
            .setStencilTestEnable(false);

    // Setup viewport
    vk::Viewport viewport = vk::Viewport()
                                .setX(0.0f)
                                .setY(0.0f)
                                .setWidth(1.0f)
                                .setHeight(1.0f)
                                .setMinDepth(0.0f)
                                .setMaxDepth(1.0f);

    vk::Rect2D scissor = vk::Rect2D().setOffset(0).setExtent(vk::Extent2D{1, 1});

    vk::PipelineViewportStateCreateInfo viewportCreateInfo =
        vk::PipelineViewportStateCreateInfo().setViewports(viewport).setScissors(scissor);

    // Multisampling state
    const vk::PipelineMultisampleStateCreateInfo multisampleCreateInfo =
        vk::PipelineMultisampleStateCreateInfo()
            .setSampleShadingEnable(false)
            .setRasterizationSamples(vk::SampleCountFlagBits::e1);

    // Color blending state
    const vk::PipelineColorBlendAttachmentState baseAttachmentCreateInfo =
        vk::PipelineColorBlendAttachmentState()
            .setColorWriteMask(
                vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
            .setBlendEnable(false);

    std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments(
        1,
        baseAttachmentCreateInfo);

    vk::PipelineColorBlendStateCreateInfo colorBlendCreateInfo =
        vk::PipelineColorBlendStateCreateInfo()
            .setLogicOpEnable(false)
            .setLogicOp(vk::LogicOp::eCopy)
            .setAttachments(blendAttachments);

    const std::vector<vk::DynamicState> dynamicStates{
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo =
        vk::PipelineDynamicStateCreateInfo().setDynamicStates(dynamicStates);

    const vk::GraphicsPipelineCreateInfo pipelineCreateInfo =
        vk::GraphicsPipelineCreateInfo()
            .setStages(stageCreateInfos)
            .setLayout(mLayout)
            .setPVertexInputState(&vertexInputCreateInfo)
            .setPInputAssemblyState(&inputAssemblyCreateInfo)
            .setPRasterizationState(&rasterizerCreateInfo)
            .setPMultisampleState(&multisampleCreateInfo)
            .setPViewportState(&viewportCreateInfo)
            .setPDepthStencilState(renderPass->GetHasDepthTesting() ? &depthStencilState : nullptr)
            .setPColorBlendState(&colorBlendCreateInfo)
            .setPDynamicState(&dynamicStateCreateInfo)
            .setRenderPass(renderPass->GetRenderPassHandle())
            .setSubpass(0);

    mPipeline = VKRT_ASSERT_VK(logicalDevice.createGraphicsPipeline({}, pipelineCreateInfo));
}

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

Pipeline::~Pipeline() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    for (auto& entry : mShaders) {
        logicalDevice.destroyShaderModule(entry.second);
    }
    logicalDevice.destroyPipeline(mPipeline);
    logicalDevice.destroyPipelineLayout(mLayout);
}
}  // namespace VKRT