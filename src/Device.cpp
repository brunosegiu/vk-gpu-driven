#include "Device.h"

#include <array>
#include <limits>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "DebugUtils.h"
#include "Instance.h"
#include "ResourceLoader.h"
#include "VulkanBuffer.h"
#include "Window.h"

namespace VKRT {

ResultValue<ScopedRefPtr<Device>> Device::Create(
    ScopedRefPtr<Instance> instance,
    const vk::SurfaceKHR& surface) {
    auto [result, physicalDevice] = instance->FindSuitablePhysicalDevice(surface);
    if (result == Result::Success) {
        return {Result::Success, new Device(instance, physicalDevice, surface)};
    }
    return {Result::InvalidDeviceError, nullptr};
}

Device::Device(
    ScopedRefPtr<Instance> instance,
    vk::PhysicalDevice physicalDevice,
    const vk::SurfaceKHR& surface)
    : mContext(nullptr), mPhysicalDevice(physicalDevice) {
    const std::vector<vk::QueueFamilyProperties> queueFamiliesProperties =
        mPhysicalDevice.getQueueFamilyProperties();
    uint32_t queueFamilyIndex = 0;
    for (const auto& properties : queueFamiliesProperties) {
        if (properties.queueFlags & vk::QueueFlagBits::eGraphics) {
            if (VKRT_ASSERT_VK(physicalDevice.getSurfaceSupportKHR(queueFamilyIndex, surface))) {
                break;
            }
        }
        ++queueFamilyIndex;
    }
    VKRT_ASSERT(queueFamilyIndex < static_cast<uint32_t>(queueFamiliesProperties.size()));
    const std::vector<float> queuePriorities{1.0f};
    vk::DeviceQueueCreateInfo queueCreateInfo = vk::DeviceQueueCreateInfo()
                                                    .setQueueFamilyIndex(queueFamilyIndex)
                                                    .setQueuePriorities(queuePriorities);

    vk::PhysicalDeviceFeatures enabledFeatures = vk::PhysicalDeviceFeatures()
                                                     .setShaderInt64(true)
                                                     .setSamplerAnisotropy(true)
                                                     .setMultiDrawIndirect(true)
                                                     .setGeometryShader(true);

    vk::PhysicalDeviceVulkan11Features enabledFeatures11 =
        vk::PhysicalDeviceVulkan11Features().setShaderDrawParameters(true);

    vk::PhysicalDeviceVulkan12Features enabledFeatures12 =
        vk::PhysicalDeviceVulkan12Features()
            .setScalarBlockLayout(true)
            .setBufferDeviceAddress(true)
            .setDescriptorIndexing(true)
            .setRuntimeDescriptorArray(true)
            .setDescriptorBindingVariableDescriptorCount(true)
            .setDrawIndirectCount(true)
            .setShaderSampledImageArrayNonUniformIndexing(true);
    enabledFeatures11.setPNext(&enabledFeatures12);

    const vk::DeviceCreateInfo deviceCreateInfo =
        vk::DeviceCreateInfo()
            .setQueueCreateInfos(queueCreateInfo)
            .setPEnabledExtensionNames(Instance::sRequiredDeviceExtensions)
            .setPEnabledFeatures(&enabledFeatures)
            .setPNext(&enabledFeatures11);
    mLogicalDevice = VKRT_ASSERT_VK(mPhysicalDevice.createDevice(deviceCreateInfo));

    mGraphicsQueue = mLogicalDevice.getQueue(queueFamilyIndex, 0);
    const vk::CommandPoolCreateInfo commandPoolCreateInfo =
        vk::CommandPoolCreateInfo()
            .setQueueFamilyIndex(queueFamilyIndex)
            .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    mCommandPool = VKRT_ASSERT_VK(mLogicalDevice.createCommandPool(commandPoolCreateInfo));

    mDispatcher = vk::detail::DispatchLoaderDynamic(
        instance->GetHandle(),
        vkGetInstanceProcAddr,
        mLogicalDevice,
        vkGetDeviceProcAddr);

    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.vulkanApiVersion = Instance::sVulkanVersion;
    allocatorInfo.physicalDevice = mPhysicalDevice;
    allocatorInfo.device = mLogicalDevice;
    allocatorInfo.instance = instance->GetHandle();
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;
    vmaCreateAllocator(&allocatorInfo, &mAllocator);
}

void Device::SetContext(ScopedRefPtr<Context> context) {
    mContext = context;
}

ScopedRefPtr<VulkanBuffer> Device::CreateBuffer(
    const vk::DeviceSize& size,
    const vk::BufferUsageFlags& usageFlags,
    const VmaAllocationCreateFlags& memoryFlags,
    const vk::MemoryAllocateFlags& memoryAllocateFlags) {
    VKRT_ASSERT(mContext != nullptr);
    return VulkanBuffer::Create(mContext, size, usageFlags, memoryFlags, memoryAllocateFlags);
}

std::vector<ScopedRefPtr<VulkanBuffer>> Device::CreateBuffers(
    const size_t& count,
    const vk::DeviceSize& size,
    const vk::BufferUsageFlags& usageFlags,
    const VmaAllocationCreateFlags& memoryFlags,
    const vk::MemoryAllocateFlags& memoryAllocateFlags) {
    VKRT_ASSERT(mContext != nullptr);
    std::vector<ScopedRefPtr<VulkanBuffer>> buffers;
    for (uint32_t bufferIndex = 0; bufferIndex < count; ++bufferIndex) {
        ScopedRefPtr<VulkanBuffer> buffer =
            VulkanBuffer::Create(mContext, size, usageFlags, memoryFlags, memoryAllocateFlags);
        buffers.push_back(buffer);
    }
    return buffers;
}

vk::CommandBuffer Device::CreateCommandBuffer() {
    vk::CommandBufferAllocateInfo commandInfo = vk::CommandBufferAllocateInfo()
                                                    .setCommandBufferCount(1)
                                                    .setCommandPool(mCommandPool)
                                                    .setLevel(vk::CommandBufferLevel::ePrimary);
    return VKRT_ASSERT_VK(mLogicalDevice.allocateCommandBuffers(commandInfo))[0];
}

void Device::SubmitCommand(const vk::CommandBuffer& commandBuffer, const vk::Fence& fence) {
    const vk::SubmitInfo submitInfo = vk::SubmitInfo().setCommandBuffers(commandBuffer);
    VKRT_ASSERT_VK(mGraphicsQueue.submit(submitInfo, fence));
}

void Device::SubmitCommandAndFlush(const vk::CommandBuffer& commandBuffer) {
    vk::Fence fence = CreateFence();
    SubmitCommand(commandBuffer, fence);
    WaitForFence(fence);
    DestroyFence(fence);
}

void Device::DestroyCommand(vk::CommandBuffer& commandBuffer) {
    mLogicalDevice.freeCommandBuffers(mCommandPool, commandBuffer);
}

vk::Fence Device::CreateFence(bool signaled) {
    vk::FenceCreateInfo fenceInfo = vk::FenceCreateInfo();
    if (signaled) {
        fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);
    }
    return VKRT_ASSERT_VK(mLogicalDevice.createFence(fenceInfo));
}

