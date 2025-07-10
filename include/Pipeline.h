#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Macros.h"
#include "RefCountPtr.h"
#include "ResourceLoader.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"

namespace VKRT {

class Context;

class Pipeline : public RefCountPtr {
public:
    Pipeline(ScopedRefPtr<Context> context);

    const vk::PipelineLayout& GetPipelineLayout() const { return mLayout; }
    const vk::Pipeline& GetPipelineHandle() const { return mPipeline; }

    virtual ~Pipeline();

protected:
    vk::ShaderModule LoadShader(Resource::Id shaderId);

    ScopedRefPtr<Context> mContext;
    vk::PipelineLayout mLayout;
    vk::Pipeline mPipeline;
    std::unordered_map<vk::ShaderStageFlagBits, vk::ShaderModule> mShaders;
};
}  // namespace VKRT