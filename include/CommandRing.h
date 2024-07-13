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
    CommandResources& GetCommand(uint32_t frameIndex);

    void Flush();
    ~CommandRing();

private:
    ScopedRefPtr<Context> mContext;
    std::vector<CommandResources> mCommands;
};
}  // namespace VKRT
