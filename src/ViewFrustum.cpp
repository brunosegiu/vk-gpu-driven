#include "ViewFrustum.h"

namespace VKRT {

ViewFrustum::ViewFrustum(const glm::mat4& viewTransform) : mPlanes() {
    Update(viewTransform);
}

void ViewFrustum::Update(const glm::mat4& viewTransform) {
    mPlanes[static_cast<uint32_t>(Planes::Left)] = glm::vec4{
        viewTransform[0].w + viewTransform[0].x,
        viewTransform[1].w + viewTransform[1].x,
        viewTransform[2].w + viewTransform[2].x,
        viewTransform[3].w + viewTransform[3].x};

    mPlanes[static_cast<uint32_t>(Planes::Right)] = glm::vec4{
        viewTransform[0].w - viewTransform[0].x,
        viewTransform[1].w - viewTransform[1].x,
        viewTransform[2].w - viewTransform[2].x,
        viewTransform[3].w - viewTransform[3].x};

    mPlanes[static_cast<uint32_t>(Planes::Top)] = glm::vec4{
        viewTransform[0].w - viewTransform[0].y,
        viewTransform[1].w - viewTransform[1].y,
        viewTransform[2].w - viewTransform[2].y,
        viewTransform[3].w - viewTransform[3].y};

    mPlanes[static_cast<uint32_t>(Planes::Bottom)] = glm::vec4{
        viewTransform[0].w + viewTransform[0].y,
        viewTransform[1].w + viewTransform[1].y,
        viewTransform[2].w + viewTransform[2].y,
        viewTransform[3].w + viewTransform[3].y};

    mPlanes[static_cast<uint32_t>(Planes::Back)] = glm::vec4{
        viewTransform[0].w + viewTransform[0].z,
        viewTransform[1].w + viewTransform[1].z,
        viewTransform[2].w + viewTransform[2].z,
        viewTransform[3].w + viewTransform[3].z};

    mPlanes[static_cast<uint32_t>(Planes::Front)] = glm::vec4{
        viewTransform[0].w - viewTransform[0].z,
        viewTransform[1].w - viewTransform[1].z,
        viewTransform[2].w - viewTransform[2].z,
        viewTransform[3].w - viewTransform[3].z};

    for (glm::vec4& plane : mPlanes) {
        const float vectorLength = glm::length(glm::vec3(plane));
        plane = plane / vectorLength;
    }
}

bool ViewFrustum::Test(const glm::mat4& objectTransform, const AABB& boundingBox) const {
    const glm::vec3& max = boundingBox.GetMax();
    const glm::vec3& min = boundingBox.GetMin();
    std::array<glm::vec3, 8> boxPoints{
        glm::vec3{min.x, min.y, min.z},
        glm::vec3{max.x, min.y, min.z},
        glm::vec3{max.x, max.y, min.z},
        glm::vec3{min.x, max.y, min.z},
        glm::vec3{min.x, min.y, max.z},
        glm::vec3{max.x, min.y, max.z},
        glm::vec3{max.x, max.y, max.z},
        glm::vec3{min.x, max.y, max.z}};
    for (glm::vec3& point : boxPoints) {
        point = glm::vec3(objectTransform * glm::vec4(point, 1.0f));
    }
    for (uint32_t planeIndex = 0; planeIndex < 6; ++planeIndex) {
        bool intersects = false;
        const glm::vec4& plane = mPlanes[planeIndex];
        for (const glm::vec3& point : boxPoints) {
            if (glm::dot(glm::vec4(point, 1.0f), plane) > 0) {
                intersects = true;
                break;
            }
        }
        if (!intersects) {
            return false;
        }
    }
    return true;
}

}  // namespace VKRT