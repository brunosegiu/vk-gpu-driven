#include "RaytracingPipeline.h"

#include <unordered_map>

#include "Context.h"
#include "DebugUtils.h"

namespace VKRT {

RaytracingPipeline::RaytracingPipeline(
    ScopedRefPtr<Context> context,
    const std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>>& shaderResources)
    : Pipeline(context) {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    
    LoadShaders(shaderResources);

    const std::vector<vk::DescriptorSetLayout> layouts = GetDescriptorLayouts();
    vk::PipelineLayoutCreateInfo layoutCreateInfo =
        vk::PipelineLayoutCreateInfo().setSetLayouts(layouts);
    mLayout = VKRT_ASSERT_VK(logicalDevice.createPipelineLayout(layoutCreateInfo));

    const std::array<vk::ShaderStageFlagBits, 3> stageOrder{
        vk::ShaderStageFlagBits::eRaygenKHR,
        vk::ShaderStageFlagBits::eClosestHitKHR,
        vk::ShaderStageFlagBits::eMissKHR,
    };

    std::vector<vk::PipelineShaderStageCreateInfo> stageCreateInfos;
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> rayTracingGroupCreateInfos;
    {

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

    const auto alignUp = [](uint64_t handle, uint64_t alignment) {
        return (handle + alignment - 1) & ~(alignment - 1);
    };
    const uint64_t baseAlignment = rayTracingProperties.shaderGroupBaseAlignment;
    const uint64_t handleAlignment = rayTracingProperties.shaderGroupHandleAlignment;

    const uint64_t handleSize = rayTracingProperties.shaderGroupHandleSize;
    const uint64_t handleSizeAligned = alignUp(handleSize, handleAlignment);

    const uint64_t groupCount = static_cast<uint64_t>(rayTracingGroupCreateInfos.size());
    const uint64_t sbtSize = groupCount * handleSize;

    std::vector<uint8_t> shaderHandleStorage =
        VKRT_ASSERT_VK(logicalDevice.getRayTracingShaderGroupHandlesKHR<uint8_t>(
            mPipeline,
            0,
            groupCount,
            sbtSize,
            mContext->GetDevice()->GetDispatcher()));

    uint8_t* shaderHandleStoragePtr = shaderHandleStorage.data();

    vk::DeviceAddress raygenAddr;
    {
        mRayGenTable = mContext->GetDevice()->CreateBuffer(
            handleSize + baseAlignment - 1,
            vk::BufferUsageFlagBits::eShaderBindingTableKHR |
                vk::BufferUsageFlagBits::eShaderDeviceAddress,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        raygenAddr = alignUp(mRayGenTable->GetDeviceAddress(), baseAlignment);
        uint64_t writeOffset = uint64_t(raygenAddr - mRayGenTable->GetDeviceAddress());
        uint8_t* rayGenTableData = mRayGenTable->MapBuffer();
        std::copy_n(shaderHandleStoragePtr, handleSize, rayGenTableData + writeOffset);
        shaderHandleStoragePtr += handleSize;
        mRayGenTable->UnmapBuffer();
    }

    vk::DeviceAddress rayHitAddr;
    {
        mRayHitTable = mContext->GetDevice()->CreateBuffer(
            handleSize + baseAlignment - 1,
            vk::BufferUsageFlagBits::eShaderBindingTableKHR |
                vk::BufferUsageFlagBits::eShaderDeviceAddress,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT);
        rayHitAddr = alignUp(mRayHitTable->GetDeviceAddress(), baseAlignment);
        uint64_t writeOffset = uint64_t(rayHitAddr - mRayHitTable->GetDeviceAddress());
        uint8_t* rayHitTableData = mRayHitTable->MapBuffer();
        std::copy_n(shaderHandleStoragePtr, handleSize, rayHitTableData + writeOffset);
        shaderHandleStoragePtr += handleSize;
        mRayHitTable->UnmapBuffer();
    }

    uint32_t missTableCount = mShaders.at(vk::ShaderStageFlagBits::eMissKHR).size();
    vk::DeviceAddress rayMissAddr;
    {
        mRayMissTable = mContext->GetDevice()->CreateBuffer(
            handleSize * missTableCount + baseAlignment - 1,
            vk::BufferUsageFlagBits::eShaderBindingTableKHR |
                vk::BufferUsageFlagBits::eShaderDeviceAddress,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT);
        rayMissAddr = alignUp(mRayMissTable->GetDeviceAddress(), baseAlignment);
        uint64_t writeOffset = uint64_t(rayMissAddr - mRayMissTable->GetDeviceAddress());
        uint8_t* rayMissTableData = mRayMissTable->MapBuffer();

        for (uint32_t i = 0; i < missTableCount; ++i) {
            std::copy_n(
                shaderHandleStoragePtr,
                handleSize,
                rayMissTableData + writeOffset + i * handleSizeAligned);
            shaderHandleStoragePtr += handleSize;
        }

        mRayMissTable->UnmapBuffer();
    }

    mTableRef = RayTracingTablesRef{
        .rayGen = vk::StridedDeviceAddressRegionKHR()
                      .setDeviceAddress(raygenAddr)
                      .setSize(handleSizeAligned)
                      .setStride(handleSizeAligned),
        .rayHit = vk::StridedDeviceAddressRegionKHR()
                      .setDeviceAddress(rayHitAddr)
                      .setSize(handleSizeAligned)
                      .setStride(handleSizeAligned),
        .rayMiss = vk::StridedDeviceAddressRegionKHR()
                       .setDeviceAddress(rayMissAddr)
                       .setSize(handleSizeAligned * missTableCount)
                       .setStride(handleSizeAligned),
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