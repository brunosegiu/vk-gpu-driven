#include "ShaderParameterCollection.h"

#include <array>

#include "DebugUtils.h"

namespace VKRT {

ShaderParameterCollection::ShaderParameterCollection(ScopedRefPtr<Context> context)
    : mContext(context) {}

constexpr std::array<ShaderParameter::UpdateFrequency, 2> sUpdateFrequencies{
    ShaderParameter::UpdateFrequency::PerFrame,
    ShaderParameter::UpdateFrequency::Once};

void ShaderParameterCollection::CreateDescriptorLayout() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();

    std::unordered_map<vk::DescriptorType, uint32_t> descriptorSizes;
    for (ShaderParameter::UpdateFrequency updateFrequency : sUpdateFrequencies) {
        const std::vector<ScopedRefPtr<ShaderParameter>>& shaderParemeters =
            mParameters[updateFrequency];

        if (shaderParemeters.empty()) {
            continue;
        }

        std::vector<vk::DescriptorSetLayoutBinding> descriptorBindings;
        std::vector<vk::DescriptorBindingFlags> bindingFlags;
        uint32_t descriptorBinding = 0;
        for (const ScopedRefPtr<ShaderParameter>& parameter : shaderParemeters) {
            parameter->SetBinding(descriptorBinding);
            descriptorBindings.emplace_back(vk::DescriptorSetLayoutBinding()
                                                .setBinding(descriptorBinding)
                                                .setDescriptorType(parameter->GetDescriptorType())
                                                .setDescriptorCount(parameter->GetCount())
                                                .setStageFlags(parameter->GetStageFlags()));

            vk::DescriptorBindingFlags bindingFlag =
                parameter->GetHasVariableCount()
                    ? vk::DescriptorBindingFlagBits::eVariableDescriptorCount
                    : vk::DescriptorBindingFlags{};
            bindingFlags.emplace_back(bindingFlag);
            ++descriptorBinding;
        }

        {
            for (const vk::DescriptorSetLayoutBinding& binding : descriptorBindings) {
                auto it = descriptorSizes.find(binding.descriptorType);
                if (it == descriptorSizes.end()) {
                    descriptorSizes[binding.descriptorType] =
                        binding.descriptorCount * mContext->GetMaxInFlightFrameCount();
                } else {
                    descriptorSizes[binding.descriptorType] +=
                        binding.descriptorCount * mContext->GetMaxInFlightFrameCount();
                }
            }
        }

        vk::DescriptorSetLayoutBindingFlagsCreateInfo layoutFlagsCreateInfo =
            vk::DescriptorSetLayoutBindingFlagsCreateInfo().setBindingFlags(bindingFlags);

        vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo =
            vk::DescriptorSetLayoutCreateInfo()
                .setBindings(descriptorBindings)
                .setPNext(&layoutFlagsCreateInfo);

        vk::DescriptorSetLayout layout = VKRT_ASSERT_VK(logicalDevice.createDescriptorSetLayout(
            descriptorSetLayoutCreateInfo,
            nullptr,
            mContext->GetDevice()->GetDispatcher()));

        mDescriptorSetLayouts.emplace(updateFrequency, layout);
    }

    mDescriptorSizes = std::vector<vk::DescriptorPoolSize>();
    for (auto descriptorSize : descriptorSizes) {
        mDescriptorSizes.emplace_back(descriptorSize.first, descriptorSize.second);
    }
}

void ShaderParameterCollection::CreateDescriptorPool() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    {
        vk::DescriptorPoolCreateInfo poolCreateInfo =
            vk::DescriptorPoolCreateInfo()
                .setPoolSizes(mDescriptorSizes)
                .setMaxSets(sUpdateFrequencies.size() * mContext->GetSwapchain()->GetImageCount());
        mDescriptorPool = VKRT_ASSERT_VK(logicalDevice.createDescriptorPool(poolCreateInfo));
    }
}

void ShaderParameterCollection::CreateDescriptorSets() {
    if (!mDescriptorSets.empty()) {
        return;
    }
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    CreateDescriptorPool();
    for (ShaderParameter::UpdateFrequency updateFrequency : sUpdateFrequencies) {
        const std::vector<ScopedRefPtr<ShaderParameter>>& shaderParemeters =
            mParameters[updateFrequency];
        if (shaderParemeters.empty()) {
            continue;
        }

        size_t descriptorCount = 1;
        if (updateFrequency == ShaderParameter::UpdateFrequency::PerFrame) {
            descriptorCount = mContext->GetMaxInFlightFrameCount();
        }

        std::vector<vk::DescriptorSetLayout> layouts(
            descriptorCount,
            mDescriptorSetLayouts[updateFrequency]);

        uint32_t dynamicCount = 0;
        for (const ScopedRefPtr<ShaderParameter>& parameter : shaderParemeters) {
            if (parameter->GetHasVariableCount()) {
                dynamicCount = parameter->GetVariableCount();
            }
        }
        std::vector<uint32_t> dynamicCounts(descriptorCount, dynamicCount);

        vk::DescriptorSetVariableDescriptorCountAllocateInfo dynamicCountInfo =
            vk::DescriptorSetVariableDescriptorCountAllocateInfo().setDescriptorCounts(
                dynamicCounts);

        vk::DescriptorSetAllocateInfo descriptorAllocateInfo =
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(mDescriptorPool)
                .setSetLayouts(layouts)
                .setPNext(&dynamicCountInfo);

        std::vector<vk::DescriptorSet> descriptorSets =
            VKRT_ASSERT_VK(logicalDevice.allocateDescriptorSets(
                descriptorAllocateInfo,
                mContext->GetDevice()->GetDispatcher()));

        mDescriptorSets.emplace(updateFrequency, descriptorSets);
    }
}

