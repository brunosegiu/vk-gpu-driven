#pragma once

#include <unordered_map>

#include "ShaderParameter.h"

namespace VKRT {
class ShaderParameterCollection : public RefCountPtr {
public:
    ShaderParameterCollection(ScopedRefPtr<Context> context);

    void AddParameter(ScopedRefPtr<ShaderParameter> parameter);

    void CreateDescriptorSets();

    void UpdateDescriptors(uint32_t frameIndex);

    std::vector<vk::DescriptorSetLayout> GetLayouts();
    std::vector<vk::DescriptorSet> GetDescriptorSets(uint32_t frameIndex);
    std::vector<vk::PushConstantRange> GetPushConstants();

    ~ShaderParameterCollection();

private:
    void CreateDescriptorLayout();
    void CreateDescriptorPool();

    ScopedRefPtr<Context> mContext;

    std::unordered_map<ShaderParameter::UpdateFrequency, std::vector<ScopedRefPtr<ShaderParameter>>>
        mParameters;
    vk::DescriptorPool mDescriptorPool;
    std::unordered_map<ShaderParameter::UpdateFrequency, std::vector<vk::DescriptorSet>>
        mDescriptorSets;
    std::unordered_map<ShaderParameter::UpdateFrequency, vk::DescriptorSetLayout>
        mDescriptorSetLayouts;
    std::vector<vk::DescriptorPoolSize> mDescriptorSizes;
    bool mUpdatedOnce;
};
}  // namespace VKRT
