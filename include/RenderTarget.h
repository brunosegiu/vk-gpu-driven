#pragma once

#include <vulkan/vulkan.hpp>

#include "Texture.h"

namespace VKRT {
class RenderTarget : public RefCountPtr {
public:
    RenderTarget(ScopedRefPtr<Context> context, const std::vector<ScopedRefPtr<Texture>>& textures);

    RenderTarget(ScopedRefPtr<Context> context, const ScopedRefPtr<Texture>& texture);

    const vk::Format& GetFormat() const { return mTextures.front()->GetFormat(); }
    const vk::ImageAspectFlagBits& GetImageAspect() const {
        return mTextures.front()->GetImageAspect();
    }
    const uint32_t& GetWidth() const { return mTextures.front()->GetWidth(); }
    const uint32_t& GetHeight() const { return mTextures.front()->GetHeight(); }
    const std::vector<vk::ImageView> GetImageViews() const;

    ~RenderTarget();

private:
    ScopedRefPtr<Context> mContext;

    std::vector<ScopedRefPtr<Texture>> mTextures;
};
}  // namespace VKRT
