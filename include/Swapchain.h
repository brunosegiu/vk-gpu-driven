#pragma once
#include <string>
#include <vector>

#include "RefCountPtr.h"
#include "Result.h"
#include "VulkanBase.h"

namespace VKRT {

class Context;
class Texture;

class Swapchain : public RefCountPtr {
public:
    Swapchain(ScopedRefPtr<Context> context);

    vk::Format GetFormat() { return mFormat; }
    vk::Extent2D GetExtent() { return mExtent; }

    uint32_t AcquireNextImage();
    void Present();

    uint32_t GetCurrentIndex() { return mCurrentImageIndex; }

    vk::Semaphore& GetPresentSemaphore() { return mPresentSemaphore; }
    vk::Semaphore& GetRenderSemaphore() { return mRenderSemaphore; }

    const std::vector<ScopedRefPtr<Texture>>& GetRenderTargets() const { return mImages; }
    const uint32_t GetImageCount() const { return static_cast<uint32_t>(mImages.size()); }

    ~Swapchain();

private:
    ScopedRefPtr<Context> mContext;
    vk::SwapchainKHR mSwapchainHandle;
    vk::Format mFormat;
    vk::Extent2D mExtent;
    std::vector<ScopedRefPtr<Texture>> mImages;
    vk::Semaphore mPresentSemaphore;
    vk::Semaphore mRenderSemaphore;
    uint32_t mCurrentImageIndex;
};
}  // namespace VKRT
