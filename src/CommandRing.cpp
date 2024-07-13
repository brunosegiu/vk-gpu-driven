#include "CommandRing.h"

#include "DebugUtils.h"

namespace VKRT {
CommandRing::CommandRing(ScopedRefPtr<Context> context)
    : mContext(context) {
    for (uint32_t imageIndex = 0; imageIndex < mContext->GetMaxInFlightFrameCount(); ++imageIndex) {
        vk::CommandBuffer commandBuffer = mContext->GetDevice()->CreateCommandBuffer();
        vk::Fence fence = mContext->GetDevice()->CreateFence(true);
        CommandResources resources = {.buffer = commandBuffer, .fence = fence};
        mCommands.push_back(resources);
    }
}

CommandRing::CommandResources& CommandRing::Cycle() {
    mCurrentIndex = (mCurrentIndex + 1) % mCommands.size();
    auto command = mCommands[mCurrentIndex];
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