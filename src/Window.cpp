#include "Window.h"

#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>

#include "Context.h"
#include "Device.h"
#include "Instance.h"

namespace VKRT {

ResultValue<ScopedRefPtr<Window>> Window::Create(uint32_t width, uint32_t height) {
    return {Result::Success, new Window(width, height)};
}

Window::Window(uint32_t width, uint32_t height) : mNativeHandle(nullptr), mContext(nullptr) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    mNativeHandle = glfwCreateWindow(width, height, "VKRT", nullptr, nullptr);

    mInputManager = new InputManager(this);

    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOther(mNativeHandle, true);
    }
}

bool Window::Update() {
    return mInputManager->Update();
}

std::vector<std::string> Window::GetRequiredVulkanExtensions() {
    uint32_t requiredExtensionCount = 0;
    const char** requiredExtensionNames = nullptr;
    requiredExtensionNames = glfwGetRequiredInstanceExtensions(&requiredExtensionCount);
    std::vector<std::string> result;
    result.reserve(requiredExtensionCount);
    for (int extIndex = 0; extIndex < requiredExtensionCount; ++extIndex) {
        result.push_back(requiredExtensionNames[extIndex]);
    }
    return result;
}

Window::Size2D Window::GetSize() {
    Size2D size;
    glfwGetFramebufferSize(
        mNativeHandle,
        reinterpret_cast<int*>(&size.width),
        reinterpret_cast<int*>(&size.height));
    return size;
}

ResultValue<ScopedRefPtr<Context>> Window::CreateContext() {
    if (mContext == nullptr) {
        auto [instanceResult, instance] = Instance::Create(this);
        if (instanceResult == Result::Success) {
            vk::SurfaceKHR surface = instance->CreateSurface(this);
            auto [deviceResult, device] = Device::Create(instance, surface);
            if (deviceResult == Result::Success) {
                mContext = new Context(this, instance, surface, device);
                return {Result::Success, mContext};
            } else {
                return {deviceResult, nullptr};
            }
        }
        return {instanceResult, nullptr};
    }
    return {Result::Success, mContext};
}

void Window::DestroyContext() {
    mContext->Destroy();
    mContext = nullptr;
}

Window::~Window() {
    {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    if (mNativeHandle != nullptr) {
        glfwDestroyWindow(mNativeHandle);
    }
    glfwTerminate();
}
}  // namespace VKRT
