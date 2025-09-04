#include "Pipeline.h"

#include <unordered_map>

#include <spirv-reflect/spirv_reflect.h>

#include "Context.h"
#include "DebugUtils.h"

namespace VKRT {
Pipeline::Pipeline(ScopedRefPtr<Context> context)
    : mContext(context), mDescriptorsUpdatedOnce(false), mHasDescriptorSets(false) {}

void Pipeline::LoadShaders(
    std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>> shaders) {
    vk::ShaderStageFlags stageFlags;
    for (const auto& shaderEntry : shaders) {
        stageFlags |= shaderEntry.first;
    }

    for (const auto& shaderEntry : shaders) {
        for (const auto& resourceId : shaderEntry.second) {
            Resource shaderResource = ResourceLoader::Load(resourceId);
            vk::ShaderModuleCreateInfo shaderCreateInfo =
                vk::ShaderModuleCreateInfo()
                    .setCodeSize(shaderResource.size * sizeof(uint8_t))
                    .setPCode(reinterpret_cast<const uint32_t*>(shaderResource.buffer));
            mShaders[shaderEntry.first].push_back(VKRT_ASSERT_VK(
                mContext->GetDevice()->GetLogicalDevice().createShaderModule(shaderCreateInfo)));
            CreateDescriptorLayouts(stageFlags, shaderResource);
            ResourceLoader::CleanUp(shaderResource);
        }
    }
}

void Pipeline::CreateDescriptorLayouts(vk::ShaderStageFlags stageFlags, const Resource& resource) {
    std::unordered_map<vk::DescriptorType, uint32_t> descriptorSizes;
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();

    if (mParameters.empty()) {
        SpvReflectShaderModule module;
        SpvReflectResult result = spvReflectCreateShaderModule(
            resource.size * sizeof(uint8_t),
            reinterpret_cast<const uint32_t*>(resource.buffer),
            &module);
        VKRT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

        uint32_t variableCount = 0;
        result = spvReflectEnumerateDescriptorSets(&module, &variableCount, NULL);
        VKRT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
        std::vector<SpvReflectDescriptorSet*> descriptorData(variableCount);
        result = spvReflectEnumerateDescriptorSets(&module, &variableCount, descriptorData.data());
        VKRT_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

        for (SpvReflectDescriptorSet* setData : descriptorData) {
            ParameterUpdateFrequency frequency =
                static_cast<ParameterUpdateFrequency>(setData->set);

            std::vector<vk::DescriptorSetLayoutBinding> descriptorBindings;
            std::vector<vk::DescriptorBindingFlags> bindingFlags;

            for (uint32_t bindingIndex = 0; bindingIndex < setData->binding_count; ++bindingIndex) {
                SpvReflectDescriptorBinding* binding = setData->bindings[bindingIndex];

                ParameterData& parameterData = mParameters[frequency].parameters.emplace_back();
                parameterData.binding = binding->binding;
                parameterData.count = binding->count;
                parameterData.hasVariableCount =
                    binding->type_description->traits.array.dims_count > 0 &&
                    (binding->type_description->op == SpvOpTypeArray ||
                     binding->type_description->op == SpvOpTypeRuntimeArray);
                parameterData.stageFlags = stageFlags;

                switch (binding->descriptor_type) {
                    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: {
                        parameterData.descriptorType = vk::DescriptorType::eSampler;
                    } break;
                    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
                        parameterData.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                    } break;
                    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
                        parameterData.descriptorType = vk::DescriptorType::eSampledImage;
                    } break;
                    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
                        parameterData.descriptorType = vk::DescriptorType::eStorageImage;
                    } break;
                    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: {
                        parameterData.descriptorType = vk::DescriptorType::eUniformTexelBuffer;
                    } break;
                    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
                        parameterData.descriptorType = vk::DescriptorType::eStorageTexelBuffer;
                    } break;
                    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
                        parameterData.descriptorType = vk::DescriptorType::eUniformBuffer;
                    } break;
                    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
                        parameterData.descriptorType = vk::DescriptorType::eStorageBuffer;
                    } break;
                    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: {
                        parameterData.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
                    } break;
                    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
                        parameterData.descriptorType = vk::DescriptorType::eStorageBufferDynamic;
                    } break;
                    case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: {
                        parameterData.descriptorType = vk::DescriptorType::eInputAttachment;
                    } break;
                    case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {
                        parameterData.descriptorType =
                            vk::DescriptorType::eAccelerationStructureKHR;
                    } break;
                }

                const uint32_t count =
                    parameterData.hasVariableCount ? MaxBindlessCount : parameterData.count;
                descriptorSizes[parameterData.descriptorType] +=
                    count * mContext->GetMaxInFlightFrameCount();

                {
                    descriptorBindings.emplace_back(
                        vk::DescriptorSetLayoutBinding()
                            .setBinding(parameterData.binding)
                            .setDescriptorType(parameterData.descriptorType)
                            .setDescriptorCount(count)
                            .setStageFlags(stageFlags));

                    vk::DescriptorBindingFlags bindingFlag =
                        parameterData.hasVariableCount
                            ? vk::DescriptorBindingFlagBits::eVariableDescriptorCount
                            : vk::DescriptorBindingFlags{};
                    bindingFlags.emplace_back(bindingFlag);
                }

                const uint32_t descriptorCount = frequency == ParameterUpdateFrequency::PerFrame
                                                     ? mContext->GetMaxInFlightFrameCount()
                                                     : 1;
                switch (parameterData.descriptorType) {
                    case vk::DescriptorType::eUniformBuffer:
                    case vk::DescriptorType::eStorageBuffer: {
                        parameterData.bufferInfos =
                            std::vector<std::vector<vk::DescriptorBufferInfo>>(descriptorCount);
                    } break;
                    case vk::DescriptorType::eSampler:
                    case vk::DescriptorType::eSampledImage:
                    case vk::DescriptorType::eStorageImage: {
                        parameterData.imageInfos =
                            std::vector<std::vector<vk::DescriptorImageInfo>>(descriptorCount);
                    } break;
                    case vk::DescriptorType::eAccelerationStructureKHR: {
                        parameterData.accelerationStructureInfos =
                            std::vector<vk::WriteDescriptorSetAccelerationStructureKHR>(
                                descriptorCount);
                    } break;
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

            mParameters[frequency].descriptorLayout = layout;
        }
        spvReflectDestroyShaderModule(&module);
        {
            mDescriptorSizes = std::vector<vk::DescriptorPoolSize>();
            for (auto descriptorSize : descriptorSizes) {
                mDescriptorSizes.emplace_back(descriptorSize.first, descriptorSize.second);
            }

            vk::DescriptorPoolCreateInfo poolCreateInfo =
                vk::DescriptorPoolCreateInfo()
                    .setPoolSizes(mDescriptorSizes)
                    .setMaxSets(
                        ParameterUpdateFrequencyCount * mContext->GetSwapchain()->GetImageCount());
            mDescriptorPool = VKRT_ASSERT_VK(logicalDevice.createDescriptorPool(poolCreateInfo));
        }
    }
}

