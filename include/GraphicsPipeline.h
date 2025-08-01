#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Macros.h"
#include "Pipeline.h"
#include "RefCountPtr.h"
#include "RenderPass.h"
#include "ResourceLoader.h"
#include "ShaderParameterCollection.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"
#include "Mesh.h"

namespace VKRT {

class Context;

struct GraphicsPipelineOptionals {
    bool enableDepthBias = false;
    float depthBias = 0.0f;
    float depthSlope = 1.0f;
};

class GraphicsPipeline : public Pipeline {
public:
    GraphicsPipeline(
        ScopedRefPtr<Context> context,
        const ScopedRefPtr<ShaderParameterCollection>& parameters,
        const std::unordered_map<vk::ShaderStageFlagBits, Resource::Id>& shaderResourcesMap,
        ScopedRefPtr<RenderPass> renderPass,
        const std::vector<GeometryLayout>& geometryLayout,
        GraphicsPipelineOptionals optionals = {});

    ~GraphicsPipeline();
};
}  // namespace VKRT