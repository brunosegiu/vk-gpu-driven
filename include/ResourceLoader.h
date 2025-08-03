#pragma once

#include <cstddef>
#include <cstdint>

#include "Macros.h"

namespace VKRT {

struct Resource {
    const uint8_t* buffer;
    size_t size;

    enum class Id {
        CullingShader,
        CompactionShader,
        UberShaderOpaqueVertexShader,
        UberShaderOpaqueFragmentShader,
        UberShaderAlphaMaskedVertexShader,
        UberShaderAlphaMaskedFragmentShader,
        DepthOnlyOpaqueVertexShader,
        DepthOnlyOpaqueFragmentShader,
        DepthOnlyAlphaMaskedVertexShader,
        DepthOnlyAlphaMaskedFragmentShader,
    };
};

class ResourceLoader {
public:
    static Resource Load(const Resource::Id& resourceId);
    static void CleanUp(Resource& resource);
};

}  // namespace VKRT
