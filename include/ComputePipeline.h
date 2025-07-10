#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Macros.h"
#include "RefCountPtr.h"
#include "ResourceLoader.h"
#include "ShaderParameterCollection.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"
#include "Pipeline.h"

namespace VKRT {

class Context;

class ComputePipeline : public Pipeline {
public:
    ComputePipeline(
        ScopedRefPtr<Context> context,
        const ScopedRefPtr<ShaderParameterCollection>& parameters,
        const std::pair<vk::ShaderStageFlagBits, Resource::Id>& shaderResourcesMap);

    ~ComputePipeline();
};
}  // namespace VKRT