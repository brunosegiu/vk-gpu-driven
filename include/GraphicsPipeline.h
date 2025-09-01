#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Macros.h"
#include "Mesh.h"
#include "Pipeline.h"
#include "RefCountPtr.h"
#include "RenderPass.h"
#include "ResourceLoader.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"

namespace VKRT {

class Context;

struct GraphicsPipelineOptionals {
    bool enableDepthTest = true;
    bool enableDepthBias = false;
    bool enableCulling = true;
    bool reverseCulling = false;
    float depthBias = 0.0f;
    float depthSlope = 1.0f;
    bool enableBlending = false;
    bool reverseWindingOrder = false;
};

class GraphicsPipeline : public Pipeline {
public:
    GraphicsPipeline(
        ScopedRefPtr<Context> context,
        const std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>>&
            shaderResourcesMap,
        ScopedRefPtr<RenderPass> renderPass,
        const std::vector<GeometryLayout>& geometryLayout,
        GraphicsPipelineOptionals optionals = {});

    ~GraphicsPipeline();
};
}  // namespace VKRT