void Pipeline::CreateDescriptorSets() {
    if (!mHasDescriptorSets) {
        vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
        for (auto& entry : mParameters) {
            const ParameterUpdateFrequency& frequency = entry.first;
            std::vector<ParameterData>& parameterData = entry.second.parameters;

            size_t descriptorCount = 1;
            if (frequency == ParameterUpdateFrequency::PerFrame) {
                descriptorCount = mContext->GetMaxInFlightFrameCount();
            }

            std::vector<vk::DescriptorSetLayout> layouts(
                descriptorCount,
                mParameters[frequency].descriptorLayout);
            uint32_t dynamicCount = 0;
            for (const ParameterData& parameter : parameterData) {
                if (parameter.hasVariableCount) {
                    dynamicCount = parameter.imageInfos.front().size();
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

            entry.second.descriptorSets = descriptorSets;
        }

        mHasDescriptorSets = true;
    }
}

void Pipeline::UpdateDescriptors(uint32_t frameIndex) {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
    for (auto& entry : mParameters) {
        const ParameterUpdateFrequency& frequency = entry.first;
        std::vector<ParameterData>& parameterData = entry.second.parameters;

        if (parameterData.empty()) {
            continue;
        }
        if (frequency == ParameterUpdateFrequency::Once && mDescriptorsUpdatedOnce) {
            continue;
        }
        const uint32_t effectiveFrameIndex =
            frequency == ParameterUpdateFrequency::PerFrame ? frameIndex : 0;

        const vk::DescriptorSet& descriptorSet = entry.second.descriptorSets[effectiveFrameIndex];
        for (const ParameterData& parameter : parameterData) {
            vk::WriteDescriptorSet descriptorUpdate =
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSet)
                    .setDstBinding(parameter.binding)
                    .setDescriptorType(parameter.descriptorType)
                    .setDescriptorCount(1);

            switch (parameter.descriptorType) {
                case vk::DescriptorType::eUniformBuffer:
                case vk::DescriptorType::eStorageBuffer: {
                    descriptorUpdate.setBufferInfo(parameter.bufferInfos[effectiveFrameIndex]);
                } break;
                case vk::DescriptorType::eSampler:
                case vk::DescriptorType::eSampledImage:
                case vk::DescriptorType::eStorageImage: {
                    descriptorUpdate.setImageInfo(parameter.imageInfos[effectiveFrameIndex]);
                } break;
                case vk::DescriptorType::eAccelerationStructureKHR: {
                    descriptorUpdate.setPNext(
                        &parameter.accelerationStructureInfos[effectiveFrameIndex]);
                } break;
            }

            writeDescriptorSets.emplace_back(descriptorUpdate);
        }
    }
    logicalDevice.updateDescriptorSets(writeDescriptorSets, {});
    mDescriptorsUpdatedOnce = true;
}

std::vector<vk::DescriptorSet> Pipeline::GetDescriptorSets(uint32_t frameIndex) {
    CreateDescriptorSets();
    UpdateDescriptors(frameIndex);

    std::vector<vk::DescriptorSet> descriptors;
    descriptors.push_back(
        mParameters[ParameterUpdateFrequency::PerFrame].descriptorSets[frameIndex]);
    descriptors.push_back(mParameters[ParameterUpdateFrequency::Once].descriptorSets[0]);
    return descriptors;
}

// Persistent parameters
void Pipeline::Bind(uint32_t binding, const ScopedRefPtr<VulkanBuffer>& buffer) {
    Bind(ParameterUpdateFrequency::Once, 0, binding, buffer);
}

void Pipeline::Bind(uint32_t binding, const std::vector<ScopedRefPtr<VulkanBuffer>>& buffers) {
    Bind(ParameterUpdateFrequency::Once, 0, binding, buffers);
}

void Pipeline::Bind(uint32_t binding, const ScopedRefPtr<Texture>& texture, int32_t mipIndex) {
    Bind(ParameterUpdateFrequency::Once, 0, binding, texture, mipIndex);
}

void Pipeline::Bind(
    uint32_t binding,
    const std::vector<ScopedRefPtr<Texture>>& textures,
    int32_t mipIndex) {
    Bind(ParameterUpdateFrequency::Once, 0, binding, textures, mipIndex);
}

void Pipeline::Bind(uint32_t binding, const vk::Sampler& sampler) {
    Bind(ParameterUpdateFrequency::Once, 0, binding, sampler);
}

void Pipeline::Bind(uint32_t binding, const vk::AccelerationStructureKHR& accelerationStructure) {
    Bind(ParameterUpdateFrequency::Once, 0, binding, accelerationStructure);
}

// Per-frame parameters
void Pipeline::Bind(
    uint32_t frameIndex,
    uint32_t binding,
    const ScopedRefPtr<VulkanBuffer>& buffer) {
    Bind(ParameterUpdateFrequency::PerFrame, frameIndex, binding, buffer);
}

void Pipeline::Bind(
    uint32_t frameIndex,
    uint32_t binding,
    const std::vector<ScopedRefPtr<VulkanBuffer>>& buffers) {
    Bind(ParameterUpdateFrequency::PerFrame, frameIndex, binding, buffers);
}

void Pipeline::Bind(
    uint32_t frameIndex,
    uint32_t binding,
    const ScopedRefPtr<Texture>& texture,
    int32_t mipIndex) {
    Bind(ParameterUpdateFrequency::PerFrame, frameIndex, binding, texture, mipIndex);
}

void Pipeline::Bind(
    uint32_t binding,
    uint32_t frameIndex,
    const std::vector<ScopedRefPtr<Texture>>& textures,
    int32_t mipIndex) {
    Bind(ParameterUpdateFrequency::PerFrame, frameIndex, binding, textures, mipIndex);
}

void Pipeline::Bind(uint32_t frameIndex, uint32_t binding, const vk::Sampler& sampler) {
    Bind(ParameterUpdateFrequency::PerFrame, frameIndex, binding, sampler);
}

void Pipeline::Bind(
    uint32_t frameIndex,
    uint32_t binding,
    const vk::AccelerationStructureKHR& accelerationStructure) {
    Bind(ParameterUpdateFrequency::PerFrame, frameIndex, binding, accelerationStructure);
}

void Pipeline::Bind(
    ParameterUpdateFrequency frequency,
    uint32_t frameIndex,
    uint32_t binding,
    const ScopedRefPtr<VulkanBuffer>& buffer) {
    Bind(frequency, frameIndex, binding, std::vector<ScopedRefPtr<VulkanBuffer>>{buffer});
}

void Pipeline::Bind(
    ParameterUpdateFrequency frequency,
    uint32_t frameIndex,
    uint32_t binding,
    const std::vector<ScopedRefPtr<VulkanBuffer>>& buffers) {
    ParameterData& parameter = mParameters[frequency].parameters[binding];
    VKRT_ASSERT(
        parameter.descriptorType == vk::DescriptorType::eUniformBuffer ||
        parameter.descriptorType == vk::DescriptorType::eStorageBuffer);
    frameIndex = frequency == ParameterUpdateFrequency::Once ? 0 : frameIndex;
    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    for (const ScopedRefPtr<VulkanBuffer>& buffer : buffers) {
        bufferInfos.push_back(buffer->GetDescriptorInfo());
    }
    parameter.bufferInfos[frameIndex] = bufferInfos;
}

void Pipeline::Bind(
    ParameterUpdateFrequency frequency,
    uint32_t frameIndex,
    uint32_t binding,
    const ScopedRefPtr<Texture>& texture,
    int32_t mipIndex) {
    Bind(frequency, frameIndex, binding, std::vector<ScopedRefPtr<Texture>>{texture}, mipIndex);
}

void Pipeline::Bind(
    ParameterUpdateFrequency frequency,
    uint32_t frameIndex,
    uint32_t binding,
    const std::vector<ScopedRefPtr<Texture>>& textures,
    int32_t mipIndex) {
    ParameterData& parameter = mParameters[frequency].parameters[binding];
    VKRT_ASSERT(
        parameter.descriptorType == vk::DescriptorType::eSampledImage ||
        parameter.descriptorType == vk::DescriptorType::eStorageImage);

    bool isReadOnly = mParameters[frequency].parameters[binding].descriptorType ==
                      vk::DescriptorType::eSampledImage;

    frameIndex = frequency == ParameterUpdateFrequency::Once ? 0 : frameIndex;
    std::vector<vk::DescriptorImageInfo> imageInfos;
    for (const ScopedRefPtr<Texture>& texture : textures) {
        imageInfos.push_back(texture->GetDescriptorInfo(isReadOnly, mipIndex));
    }
    parameter.imageInfos[frameIndex] = imageInfos;
}

void Pipeline::Bind(
    ParameterUpdateFrequency frequency,
    uint32_t frameIndex,
    uint32_t binding,
    const vk::Sampler& sampler) {
    ParameterData& parameter = mParameters[frequency].parameters[binding];
    VKRT_ASSERT(parameter.descriptorType == vk::DescriptorType::eSampler);
    frameIndex = frequency == ParameterUpdateFrequency::Once ? 0 : frameIndex;
    parameter.sampler = sampler;
    parameter.imageInfos[frameIndex] = {vk::DescriptorImageInfo().setSampler(parameter.sampler)};
}

void Pipeline::Bind(
    ParameterUpdateFrequency frequency,
    uint32_t frameIndex,
    uint32_t binding,
    const vk::AccelerationStructureKHR& accelerationStructure) {
    ParameterData& parameter = mParameters[frequency].parameters[binding];
    VKRT_ASSERT(parameter.descriptorType == vk::DescriptorType::eAccelerationStructureKHR);
    frameIndex = frequency == ParameterUpdateFrequency::Once ? 0 : frameIndex;
    parameter.accelerationStructure = accelerationStructure;
    parameter.accelerationStructureInfos[frameIndex] = {
        vk::WriteDescriptorSetAccelerationStructureKHR().setAccelerationStructures(
            parameter.accelerationStructure)};
}

std::vector<vk::DescriptorSetLayout> Pipeline::GetDescriptorLayouts() {
    std::vector<vk::DescriptorSetLayout> layouts;
    const vk::DescriptorSetLayout perFrameLayout =
        mParameters[ParameterUpdateFrequency::PerFrame].descriptorLayout;
    const vk::DescriptorSetLayout onceLayout =
        mParameters[ParameterUpdateFrequency::Once].descriptorLayout;
    if (perFrameLayout != nullptr) {
        layouts.push_back(perFrameLayout);
    }
    if (onceLayout != nullptr) {
        layouts.push_back(onceLayout);
    }
    return layouts;
}

Pipeline::~Pipeline() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    logicalDevice.destroyDescriptorPool(mDescriptorPool);
    for (auto& parameter : mParameters) {
        logicalDevice.destroyDescriptorSetLayout(parameter.second.descriptorLayout);
    }
}

}  // namespace VKRT