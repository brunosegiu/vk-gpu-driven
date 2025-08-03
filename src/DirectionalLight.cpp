#include "DirectionalLight.h"

#include "DebugUtils.h"

#undef MemoryBarrier

namespace VKRT {

DirectionalLight::DirectionalLight()
    : mRadiance(10.0f),
      mDirection(0.0f, -1.0f, 0.0f),
      mFrustumWidth(50.0f),
      mDistance(500.0f),
      mNear(1.0f),
      mFar(1500.0f) {}

glm::mat4 DirectionalLight::ComputeShadowMatrix() {
    glm::vec3 shadowPos = -mDirection * mDistance;
    glm::mat4 projection = glm::ortho<float>(
        -mFrustumWidth,
        mFrustumWidth,
        -mFrustumWidth,
        mFrustumWidth,
        mNear,
        mFar);
    projection[1][1] *= -1.0f;

    return projection *
           glm::lookAt(shadowPos, shadowPos + mDirection * mDistance, glm::vec3(0.0f, 1.0f, 0.0f));
}

DirectionalLight::~DirectionalLight() {}

}  // namespace VKRT