#include "Utils.h"

#include <glm/gtx/rotate_vector.hpp>

namespace VKRT {

Utils::Geometry Utils::BuildSphere(uint32_t slices, uint32_t stacks) {
    Utils::Geometry geometry;
    const float radius = 1.0f;

    const uint32_t interiorRings = stacks - 1;
    const uint32_t interiorVerts = interiorRings * slices;
    const uint32_t topIndex = 0;
    const uint32_t firstRing = 1;
    const uint32_t bottomIndex = firstRing + interiorVerts;

    geometry.positions.reserve(2 + interiorVerts);
    geometry.indices.reserve((slices * 2u * (stacks - 1)) * 3u);

    geometry.positions.emplace_back(0.0f, radius, 0.0f);

    for (uint32_t r = 1; r <= interiorRings; ++r) {
        const float theta = glm::pi<float>() * float(r) / float(stacks);  // (0..pi)
        const float sinT = std::sin(theta);
        const float cosT = std::cos(theta);

        for (uint32_t s = 0; s < slices; ++s) {
            const float phi = 2.0f * glm::pi<float>() * float(s) / float(slices);
            const float cosP = std::cos(phi);
            const float sinP = std::sin(phi);
            geometry.positions.emplace_back(
                radius * sinT * cosP,
                radius * cosT,
                radius * sinT * sinP);
        }
    }

    geometry.positions.emplace_back(0.0f, -radius, 0.0f);

    if (interiorRings > 0) {
        const uint32_t ringStart = firstRing;
        for (uint32_t s = 0; s < slices; ++s) {
            const uint32_t a = ringStart + s;
            const uint32_t b = ringStart + ((s + 1) % slices);
            geometry.indices.push_back(topIndex);
            geometry.indices.push_back(a);
            geometry.indices.push_back(b);
        }
    }

    for (uint32_t r = 0; r + 1 < interiorRings; ++r) {
        const uint32_t curr = firstRing + r * slices;
        const uint32_t next = curr + slices;

        for (uint32_t s = 0; s < slices; ++s) {
            const uint32_t s0 = s;
            const uint32_t s1 = (s + 1) % slices;

            const uint32_t a = curr + s0;
            const uint32_t b = curr + s1;
            const uint32_t c = next + s0;
            const uint32_t d = next + s1;

            geometry.indices.push_back(a);
            geometry.indices.push_back(c);
            geometry.indices.push_back(b);

            geometry.indices.push_back(b);
            geometry.indices.push_back(c);
            geometry.indices.push_back(d);
        }
    }

    if (interiorRings > 0) {
        const uint32_t lastRing = firstRing + (interiorRings - 1) * slices;
        for (uint32_t s = 0; s < slices; ++s) {
            const uint32_t a = lastRing + s;
            const uint32_t b = lastRing + ((s + 1) % slices);
            geometry.indices.push_back(a);
            geometry.indices.push_back(bottomIndex);
            geometry.indices.push_back(b);
        }
    }

    return geometry;
}

}  // namespace VKRT