void Device::WaitForFence(vk::Fence& fence) {
    VKRT_ASSERT_VK(
        mLogicalDevice.waitForFences(fence, true, (std::numeric_limits<uint64_t>::max)()));
}

void Device::DestroyFence(vk::Fence& fence) {
    mLogicalDevice.destroyFence(fence);
}

Device::SwapchainCapabilities Device::GetSwapchainCapabilities(vk::SurfaceKHR surface) {
    SwapchainCapabilities capabilities{
        .surfaceCapabilities = VKRT_ASSERT_VK(mPhysicalDevice.getSurfaceCapabilitiesKHR(surface)),
        .supportedFormats = VKRT_ASSERT_VK(mPhysicalDevice.getSurfaceFormatsKHR(surface)),
        .supportedPresentModes = VKRT_ASSERT_VK(mPhysicalDevice.getSurfacePresentModesKHR(surface)),
    };
    return capabilities;
}

vk::PhysicalDeviceProperties Device::GetDeviceProperties() {
    return mPhysicalDevice.getProperties();
}

vk::PhysicalDeviceRayTracingPipelinePropertiesKHR Device::GetRayTracingProperties() {
    auto result = mPhysicalDevice.getProperties2<
        vk::PhysicalDeviceProperties2,
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
    return result.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
}

Device::~Device() {
    vmaDestroyAllocator(mAllocator);
    mLogicalDevice.destroyCommandPool(mCommandPool);
    mLogicalDevice.destroy();
}

}  // namespace VKRT
