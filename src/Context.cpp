#include "Context.h"

#include <GLFW/glfw3.h>

#include "DebugUtils.h"

namespace VKRT {

Context::Context(
    ScopedRefPtr<Window> window,
    ScopedRefPtr<Instance> instance,
    vk::SurfaceKHR surface,
    ScopedRefPtr<Device> device) {
    mWindow = window;
    mInstance = instance;
    mSurface = surface;
    mDevice = device;
    mDevice->SetContext(this);
    mSwapchain = new Swapchain(this);
}

void Context::BeginMarker(const vk::CommandBuffer& commandBuffer, const std::string& name) {
#ifdef VKRT_ENABLE_VALIDATION
    commandBuffer.beginDebugUtilsLabelEXT(
        vk::DebugUtilsLabelEXT().setPLabelName(name.c_str()),
        GetDevice()->GetDispatcher());
#endif
}

void Context::EndMarker(const vk::CommandBuffer& commandBuffer) {
#ifdef VKRT_ENABLE_VALIDATION
    commandBuffer.endDebugUtilsLabelEXT(GetDevice()->GetDispatcher());
#endif
}

void Context::Destroy() {
    VKRT_ASSERT_VK(mDevice->GetLogicalDevice().waitIdle());
    mSwapchain = nullptr;
    mInstance->DestroySurface(mSurface);
    mDevice = nullptr;
    mInstance = nullptr;
    mWindow = nullptr;
}

Context::~Context() {}

}  // namespace VKRT