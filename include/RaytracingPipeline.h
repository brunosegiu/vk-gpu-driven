#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Macros.h"
#include "Pipeline.h"
#include "RefCountPtr.h"
#include "ShaderParameterCollection.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"

namespace VKRT {

class Context;

class RaytracingPipeline : public Pipeline {
public:
    RaytracingPipeline(
        ScopedRefPtr<Context> context,
        const ScopedRefPtr<ShaderParameterCollection>& parameters,
        const std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>>& shaderResources);

    struct RayTracingTablesRef {
        vk::StridedDeviceAddressRegionKHR rayGen, rayHit, rayMiss, callable;
    };
    const RayTracingTablesRef& GetTablesRef() const { return mTableRef; }

    ~RaytracingPipeline();

private:
    size_t mHandleSize, mHandleSizeAligned;
    ScopedRefPtr<VulkanBuffer> mRayGenTable;
    ScopedRefPtr<VulkanBuffer> mRayHitTable;
    ScopedRefPtr<VulkanBuffer> mRayMissTable;
    RayTracingTablesRef mTableRef;
};
}  // namespace VKRT