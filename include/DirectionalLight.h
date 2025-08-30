#pragma once

#include <vector>

#include "Camera.h"
#include "MeshSystem.h"
#include "Object.h"
#include "RefCountPtr.h"
#include "VulkanBase.h"
#include "VulkanBuffer.h"

namespace VKRT {

class Context;

class DirectionalLight : public RefCountPtr {
public:
    DirectionalLight();

    glm::mat4 ComputeShadowMatrix(
        const float frustumWidth,
        const float shadowDistance,
        const float near,
        const float far);

    const glm::vec3& GetRadiance() const { return mRadiance; }
    const glm::vec3& GetDirection() const { return mDirection; }

    void SetRadiance(glm::vec3 radiance) { mRadiance = radiance; }
    void SetDirection(const glm::vec3& direction) { mDirection = direction; }

    ~DirectionalLight();

private:
    glm::vec3 mRadiance;
    glm::vec3 mDirection;
};
}  // namespace VKRT