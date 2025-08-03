#include "ResourceLoader.h"

#include <algorithm>
#include <string>

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
    uint32_t actualId = 0;
    switch (resourceId) {
        case Resource::Id::CullingShader:
            actualId = VKRT_RESOURCE_CULLING_SHADER;
            break;
        case Resource::Id::CompactionShader:
            actualId = VKRT_RESOURCE_COMPACTION_SHADER;
            break;
        case Resource::Id::UberShaderOpaqueVertexShader:
            actualId = VKRT_RESOURCE_UBER_SHADER_OPAQUE_VERTEX;
            break;
        case Resource::Id::UberShaderOpaqueFragmentShader:
            actualId = VKRT_RESOURCE_UBER_SHADER_OPAQUE_FRAGMENT;
            break;
        case Resource::Id::UberShaderAlphaMaskedVertexShader:
            actualId = VKRT_RESOURCE_UBER_SHADER_ALPHA_MASKED_VERTEX;
            break;
        case Resource::Id::UberShaderAlphaMaskedFragmentShader:
            actualId = VKRT_RESOURCE_UBER_SHADER_ALPHA_MASKED_FRAGMENT;
            break;
        case Resource::Id::DepthOnlyOpaqueVertexShader:
            actualId = VKRT_RESOURCE_DEPTH_ONLY_OPAQUE_VERTEX_SHADER;
            break;
        case Resource::Id::DepthOnlyOpaqueFragmentShader:
            actualId = VKRT_RESOURCE_DEPTH_ONLY_OPAQUE_FRAGMENT_SHADER;
            break;
        case Resource::Id::DepthOnlyAlphaMaskedVertexShader:
            actualId = VKRT_RESOURCE_DEPTH_ONLY_ALPHA_MASKED_VERTEX_SHADER;
            break;
        case Resource::Id::DepthOnlyAlphaMaskedFragmentShader:
            actualId = VKRT_RESOURCE_DEPTH_ONLY_ALPHA_MASKED_FRAGMENT_SHADER;
            break;
        default:
            return {nullptr, 0};
    }

    HMODULE module = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&ResourceLoader::Load,
        &module);
    HRSRC resource = FindResource(module, MAKEINTRESOURCE(actualId), RT_RCDATA);
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
