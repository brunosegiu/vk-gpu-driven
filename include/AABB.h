#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace VKRT {
class AABB {
public:
    AABB(const std::vector<glm::vec3>& vertices = {});

    const glm::vec3& GetMax() const { return mMaxBounds; }
    const glm::vec3& GetMin() const { return mMinBounds; }

private:
    glm::vec3 mMaxBounds;
    glm::vec3 mMinBounds;
};
}  // namespace VKRT