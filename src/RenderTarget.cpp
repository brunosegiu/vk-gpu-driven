#include "RenderTarget.h"

#include "DebugUtils.h"
#include "Device.h"
#include "Texture.h"
#include "VulkanBuffer.h"

namespace VKRT {

RenderTarget::RenderTarget(
    ScopedRefPtr<Context> context,
    const std::vector<ScopedRefPtr<Texture>>& renderTargets)
    : mContext(context), mTextures(renderTargets) {}

RenderTarget::RenderTarget(ScopedRefPtr<Context> context, const ScopedRefPtr<Texture>& renderTarget)
    : RenderTarget(context, std::vector<ScopedRefPtr<Texture>>{renderTarget}) {}

const std::vector<vk::ImageView> RenderTarget::GetImageViews() const {
    std::vector<vk::ImageView> imageViews;
    for (const ScopedRefPtr<Texture>& texture : mTextures) {
        // TODO: Support binding individual mips for rendering
        imageViews.emplace_back(texture->GetImageView(0));
    }
    return imageViews;
}

RenderTarget::~RenderTarget() {}

}  // namespace VKRT