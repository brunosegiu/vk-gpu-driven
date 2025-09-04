#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Macros.h"
#include "RefCountPtr.h"
#include "ResourceLoader.h"
#include "Texture.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"

namespace VKRT {

class Context;

enum class ParameterUpdateFrequency { PerFrame = 0, Once = 1 };
constexpr uint32_t ParameterUpdateFrequencyCount = 2u;
constexpr uint32_t MaxBindlessCount = 4096u;

class Pipeline : public RefCountPtr {
public:
    Pipeline(ScopedRefPtr<Context> context);

    const vk::PipelineLayout& GetPipelineLayout() const { return mLayout; }
    const vk::Pipeline& GetPipelineHandle() const { return mPipeline; }

    std::vector<vk::DescriptorSet> GetDescriptorSets(uint32_t frameIndex);

    // Persistent parameters
    void Bind(uint32_t binding, const ScopedRefPtr<VulkanBuffer>& buffer);
    void Bind(uint32_t binding, const std::vector<ScopedRefPtr<VulkanBuffer>>& buffers);
    void Bind(uint32_t binding, const ScopedRefPtr<Texture>& texture, int32_t mipIndex = -1);
    void Bind(
        uint32_t binding,
        const std::vector<ScopedRefPtr<Texture>>& textures,
        int32_t mipIndex = -1);
    void Bind(uint32_t binding, const vk::Sampler& sampler);
    void Bind(uint32_t binding, const vk::AccelerationStructureKHR& accelerationStructure);

    // Per-frame parameters
    void Bind(uint32_t frameIndex, uint32_t binding, const ScopedRefPtr<VulkanBuffer>& buffer);
    void Bind(
        uint32_t frameIndex,
        uint32_t binding,
        const std::vector<ScopedRefPtr<VulkanBuffer>>& buffers);
    void Bind(
        uint32_t frameIndex,
        uint32_t binding,
        const ScopedRefPtr<Texture>& texture,
        int32_t mipIndex = -1);
    void Bind(
        uint32_t frameIndex,
        uint32_t binding,
        const std::vector<ScopedRefPtr<Texture>>& textures,
        int32_t mipIndex = -1);
    void Bind(uint32_t frameIndex, uint32_t binding, const vk::Sampler& sampler);
    void Bind(
        uint32_t frameIndex,
        uint32_t binding,
        const vk::AccelerationStructureKHR& accelerationStructure);

    std::vector<vk::DescriptorSetLayout> GetDescriptorLayouts();

    virtual ~Pipeline();

protected:
    void LoadShaders(
        std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>> shaders);

    void CreateDescriptorLayouts(vk::ShaderStageFlags stageFlags, const Resource& resource);
    void CreateDescriptorSets();
    void UpdateDescriptors(uint32_t frameIndex);

    void Bind(
        ParameterUpdateFrequency frequency,
        uint32_t binding,
        uint32_t frameIndex,
        const ScopedRefPtr<VulkanBuffer>& buffer);
    void Bind(
        ParameterUpdateFrequency frequency,
        uint32_t binding,
        uint32_t frameIndex,
        const std::vector<ScopedRefPtr<VulkanBuffer>>& buffers);

    void Bind(
        ParameterUpdateFrequency frequency,
        uint32_t frameIndex,
        uint32_t binding,
        const ScopedRefPtr<Texture>& texture,
        int32_t mipIndex);
    void Bind(
        ParameterUpdateFrequency frequency,
        uint32_t frameIndex,
        uint32_t binding,
        const std::vector<ScopedRefPtr<Texture>>& textures,
        int32_t mipIndex);

    void Bind(
        ParameterUpdateFrequency frequency,
        uint32_t frameIndex,
        uint32_t binding,
        const vk::Sampler& sampler);

    void Bind(
        ParameterUpdateFrequency frequency,
        uint32_t frameIndex,
        uint32_t binding,
        const vk::AccelerationStructureKHR& accelerationStructure);

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
        vk::Sampler sampler;
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