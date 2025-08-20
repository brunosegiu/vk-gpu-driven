#include "RaytracingPipeline.h"

#include <unordered_map>

#include "Context.h"
#include "DebugUtils.h"

namespace VKRT {

RaytracingPipeline::RaytracingPipeline(
    ScopedRefPtr<Context> context,
    const ScopedRefPtr<ShaderParameterCollection>& parameters,
    const std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>>& shaderResources)
    : Pipeline(context) {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();

    std::vector<vk::PushConstantRange> pushConstants = parameters->GetPushConstants();
    const std::vector<vk::DescriptorSetLayout> layouts = parameters->GetLayouts();
    vk::PipelineLayoutCreateInfo layoutCreateInfo =
        vk::PipelineLayoutCreateInfo().setSetLayouts(layouts).setPushConstantRanges(pushConstants);
    mLayout = VKRT_ASSERT_VK(logicalDevice.createPipelineLayout(layoutCreateInfo));

    const std::array<vk::ShaderStageFlagBits, 3> stageOrder{
        vk::ShaderStageFlagBits::eRaygenKHR,
        vk::ShaderStageFlagBits::eClosestHitKHR,
        vk::ShaderStageFlagBits::eMissKHR,
    };

    std::vector<vk::PipelineShaderStageCreateInfo> stageCreateInfos;
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> rayTracingGroupCreateInfos;
    {
        for (const std::pair<vk::ShaderStageFlagBits, std::vector<Resource::Id>>& entry :
             shaderResources) {
            std::vector<vk::ShaderModule> modules;
            for (const Resource::Id& shaders : entry.second) {
                modules.push_back(LoadShader(shaders));
            }
            mShaders.emplace(entry.first, modules);
        }

        uint32_t shaderIndex = 0;
        for (const vk::ShaderStageFlagBits stage : stageOrder) {
            if (mShaders.find(stage) != mShaders.end()) {
                for (const vk::ShaderModule& module : mShaders.at(stage)) {
                    stageCreateInfos.push_back(vk::PipelineShaderStageCreateInfo()
                                                   .setPName("main")
                                                   .setModule(module)
                                                   .setStage(stage));
                    vk::RayTracingShaderGroupCreateInfoKHR groupCreateInfo =
                        vk::RayTracingShaderGroupCreateInfoKHR()
                            .setAnyHitShader(VK_SHADER_UNUSED_KHR)
                            .setClosestHitShader(VK_SHADER_UNUSED_KHR)
                            .setIntersectionShader(VK_SHADER_UNUSED_KHR)
                            .setGeneralShader(VK_SHADER_UNUSED_KHR);
                    if (stage == vk::ShaderStageFlagBits::eClosestHitKHR) {
                        groupCreateInfo.setType(
                            vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup);
                        groupCreateInfo.setClosestHitShader(shaderIndex);
                    } else {
                        groupCreateInfo.setGeneralShader(shaderIndex);
                        groupCreateInfo.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
                    }
                    rayTracingGroupCreateInfos.push_back(groupCreateInfo);
                    ++shaderIndex;
                }
            }
        }
    }

    vk::RayTracingPipelineCreateInfoKHR rayTracingPipelineCreateInfo =
        vk::RayTracingPipelineCreateInfoKHR()
            .setStages(stageCreateInfos)
            .setGroups(rayTracingGroupCreateInfos)
            .setMaxPipelineRayRecursionDepth(
                mContext->GetDevice()->GetRayTracingProperties().maxRayRecursionDepth)
            .setLayout(mLayout);
    mPipeline = VKRT_ASSERT_VK(logicalDevice.createRayTracingPipelineKHR(
        {},
        {},
        rayTracingPipelineCreateInfo,
        nullptr,
        mContext->GetDevice()->GetDispatcher()));

    vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties =
        mContext->GetDevice()->GetRayTracingProperties();
    mHandleSize = rayTracingProperties.shaderGroupHandleSize;
    const size_t handleAlignment = rayTracingProperties.shaderGroupHandleAlignment;
    mHandleSizeAligned = (mHandleSize + handleAlignment - 1) & ~(handleAlignment - 1);
    const uint32_t groupCount = static_cast<uint32_t>(rayTracingGroupCreateInfos.size());
    const size_t sbtSize = groupCount * mHandleSizeAligned;

    std::vector<uint8_t> shaderHandleStorage =
        VKRT_ASSERT_VK(logicalDevice.getRayTracingShaderGroupHandlesKHR<uint8_t>(
            mPipeline,
            0,
            groupCount,
            sbtSize,
            mContext->GetDevice()->GetDispatcher()));

    {
        mRayGenTable = mContext->GetDevice()->CreateBuffer(
            mHandleSize,
            vk::BufferUsageFlagBits::eShaderBindingTableKHR |
                vk::BufferUsageFlagBits::eShaderDeviceAddress,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        uint8_t* rayGenTableData = mRayGenTable->MapBuffer();
        std::copy_n(shaderHandleStorage.begin(), mHandleSize, rayGenTableData);
        mRayGenTable->UnmapBuffer();
    }

    {
        mRayHitTable = mContext->GetDevice()->CreateBuffer(
            mHandleSize,
            vk::BufferUsageFlagBits::eShaderBindingTableKHR |
                vk::BufferUsageFlagBits::eShaderDeviceAddress,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT);
        uint8_t* rayHitTableData = mRayHitTable->MapBuffer();
        std::copy_n(shaderHandleStorage.begin() + mHandleSizeAligned, mHandleSize, rayHitTableData);
        mRayHitTable->UnmapBuffer();
    }

    uint32_t missTableCount = mShaders.at(vk::ShaderStageFlagBits::eMissKHR).size();

    {
        mRayMissTable = mContext->GetDevice()->CreateBuffer(
            mHandleSize * missTableCount,
            vk::BufferUsageFlagBits::eShaderBindingTableKHR |
                vk::BufferUsageFlagBits::eShaderDeviceAddress,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT);
        uint8_t* rayMissTableData = mRayMissTable->MapBuffer();
        std::copy_n(
            shaderHandleStorage.begin() + mHandleSizeAligned * missTableCount,
            mHandleSize * missTableCount,
            rayMissTableData);
        mRayMissTable->UnmapBuffer();
    }

    mTableRef = RayTracingTablesRef{
        .rayGen = vk::StridedDeviceAddressRegionKHR()
                      .setDeviceAddress(mRayGenTable->GetDeviceAddress())
                      .setSize(mHandleSizeAligned)
                      .setStride(mHandleSizeAligned),
        .rayHit = vk::StridedDeviceAddressRegionKHR()
                      .setDeviceAddress(mRayHitTable->GetDeviceAddress())
                      .setSize(mHandleSizeAligned)
                      .setStride(mHandleSizeAligned),
        .rayMiss = vk::StridedDeviceAddressRegionKHR()
                       .setDeviceAddress(mRayMissTable->GetDeviceAddress())
                       .setSize(mHandleSizeAligned * missTableCount)
                       .setStride(mHandleSizeAligned),
        .callable = vk::StridedDeviceAddressRegionKHR()};
}

RaytracingPipeline::~RaytracingPipeline() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    logicalDevice.destroyPipeline(mPipeline);
    logicalDevice.destroyPipelineLayout(mLayout);
    for (auto& entry : mShaders) {
        for (vk::ShaderModule& module : entry.second) {
            logicalDevice.destroyShaderModule(module);
        }
    }
}

}  // namespace VKRT