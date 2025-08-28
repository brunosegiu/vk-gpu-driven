#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Macros.h"
#include "RefCountPtr.h"
#include "ResourceLoader.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"
#include "Texture.h"

namespace VKRT {

class Context;

enum class ParameterUpdateFrequency { PerFrame = 0, Once = 1 };
constexpr uint32_t ParameterUpdateFrequencyCount = 2u;

class Pipeline : public RefCountPtr {
public:
    Pipeline(ScopedRefPtr<Context> context);

    const vk::PipelineLayout& GetPipelineLayout() const { return mLayout; }
    const vk::Pipeline& GetPipelineHandle() const { return mPipeline; }

    std::vector<vk::DescriptorSet> GetDescriptorSets(uint32_t frameIndex);

    void Bind(
        ParameterUpdateFrequency frequency,
        uint32_t binding,
        ScopedRefPtr<VulkanBuffer> buffer);
    void Bind(
        ParameterUpdateFrequency frequency,
        uint32_t binding,
        std::vector<ScopedRefPtr<VulkanBuffer>> buffers);

    void Bind(ParameterUpdateFrequency frequency, uint32_t binding, ScopedRefPtr<Texture> texture);
    void Bind(
        ParameterUpdateFrequency frequency,
        uint32_t binding,
        std::vector<ScopedRefPtr<Texture>> textures);

    void Bind(ParameterUpdateFrequency frequency, uint32_t binding, vk::Sampler sampler);

    void Bind(ParameterUpdateFrequency frequency, uint32_t binding, vk::AccelerationStructureKHR accelerationStructure);

    std::vector<vk::DescriptorSetLayout> GetDescriptorLayouts();

    virtual ~Pipeline();

protected:
    void LoadShaders(
        std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>> shaders);

    void CreateDescriptorLayouts(vk::ShaderStageFlags stageFlags, const Resource& resource);
    void CreateDescriptorSets();
    void UpdateDescriptors(uint32_t frameIndex);

    ScopedRefPtr<Context> mContext;
    vk::PipelineLayout mLayout;
    vk::Pipeline mPipeline;
    std::unordered_map<vk::ShaderStageFlagBits, std::vector<vk::ShaderModule>> mShaders;

    struct ParameterData {
        vk::DescriptorType descriptorType;
        uint32_t binding;
        uint32_t count;
        vk::ShaderStageFlags stageFlags;
        bool hasVariableCount;
        std::vector<std::vector<vk::DescriptorBufferInfo>> bufferInfos;
        std::vector<std::vector<vk::DescriptorImageInfo>> imageInfos;
        std::vector<vk::WriteDescriptorSetAccelerationStructureKHR> accelerationStructureInfos;
        vk::AccelerationStructureKHR accelerationStructure;
    };
    struct DescriptorData {
        std::vector<ParameterData> parameters;
        vk::DescriptorSetLayout descriptorLayout;
        std::vector<vk::DescriptorSet> descriptorSets;
    };
    std::unordered_map<ParameterUpdateFrequency, DescriptorData> mParameters;
    vk::DescriptorPool mDescriptorPool;
    std::vector<vk::DescriptorPoolSize> mDescriptorSizes;
    bool mDescriptorsUpdatedOnce;
    bool mHasDescriptorSets;
};
}  // namespace VKRT