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
        GeometryPassOpaqueVertexShader,
        GeometryPassOpaqueFragmentShader,
        GeometryPassAlphaMaskedVertexShader,
        GeometryPassAlphaMaskedFragmentShader,
        TransparentPassVertexShader,
        TransparentPassFragmentShader,
        DepthOnlyOpaqueVertexShader,
        DepthOnlyOpaqueFragmentShader,
        DepthOnlyAlphaMaskedVertexShader,
        DepthOnlyAlphaMaskedFragmentShader,
        VisibilityBufferShadeVertexShader,
        VisibilityBufferShadeFragmentShader,
        SSAOVertexShader,
        SSAOFragmentShader,
        EdgeAwareBoxBlurVertexShader,
        EdgeAwareBoxBlurFragmentShader,
        RaytraceProbeGenShader,
        RaytraceProbeHitShader,
        RaytraceProbeMissShader,
        RaytraceProbeShadowMissShader,
        UpdateProbesShader,
        ProbeVertexShader,
        ProbeFragmentShader,
        ShadowMomentsBlurVerticalVertexShader,
        ShadowMomentsBlurVerticalFragmentShader,
        ShadowMomentsBlurHorizontalVertexShader,
        ShadowMomentsBlurHorizontalFragmentShader,
        GlossyReflectionsGenShader,
        GlossyReflectionsHitShader,
        GlossyReflectionsMissShader,
        BlurReflectionsShader,
        PostProcessingVertexShader,
        PostProcessingFragmentShader,
    };
};

class ResourceLoader {
public:
    static Resource Load(const Resource::Id& resourceId);
    static void CleanUp(Resource& resource);
};

}  // namespace VKRT
