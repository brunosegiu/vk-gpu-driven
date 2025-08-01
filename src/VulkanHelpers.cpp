#pragma once

#include "VulkanHelpers.h"

namespace VKRT {
vk::AccessFlags Helpers::GetAccessMasksForStage(vk::PipelineStageFlags stageFlags, bool isSource) {
    vk::AccessFlags accessFlags;

    if (stageFlags & vk::PipelineStageFlagBits::eDrawIndirect) {
        accessFlags |= vk::AccessFlagBits::eIndirectCommandRead;
    }
    if (stageFlags & vk::PipelineStageFlagBits::eVertexInput) {
        accessFlags |= vk::AccessFlagBits::eVertexAttributeRead | vk::AccessFlagBits::eIndexRead;
    }
    if (stageFlags & vk::PipelineStageFlagBits::eVertexShader ||
        stageFlags & vk::PipelineStageFlagBits::eFragmentShader ||
        stageFlags & vk::PipelineStageFlagBits::eComputeShader) {
        accessFlags |= vk::AccessFlagBits::eShaderRead;
        if (!isSource) {
            accessFlags |= vk::AccessFlagBits::eShaderWrite;
        }
    }
    if (stageFlags & vk::PipelineStageFlagBits::eEarlyFragmentTests ||
        stageFlags & vk::PipelineStageFlagBits::eLateFragmentTests) {
        accessFlags |= vk::AccessFlagBits::eDepthStencilAttachmentRead;
        if (!isSource) {
            accessFlags |= vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        }
    }
    if (stageFlags & vk::PipelineStageFlagBits::eColorAttachmentOutput) {
        accessFlags |= vk::AccessFlagBits::eColorAttachmentRead;
        if (!isSource) {
            accessFlags |= vk::AccessFlagBits::eColorAttachmentWrite;
        }
    }
    if (stageFlags & vk::PipelineStageFlagBits::eTransfer) {
        accessFlags |= vk::AccessFlagBits::eTransferRead;
        if (!isSource) {
            accessFlags |= vk::AccessFlagBits::eTransferWrite;
        }
    }
    if (stageFlags & vk::PipelineStageFlagBits::eHost) {
        if (isSource) {
            accessFlags |= vk::AccessFlagBits::eHostRead | vk::AccessFlagBits::eHostWrite;
        } else {
            accessFlags |= vk::AccessFlagBits::eHostWrite;
        }
    }
    if (stageFlags & vk::PipelineStageFlagBits::eAllGraphics ||
        stageFlags & vk::PipelineStageFlagBits::eAllCommands) {
        accessFlags |= vk::AccessFlagBits::eMemoryRead;
        if (!isSource) {
            accessFlags |= vk::AccessFlagBits::eMemoryWrite;
        }
    }

    return accessFlags;
}
}  // namespace VKRT