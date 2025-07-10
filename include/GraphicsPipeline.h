#pragma once

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
#include "Pipeline.h"

namespace VKRT {

class Context;

struct GeometryLayout {
    vk::Format format;
    size_t stride;
};

class GraphicsPipeline : public Pipeline {
public:
    GraphicsPipeline(
        ScopedRefPtr<Context> context,
        const ScopedRefPtr<ShaderParameterCollection>& parameters,
        const std::unordered_map<vk::ShaderStageFlagBits, Resource::Id>& shaderResourcesMap,
        ScopedRefPtr<RenderPass> renderPass,
        const std::vector<GeometryLayout>& geometryLayout);

    ~GraphicsPipeline();
};
}  // namespace VKRT