#include "UIRenderer.h"

#include "DebugUtils.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

namespace VKRT {

// Based on: https://github.com/ocornut/imgui/blob/master/examples/example_glfw_vulkan/main.cpp

void ApplyFlatStyle() {
    auto Hex = [](uint32_t rgb, float a = 1.0f) -> ImVec4 {
        return ImVec4(
            ((rgb >> 16) & 0xFF) / 255.0f,
            ((rgb >> 8) & 0xFF) / 255.0f,
            ((rgb >> 0) & 0xFF) / 255.0f,
            a);
    };

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 0.0f;
    style.ChildRounding = 10.0f;
    style.FrameRounding = 10.0f;
    style.PopupRounding = 10.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 10.0f;
    style.TabRounding = 10.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;

    style.WindowPadding = ImVec2(14, 14);
    style.FramePadding = ImVec2(10, 8);
    style.CellPadding = ImVec2(10, 8);
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.SeparatorTextBorderSize = 0.0f;

    // --- Palette ---
    // Base surfaces (very dark, slightly bluish/neutral)
    const ImVec4 bg0 = Hex(0x0E1116, 0.5f);   // viewport bg
    const ImVec4 bg1 = Hex(0x12151B, 0.5f);   // window bg
    const ImVec4 bg2 = Hex(0x181C23, 0.5f);   // child/panel
    const ImVec4 bg3 = Hex(0x1F252D, 0.5f);   // hover/active fill
    const ImVec4 bdr = Hex(0x2A313C, 1.0f);   // subtle border
    const ImVec4 bdrF = Hex(0x3B4654, 1.0f);  // border on hover/active

    const ImVec4 txt = Hex(0xE6E6E6);
    const ImVec4 txtMuted = Hex(0xAEB4BF);
    const ImVec4 txtDim = Hex(0x7E8794);

    const ImVec4 accent = Hex(0x10A37F);
    const ImVec4 accentHover = Hex(0x16B08D);
    const ImVec4 accentActive = Hex(0x0E8F72);

    const ImVec4 okCol = Hex(0x28C76F);
    const ImVec4 warnCol = Hex(0xFFB020);
    const ImVec4 errCol = Hex(0xE05252);

    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text] = txt;
    colors[ImGuiCol_TextDisabled] = txtDim;
    colors[ImGuiCol_WindowBg] = bg1;
    colors[ImGuiCol_ChildBg] = bg2;
    colors[ImGuiCol_PopupBg] = bg2;
    colors[ImGuiCol_Border] = bdr;
    colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_FrameBg] = bg2;
    colors[ImGuiCol_FrameBgHovered] = bg3;
    colors[ImGuiCol_FrameBgActive] = bg3;
    colors[ImGuiCol_TitleBg] = bg1;
    colors[ImGuiCol_TitleBgActive] = bg2;
    colors[ImGuiCol_TitleBgCollapsed] = bg1;
    colors[ImGuiCol_MenuBarBg] = bg1;
    colors[ImGuiCol_Header] = bg2;
    colors[ImGuiCol_HeaderHovered] = bg3;
    colors[ImGuiCol_HeaderActive] = bg3;
    colors[ImGuiCol_Button] = Hex(0x14181F);
    colors[ImGuiCol_ButtonHovered] = bg3;
    colors[ImGuiCol_ButtonActive] = Hex(0x27303B);
    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = Hex(0xA5B3C2);
    colors[ImGuiCol_SliderGrabActive] = accent;
    colors[ImGuiCol_Tab] = Hex(0x151A21);
    colors[ImGuiCol_TabHovered] = bg3;
    colors[ImGuiCol_TabActive] = Hex(0x1B2028);
    colors[ImGuiCol_TabUnfocused] = Hex(0x12161C);
    colors[ImGuiCol_TabUnfocusedActive] = Hex(0x171C23);
    colors[ImGuiCol_Separator] = bdr;
    colors[ImGuiCol_SeparatorHovered] = bdrF;
    colors[ImGuiCol_SeparatorActive] = bdrF;
    colors[ImGuiCol_ResizeGrip] = Hex(0x9AA6B21A, 0.10f);
    colors[ImGuiCol_ResizeGripHovered] = Hex(0x9AA6B2, 0.35f);
    colors[ImGuiCol_ResizeGripActive] = accent;
    colors[ImGuiCol_ScrollbarBg] = bg1;
    colors[ImGuiCol_ScrollbarGrab] = Hex(0x333B46);
    colors[ImGuiCol_ScrollbarGrabHovered] = Hex(0x3A4451);
    colors[ImGuiCol_ScrollbarGrabActive] = Hex(0x465364);
    colors[ImGuiCol_TextSelectedBg] = Hex(0x344056, 0.55f);
    colors[ImGuiCol_NavHighlight] = accent;
    colors[ImGuiCol_NavWindowingHighlight] = Hex(0xFFFFFF, 0.30f);
    colors[ImGuiCol_NavWindowingDimBg] = Hex(0x0E1116, 0.60f);
    colors[ImGuiCol_TableHeaderBg] = Hex(0x192028);
    colors[ImGuiCol_TableBorderStrong] = bdr;
    colors[ImGuiCol_TableBorderLight] = Hex(0x2F3642);
    colors[ImGuiCol_TableRowBg] = Hex(0x000000, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = Hex(0xFFFFFF, 0.02f);
    colors[ImGuiCol_PlotLines] = accent;
    colors[ImGuiCol_PlotLinesHovered] = accentHover;
    colors[ImGuiCol_PlotHistogram] = okCol;
    colors[ImGuiCol_PlotHistogramHovered] = warnCol;
    colors[ImGuiCol_DragDropTarget] = accent;
    colors[ImGuiCol_ModalWindowDimBg] = Hex(0x0E1116, 0.60f);
    colors[ImGuiCol_Button] = Hex(0x0F141A);
    colors[ImGuiCol_ButtonHovered] = Hex(0x182029);
    colors[ImGuiCol_ButtonActive] = Hex(0x1F2833);
    colors[ImGuiCol_HeaderActive] = Hex(0x22303A);
    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrabActive] = accent;
    colors[ImGuiCol_SeparatorHovered] = accentHover;
    colors[ImGuiCol_SeparatorActive] = accentActive;
}

