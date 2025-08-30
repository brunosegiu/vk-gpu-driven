#pragma once

#include "Context.h"
#include "RefCountPtr.h"
#include "VulkanBase.h"

namespace VKRT {
class Device;

class CommandRing : public RefCountPtr {
public:
    CommandRing(ScopedRefPtr<Context> context);

    struct CommandResources {
        vk::CommandBuffer buffer;
        vk::Fence fence;
    };
    CommandResources& Cycle();

    void WaitPreviousFrame();

    void Flush();
    ~CommandRing();

private:
    ScopedRefPtr<Context> mContext;
    std::vector<CommandResources> mCommands;
    uint32_t mCurrentIndex;
};
}  // namespace VKRT
