#include "ResourceLoader.h"

#include <algorithm>
#include <string>
#include <unordered_map>

#ifdef VKRT_PLATFORM_WINDOWS
#include <Windows.h>
#include "ShaderResources.h"
#endif

#ifdef VKRT_PLATFORM_LINUX
extern "C" {
#include "incbin.h"
}

namespace VKRT {
INCBIN(VertexShader, "uberShader.vert.spv");
INCBIN(FragmentShader, "uberShader.frag.spv");
}  // namespace VKRT
#endif

namespace VKRT {

#ifdef VKRT_PLATFORM_WINDOWS
Resource ResourceLoader::Load(const Resource::Id& resourceId) {
    static const std::unordered_map<Resource::Id, uint32_t> resourceTranslateTable{
        {Resource::Id::CullingShader, VKRT_RESOURCE_CULLING_SHADER},
        {Resource::Id::GeometryPassOpaqueVertexShader, VKRT_RESOURCE_GEOMETRY_PASS_OPAQUE_VERTEX},
        {Resource::Id::GeometryPassOpaqueFragmentShader,
         VKRT_RESOURCE_GEOMETRY_PASS_OPAQUE_FRAGMENT},
        {Resource::Id::GeometryPassAlphaMaskedVertexShader,
         VKRT_RESOURCE_GEOMETRY_PASS_ALPHA_MASKED_VERTEX},
        {Resource::Id::GeometryPassAlphaMaskedFragmentShader,
         VKRT_RESOURCE_GEOMETRY_PASS_ALPHA_MASKED_FRAGMENT},
        {Resource::Id::TransparentPassVertexShader, VKRT_RESOURCE_TRANSPARENT_VERTEX},
        {Resource::Id::TransparentPassFragmentShader, VKRT_RESOURCE_TRANSPARENT_FRAGMENT},
        {Resource::Id::DepthOnlyOpaqueVertexShader, VKRT_RESOURCE_DEPTH_ONLY_OPAQUE_VERTEX_SHADER},
        {Resource::Id::DepthOnlyOpaqueFragmentShader,
         VKRT_RESOURCE_DEPTH_ONLY_OPAQUE_FRAGMENT_SHADER},
        {Resource::Id::DepthOnlyAlphaMaskedVertexShader,
         VKRT_RESOURCE_DEPTH_ONLY_ALPHA_MASKED_VERTEX_SHADER},
        {Resource::Id::DepthOnlyAlphaMaskedFragmentShader,
         VKRT_RESOURCE_DEPTH_ONLY_ALPHA_MASKED_FRAGMENT_SHADER},
        {Resource::Id::VisibilityBufferShadeVertexShader,
         VKRT_RESOURCE_VISIBILITY_BUFFER_SHADE_VERTEX_SHADER},
        {Resource::Id::VisibilityBufferShadeFragmentShader,
         VKRT_RESOURCE_VISIBILITY_BUFFER_SHADE_FRAGMENT_SHADER},
        {Resource::Id::SSAOVertexShader, VKRT_RESOURCE_SSAO_VERTEX_SHADER},
        {Resource::Id::SSAOFragmentShader, VKRT_RESOURCE_SSAO_FRAGMENT_SHADER},
        {Resource::Id::EdgeAwareBoxBlurVertexShader,
         VKRT_RESOURCE_EDGE_AWARE_BOX_BLUR_VERTEX_SHADER},
        {Resource::Id::EdgeAwareBoxBlurFragmentShader,
         VKRT_RESOURCE_EDGE_AWARE_BOX_BLUR_FRAGMENT_SHADER},
        {Resource::Id::RaytraceProbeGenShader, VKRT_RESOURCE_RAYTRACE_PROBE_GEN_SHADER},
        {Resource::Id::RaytraceProbeHitShader, VKRT_RESOURCE_RAYTRACE_PROBE_HIT_SHADER},
        {Resource::Id::RaytraceProbeMissShader, VKRT_RESOURCE_RAYTRACE_PROBE_MISS_SHADER},
        {Resource::Id::RaytraceProbeShadowMissShader,
         VKRT_RESOURCE_RAYTRACE_PROBE_SHADOW_MISS_SHADER},
        {Resource::Id::UpdateProbesShader, VKRT_RESOURCE_UPDATE_PROBES_SHADER},
        {Resource::Id::ProbeVertexShader, VKRT_RESOURCE_PROBE_VERTEX},
        {Resource::Id::ProbeFragmentShader, VKRT_RESOURCE_PROBE_FRAGMENT},
        {Resource::Id::ShadowMomentsBlurVerticalVertexShader,
         VKRT_RESOURCE_SHADOW_MOMENTS_BLUR_VERTICAL_VERTEX},
        {Resource::Id::ShadowMomentsBlurVerticalFragmentShader,
         VKRT_RESOURCE_SHADOW_MOMENTS_BLUR_VERTICAL_FRAGMENT},
        {Resource::Id::ShadowMomentsBlurHorizontalVertexShader,
         VKRT_RESOURCE_SHADOW_MOMENTS_BLUR_HORIZONTAL_VERTEX},
        {Resource::Id::ShadowMomentsBlurHorizontalFragmentShader,
         VKRT_RESOURCE_SHADOW_MOMENTS_BLUR_HORIZONTAL_FRAGMENT},
        {Resource::Id::GlossyReflectionsGenShader, VKRT_RESOURCE_GLOSSY_REFLECTIONS_GEN_SHADER},
        {Resource::Id::GlossyReflectionsHitShader, VKRT_RESOURCE_GLOSSY_REFLECTIONS_HIT_SHADER},
        {Resource::Id::GlossyReflectionsMissShader, VKRT_RESOURCE_GLOSSY_REFLECTIONS_MISS_SHADER},
        {Resource::Id::BlurReflectionsShader, VKRT_RESOURCE_BLUR_REFLECTIONS_SHADER},
        {Resource::Id::PostProcessingVertexShader, VKRT_RESOURCE_POST_PROCESSING_VERTEX},
        {Resource::Id::PostProcessingFragmentShader, VKRT_RESOURCE_POST_PROCESSING_FRAGMENT},
    };

    HMODULE module = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&ResourceLoader::Load,
        &module);
    uint32_t actualResourceId = resourceTranslateTable.at(resourceId);
    HRSRC resource = FindResource(module, MAKEINTRESOURCE(actualResourceId), RT_RCDATA);
    if (resource != nullptr) {
        size_t bufferSize = SizeofResource(module, resource);
        HGLOBAL data = LoadResource(module, resource);
        if (data != nullptr && bufferSize != 0) {
            auto resourceBuffer = reinterpret_cast<uint8_t*>(LockResource(data));
            auto buffer = new uint8_t[bufferSize];
            std::copy_n(resourceBuffer, bufferSize, buffer);
            return {buffer, bufferSize};
        }
    }
    return {nullptr, 0};
}
#endif

#ifdef VKRT_PLATFORM_LINUX
Resource ResourceLoader::Load(const Resource::Id& resourceId) {
    switch (resourceId) {
        case Resource::Id::VertexShader: {
            return Resource{.buffer = gVertexShaderData, .size = gVertexShaderSize};
        } break;
        case Resource::Id::FragmentShader: {
            return Resource{.buffer = gFragmentShaderData, .size = gFragmentShaderSize};
        } break;
        default:
            return {nullptr, 0};
    }
}
#endif

void ResourceLoader::CleanUp(Resource& resource) {
#ifndef VKRT_PLATFORM_LINUX
    delete[] resource.buffer;
#endif
    resource.buffer = nullptr;
    resource.size = 0;
}

}  // namespace VKRT