void ShaderParameterCollection::UpdateDescriptors(uint32_t frameIndex) {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
    for (ShaderParameter::UpdateFrequency updateFrequency : sUpdateFrequencies) {
        const std::vector<ScopedRefPtr<ShaderParameter>>& shaderParemeters =
            mParameters[updateFrequency];
        if (shaderParemeters.empty()) {
            continue;
        }
        const uint32_t effectiveFrameIndex =
            updateFrequency == ShaderParameter::UpdateFrequency::PerFrame ? frameIndex : 0;

        const vk::DescriptorSet& descriptorSet =
            mDescriptorSets[updateFrequency][effectiveFrameIndex];
        for (const ScopedRefPtr<ShaderParameter>& parameter : shaderParemeters) {
            const uint32_t binding = parameter->GetBinding();
            vk::WriteDescriptorSet descriptorUpdate =
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSet)
                    .setDstBinding(binding)
                    .setDescriptorType(parameter->GetDescriptorType())
                    .setDescriptorCount(1);

            switch (parameter->GetDescriptorType()) {
                case vk::DescriptorType::eSampler: {
                    const std::vector<vk::DescriptorImageInfo> imageInfos =
                        parameter->GetImageInfos();
                    descriptorUpdate.setImageInfo(imageInfos);
                } break;
                case vk::DescriptorType::eUniformBuffer:
                case vk::DescriptorType::eStorageBuffer: {
                    const vk::DescriptorBufferInfo bufferInfo =
                        parameter->GetBufferInfo(effectiveFrameIndex);
                    descriptorUpdate.setBufferInfo(bufferInfo);
                } break;
                case vk::DescriptorType::eSampledImage: {
                    const std::vector<vk::DescriptorImageInfo> imageInfos =
                        parameter->GetImageInfos();
                    descriptorUpdate.setImageInfo(imageInfos);
                } break;
            }

            writeDescriptorSets.emplace_back(descriptorUpdate);
        }
    }

    logicalDevice.updateDescriptorSets(writeDescriptorSets, {});
}

void ShaderParameterCollection::AddParameter(ScopedRefPtr<ShaderParameter> parameter) {
    std::vector<ScopedRefPtr<ShaderParameter>>& parameters =
        mParameters[parameter->GetUpdateFrequency()];
    parameters.emplace_back(parameter);
}

std::vector<vk::DescriptorSetLayout> ShaderParameterCollection::GetLayouts() {
    if (mDescriptorSetLayouts.empty()) {
        CreateDescriptorLayout();
    }
    std::vector<vk::DescriptorSetLayout> layouts;
    auto itPerFrame = mDescriptorSetLayouts.find(ShaderParameter::UpdateFrequency::PerFrame);
    if (itPerFrame != mDescriptorSetLayouts.end()) {
        layouts.emplace_back(itPerFrame->second);
    }

    auto itOnce = mDescriptorSetLayouts.find(ShaderParameter::UpdateFrequency::Once);
    if (itOnce != mDescriptorSetLayouts.end()) {
        layouts.emplace_back(itOnce->second);
    }

    return layouts;
}

std::vector<vk::DescriptorSet> ShaderParameterCollection::GetDescriptorSets(uint32_t frameIndex) {
    std::vector<vk::DescriptorSet> descriptors;
    auto itPerFrame = mDescriptorSets.find(ShaderParameter::UpdateFrequency::PerFrame);
    if (itPerFrame != mDescriptorSets.end()) {
        descriptors.emplace_back(itPerFrame->second.at(frameIndex));
    }
    auto itOnce = mDescriptorSets.find(ShaderParameter::UpdateFrequency::Once);
    if (itOnce != mDescriptorSets.end()) {
        descriptors.emplace_back(itOnce->second.front());
    }
    return descriptors;
}

std::vector<vk::PushConstantRange> ShaderParameterCollection::GetPushConstants() {
    std::vector<vk::PushConstantRange> pushConstants;
    vk::DeviceSize currentOffset = 0;
    for (ScopedRefPtr<ShaderParameter> perFrameParameter :
         mParameters.at(ShaderParameter::UpdateFrequency::PerDraw)) {
        ShaderParameterPushConstant* pushConstantParameter =
            dynamic_cast<ShaderParameterPushConstant*>(perFrameParameter.Get());
        vk::PushConstantRange pushConstant =
            vk::PushConstantRange()
                .setSize(pushConstantParameter->GetSize())
                .setStageFlags(pushConstantParameter->GetStageFlags())
                .setOffset(currentOffset);
        pushConstantParameter->SetOffset(currentOffset);
        currentOffset += pushConstantParameter->GetSize();
        pushConstants.emplace_back(pushConstant);
    }
    return pushConstants;
}

ShaderParameterCollection::~ShaderParameterCollection() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    logicalDevice.destroyDescriptorPool(mDescriptorPool);
    for (auto entry : mDescriptorSetLayouts) {
        logicalDevice.destroyDescriptorSetLayout(entry.second);
    }
}

}  // namespace VKRT