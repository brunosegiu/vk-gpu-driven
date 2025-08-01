#pragma once

#include "Context.h"

namespace VKRT {
class Helpers {
public:
    static vk::AccessFlags GetAccessMasksForStage(vk::PipelineStageFlags stageFlags, bool isSource);

    Helpers() = delete;
    ~Helpers() = delete;
};
}  // namespace VKRT