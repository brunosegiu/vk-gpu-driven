#pragma once

#include <array>
#include <glm/glm.hpp>

#include "AABB.h"

namespace VKRT {
class ViewFrustum {
public:
    ViewFrustum(const glm::mat4& viewTransform);

    void Update(const glm::mat4& viewTransform);

    bool Test(const glm::mat4& objectTransform, const AABB& boundingBox) const;

    const std::array<glm::vec4, 6>& GetPlanes() const { return mPlanes; }

private:
    enum class Planes { Left, Right, Top, Bottom, Back, Front };
    std::array<glm::vec4, 6> mPlanes;
};
}  // namespace VKRT
