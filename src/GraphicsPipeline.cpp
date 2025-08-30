#include "GraphicsPipeline.h"

#include <unordered_map>

#include "Context.h"
#include "DebugUtils.h"

namespace VKRT {
GraphicsPipeline::GraphicsPipeline(
    ScopedRefPtr<Context> context,
    const std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>>&
        shaderResourcesMap,
    ScopedRefPtr<RenderPass> renderPass,
    const std::vector<GeometryLayout>& geometryLayout,
    GraphicsPipelineOptionals optionals)
    : Pipeline(context) {
    std::vector<vk::VertexInputBindingDescription> vertexInputDescriptions;
    std::vector<vk::VertexInputAttributeDescription> vertexInputAttributeDescriptions;
    uint32_t binding = 0;
    for (const GeometryLayout& geometryLayoutEntry : geometryLayout) {
        vertexInputDescriptions.push_back(vk::VertexInputBindingDescription()
                                              .setBinding(binding)
                                              .setStride(geometryLayoutEntry.stride)
                                              .setInputRate(vk::VertexInputRate::eVertex));

        vertexInputAttributeDescriptions.push_back(vk::VertexInputAttributeDescription()
                                                       .setBinding(binding)
                                                       .setLocation(binding)
                                                       .setFormat(geometryLayoutEntry.format));
        ++binding;
    }

    vk::PipelineVertexInputStateCreateInfo vertexInputCreateInfo =
        vk::PipelineVertexInputStateCreateInfo()
            .setVertexBindingDescriptions(vertexInputDescriptions)
            .setVertexAttributeDescriptions(vertexInputAttributeDescriptions);

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo =
        vk::PipelineInputAssemblyStateCreateInfo()
            .setTopology(vk::PrimitiveTopology::eTriangleList)
            .setPrimitiveRestartEnable(false);

    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();

    std::vector<vk::PipelineShaderStageCreateInfo> stageCreateInfos;
    LoadShaders(shaderResourcesMap);
    for (const auto& entry : mShaders) {
        const vk::PipelineShaderStageCreateInfo stageCreateInfo =
            vk::PipelineShaderStageCreateInfo()
                .setStage(entry.first)
                .setModule(entry.second.front())
                .setPName("main");
        stageCreateInfos.emplace_back(stageCreateInfo);
    }

    const std::vector<vk::DescriptorSetLayout> layouts = GetDescriptorLayouts();
    vk::PipelineLayoutCreateInfo layoutCreateInfo =
        vk::PipelineLayoutCreateInfo().setSetLayouts(layouts);
    mLayout = VKRT_ASSERT_VK(logicalDevice.createPipelineLayout(layoutCreateInfo));

    // Rasterization setup
    const vk::PipelineRasterizationStateCreateInfo rasterizerCreateInfo =
        vk::PipelineRasterizationStateCreateInfo()
            .setDepthClampEnable(false)
            .setRasterizerDiscardEnable(false)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setLineWidth(1.0f)
            .setCullMode(
                optionals.enableCulling ? vk::CullModeFlagBits::eBack : vk::CullModeFlagBits::eNone)
            .setFrontFace(
                optionals.reverseWindingOrder ? vk::FrontFace::eClockwise
                                              : vk::FrontFace::eCounterClockwise)
            .setDepthBiasEnable(optionals.enableDepthBias)
            .setDepthBiasConstantFactor(optionals.depthBias)
            .setDepthBiasSlopeFactor(optionals.depthSlope);

    // Depth testing
    vk::PipelineDepthStencilStateCreateInfo depthStencilState =
        vk::PipelineDepthStencilStateCreateInfo{}
            .setDepthTestEnable(optionals.enableDepthTest)
            .setDepthWriteEnable(optionals.enableDepthTest)
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
            .setBlendEnable(optionals.enableBlending)
            .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
            .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
            .setColorBlendOp(vk::BlendOp::eAdd)
            .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
            .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
            .setAlphaBlendOp(vk::BlendOp::eAdd)
            .setColorWriteMask(
                vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments(
        glm::max<uint32_t>(renderPass->GetColorAttachmentCount(), 1),
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

GraphicsPipeline::~GraphicsPipeline() {
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