#include "CommandRing.h"

#include "DebugUtils.h"

namespace VKRT {
CommandRing::CommandRing(ScopedRefPtr<Context> context) : mContext(context) {
    const uint32_t imageCount = mContext->GetSwapchain()->GetImageCount();
    for (uint32_t imageIndex = 0; imageIndex < imageCount; ++imageIndex) {
        vk::CommandBuffer commandBuffer = mContext->GetDevice()->CreateCommandBuffer();
        vk::Fence fence = mContext->GetDevice()->CreateFence(true);
        CommandResources resources = {.buffer = commandBuffer, .fence = fence};
        mCommands.push_back(resources);
    }
}

CommandRing::CommandResources& CommandRing::GetCommand(uint32_t frameIndex) {
    auto command = mCommands[frameIndex];
    VKRT_ASSERT_VK(mContext->GetDevice()->GetLogicalDevice().waitForFences(
        command.fence,
        true,
        std::numeric_limits<uint64_t>::max()));
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    logicalDevice.resetFences(command.fence);
    return command;
}

void CommandRing::Flush() {
    for (CommandResources& command : mCommands) {
        VKRT_ASSERT_VK(mContext->GetDevice()->GetLogicalDevice().waitForFences(
            command.fence,
            true,
            std::numeric_limits<uint64_t>::max()));
    }
}

CommandRing::~CommandRing() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    for (CommandResources& command : mCommands) {
        mContext->GetDevice()->DestroyCommand(command.buffer);
        mContext->GetDevice()->DestroyFence(command.fence);
    }
}

}  // namespace VKRT