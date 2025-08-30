#include "DirectionalLight.h"

#include "DebugUtils.h"

#undef MemoryBarrier

namespace VKRT {

DirectionalLight::DirectionalLight() : mRadiance(10.0f), mDirection(0.0f, -1.0f, 0.0f) {}

glm::mat4 DirectionalLight::ComputeShadowMatrix(
    const float frustumWidth,
    const float shadowDistance,
    const float _near,
    const float _far) {
    glm::vec3 shadowPos = -mDirection * shadowDistance;
    glm::mat4 projection =
        glm::ortho<float>(-frustumWidth, frustumWidth, -frustumWidth, frustumWidth, _near, _far);
    projection[1][1] *= -1.0f;

    return projection * glm::lookAt(
                            shadowPos,
                            shadowPos + mDirection * shadowDistance,
                            glm::vec3(0.0f, 1.0f, 0.0f));
}

DirectionalLight::~DirectionalLight() {}

}  // namespace VKRT