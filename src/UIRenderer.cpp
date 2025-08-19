#include "UIRenderer.h"

#include "DebugUtils.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

namespace VKRT {

// Based on: https://github.com/ocornut/imgui/blob/master/examples/example_glfw_vulkan/main.cpp

UIRenderer::UIRenderer(ScopedRefPtr<Context> context, ScopedRefPtr<RenderTarget> uiTarget)
    : mContext(context), mUITarget(uiTarget) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(uiTarget->GetWidth(), uiTarget->GetHeight());
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    ImGui::StyleColorsDark();

    {
        vk::DescriptorPoolSize poolSizes[] = {
            {vk::DescriptorType::eCombinedImageSampler, 1},
        };
        uint32_t maxSets = 0;
        for (vk::DescriptorPoolSize& poolSize : poolSizes) {
            maxSets += poolSize.descriptorCount;
        }
        vk::DescriptorPoolCreateInfo poolInfo =
            vk::DescriptorPoolCreateInfo()
                .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
                .setMaxSets(maxSets)
                .setPoolSizeCount((uint32_t)IM_ARRAYSIZE(poolSizes))
                .setPPoolSizes(poolSizes);
        mDescriptorPool = VKRT_ASSERT_VK(
            mContext->GetDevice()->GetLogicalDevice().createDescriptorPool(poolInfo));
    }

    {
        mRenderPass = new RenderPass(
            context,
            {
                {.renderTarget = uiTarget,
                 .loadOp = vk::AttachmentLoadOp::eLoad,
                 .initialLayout = vk::ImageLayout::eColorAttachmentOptimal,
                 .storeOp = vk::AttachmentStoreOp::eStore,
                 .finalLayout = vk::ImageLayout::ePresentSrcKHR},
            });
    }

    ImGui_ImplVulkan_InitInfo initInfo{
        .Instance = mContext->GetInstance()->GetHandle(),
        .PhysicalDevice = mContext->GetDevice()->GetPhysicalDevice(),
        .Device = mContext->GetDevice()->GetLogicalDevice(),
        .QueueFamily = mContext->GetDevice()->GetQueueFamilyIndex(),
        .Queue = mContext->GetDevice()->GetQueue(),
        .DescriptorPool = mDescriptorPool,
        .RenderPass = mRenderPass->GetRenderPassHandle(),
        .MinImageCount = mContext->GetMaxInFlightFrameCount(),
        .ImageCount = mContext->GetSwapchain()->GetImageCount(),
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .Allocator = nullptr,
        .CheckVkResultFn = VKRT_ASSERT_VK,
    };
    ImGui_ImplVulkan_Init(&initInfo);
}

void UIRenderer::Render(vk::CommandBuffer commandBuffer) {
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();

    {
        const std::vector<vk::ClearValue> clearValues{
            vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
        };
        const vk::RenderPassBeginInfo renderPassBeginInfo =
            vk::RenderPassBeginInfo()
                .setRenderPass(mRenderPass->GetRenderPassHandle())
                .setFramebuffer(
                    mRenderPass->GetFramebufferHandle(mContext->GetSwapchain()->GetCurrentIndex()))
                .setRenderArea(
                    {vk::Offset2D{0, 0}, {mUITarget->GetWidth(), mUITarget->GetHeight()}})
                .setClearValues(clearValues);
        commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
        commandBuffer.endRenderPass();
    }
}

UIRenderer::~UIRenderer() {
    ImGui_ImplVulkan_Shutdown();
    mContext->GetDevice()->GetLogicalDevice().destroyDescriptorPool(mDescriptorPool);
}

}  // namespace VKRT