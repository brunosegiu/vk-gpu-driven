#include "DirectionalLight.h"

#include "DebugUtils.h"

#undef MemoryBarrier

namespace VKRT {

DirectionalLight::DirectionalLight() : mRadiance(10.0f), mDirection(0.0f, -1.0f, 0.0f) {}

glm::mat4 DirectionalLight::ComputeShadowProjection(
    const float frustumWidth,
    const float _near,
    const float _far) {
    glm::mat4 projection =
        glm::ortho<float>(-frustumWidth, frustumWidth, -frustumWidth, frustumWidth, _near, _far);
    projection[1][1] *= -1.0f;

    return projection;
}

glm::mat4 DirectionalLight::ComputeShadowView(
    const glm::vec3& cameraOrigin,
    const float shadowDistance) {
    glm::vec3 center = cameraOrigin + mDirection * (shadowDistance * 0.5f);
    glm::vec3 eye = center - mDirection * shadowDistance;
    return glm::lookAt(eye, center, glm::vec3(0.0f, 1.0f, 0.0f));
}

DirectionalLight::~DirectionalLight() {}

}  // namespace VKRT