UIRenderer::UIRenderer(ScopedRefPtr<Context> context, ScopedRefPtr<SettingsManager> settingsManager)
    : mContext(context), mSettingsManager(settingsManager) {
    ApplyFlatStyle();
}

void UIRenderer::AddRenderTargets(ScopedRefPtr<RenderTarget> uiTarget) {
    {
        mUITarget = uiTarget;

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(uiTarget->GetWidth(), uiTarget->GetHeight());
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

        mRenderPass = new RenderPass(
            mContext,
            {
                {.renderTarget = mUITarget,
                 .loadOp = vk::AttachmentLoadOp::eLoad,
                 .initialLayout = vk::ImageLayout::eColorAttachmentOptimal,
                 .storeOp = vk::AttachmentStoreOp::eStore,
                 .finalLayout = vk::ImageLayout::ePresentSrcKHR},
            });
    }
}

void UIRenderer::AddResources() {
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

bool SliderUint(
    const char* label,
    uint32_t* v,
    uint32_t v_min,
    uint32_t v_max,
    const char* format = "%d",
    ImGuiSliderFlags flags = 0) {
    return ImGui::SliderScalar(label, ImGuiDataType_U32, v, &v_min, &v_max, format, flags);
}

bool SliderUint2(
    const char* label,
    uint32_t* v,
    uint32_t v_min,
    uint32_t v_max,
    const char* format = "%d",
    ImGuiSliderFlags flags = 0) {
    return ImGui::SliderScalarN(label, ImGuiDataType_U32, v, 2, &v_min, &v_max, format, flags);
}

bool SliderUint3(
    const char* label,
    uint32_t* v,
    uint32_t v_min,
    uint32_t v_max,
    const char* format = "%d",
    ImGuiSliderFlags flags = 0) {
    return ImGui::SliderScalarN(label, ImGuiDataType_U32, v, 3, &v_min, &v_max, format, flags);
}

void UIRenderer::Update() {
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    {
        ImVec2 windowSize{
            static_cast<float>(mUITarget->GetWidth()),
            static_cast<float>(mUITarget->GetHeight())};
        ImVec2 panelSize{windowSize.x / 4.0f, windowSize.y};
        ImVec2 panelPos{windowSize.x - panelSize.x, 0.0f};
        ImGui::SetNextWindowSize(panelSize);
        ImGui::SetNextWindowPos(panelPos);
        ImGui::Begin(
            "Settings",
            nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoDecoration);
        {
            if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen)) {
                SliderUint(
                    "Shadow map res",
                    &mSettingsManager->GetShadowMapResolution(),
                    128u,
                    8096u);
                SliderUint("Shadow taps", &mSettingsManager->GetShadowTaps(), 1u, 51u);
                ImGui::SliderFloat("Frustum width", &mSettingsManager->GetShadowFrustumWidth(), 0.0f, 100.0f);
                ImGui::SliderFloat("Distance", &mSettingsManager->GetShadowDistance(), 0.0f, 1000.0f);
                ImGui::SliderFloat("Shadow far", &mSettingsManager->GetShadowFar(), 0.1f, 1500.0f);
                ImGui::SliderFloat("Shadow near", &mSettingsManager->GetShadowNear(), 0.1f, 10.0f);
            }

            if (ImGui::CollapsingHeader("SSAO", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::SliderFloat(
                    "Radius",
                    &mSettingsManager->GetSSAOControlData().radius,
                    0.02f,
                    10.0f);
                ImGui::SliderFloat(
                    "Power",
                    &mSettingsManager->GetSSAOControlData().power,
                    0.02f,
                    10.0f);
                SliderUint(
                    "Kernel size",
                    &mSettingsManager->GetSSAOControlData().kernelSize,
                    1u,
                    128u);
                ImGui::SliderInt(
                    "Blur radius",
                    &mSettingsManager->GetSSAOControlData().blurRadius,
                    1,
                    10);
            }

            if (ImGui::CollapsingHeader("DDGI", ImGuiTreeNodeFlags_DefaultOpen)) {
                SliderUint3("Probe count", &mSettingsManager->GetProbeGridCount().x, 2, 32);
                SliderUint2("Probe resolution", &mSettingsManager->GetProbeResolution().x, 8, 512);
                SliderUint("Rays per probe", &mSettingsManager->GetProbeRayCount(), 8, 512);
                ImGui::SliderFloat3(
                    "Probe spacing",
                    &mSettingsManager->GetProbeSpacing().x,
                    0.02f,
                    10.0f);
                ImGui::SliderFloat3(
                    "Probe Grid Origin",
                    &mSettingsManager->GetProbeGridOrigin().x,
                    0.0f,
                    1000.0f);
                ImGui::SliderFloat("Hysteresis", &mSettingsManager->GetHysteresis(), 0.0f, 1.0f);
                ImGui::SliderFloat(
                    "Ray max length",
                    &mSettingsManager->GetProbeMaxRayLength(),
                    0.01f,
                    2000.0f);
                ImGui::SliderFloat(
                    "Ray min length",
                    &mSettingsManager->GetProbeMinRayLength(),
                    0.01f,
                    10.0f);
            }
        }
        ImGui::End();
    }
    ImGui::EndFrame();
}

void UIRenderer::Render(vk::CommandBuffer commandBuffer) {
    Update();

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