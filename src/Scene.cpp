#include "Scene.h"

#include "DebugUtils.h"

#undef MemoryBarrier

namespace VKRT {

Scene::Scene(ScopedRefPtr<Context> context) : mContext(context), mLocked(false), mObjects() {
    uint64_t dummyData = 0;
    mMeshSystem = new MeshSystem(mContext);
    mDummyTexture = new Texture(
        context,
        1,
        1,
        vk::Format::eR8G8B8A8Unorm,
        reinterpret_cast<uint8_t*>(&dummyData),
        4);
}

void Scene::AddObject(ScopedRefPtr<Object> object) {
    if (object != nullptr) {
        mObjects.emplace_back(object);
    }
}

void Scene::Lock() {
    // Create unified geometry buffers
    GetMeshSystem()->Upload();
    // Flatten object tree structure
    PackDrawData();
    // Create material proxies
    GenerateMaterialProxies();
    mLocked = true;
}

void Scene::GenerateMaterialProxies() {
    // Gather textures first
    std::vector<std::pair<ScopedRefPtr<Texture>, int32_t>> textureIndices;
    textureIndices.push_back({mDummyTexture, 1});
    int32_t currentTextureIndex = 1;
    for (ScopedRefPtr<Mesh>& mesh : mFlattenedMeshes) {
        const Material* material = mesh->GetMaterial();
        std::vector<ScopedRefPtr<Texture>> meshTextures{
            material->GetAlbedoTexture(),
            material->GetMetallicRoughnessTexture(),
            material->GetNormalTexture()};
        for (const ScopedRefPtr<Texture>& texture : meshTextures) {
            if (texture != nullptr) {
                auto it = std::find_if(
                    textureIndices.begin(),
                    textureIndices.end(),
                    [&texture](const auto& entry) { return entry.first == texture; });

                if (it == textureIndices.end()) {
                    textureIndices.emplace_back(texture, currentTextureIndex);
                    ++currentTextureIndex;
                }
            }
        }
    }

    // Gather materials and generate texture indices if applicable
    std::vector<MaterialProxy> materials;
    for (ScopedRefPtr<Mesh>& mesh : mFlattenedMeshes) {
        const ScopedRefPtr<Material> material = mesh->GetMaterial();
        MaterialProxy proxy{
            .albedo = material->GetAlbedo(),
            .roughness = material->GetRoughness(),
            .metallic = material->GetMetallic(),
            .albedoTextureIndex = -1,
            .metallicRoughnessTextureIndex = -1,
            .normalTextureIndex = -1,
        };
        {
            const ScopedRefPtr<Texture> albedoTexture = material->GetAlbedoTexture();
            auto albedoIt = std::find_if(
                textureIndices.begin(),
                textureIndices.end(),
                [&albedoTexture](const auto& entry) { return entry.first == albedoTexture; });
            if (albedoIt != textureIndices.end()) {
                proxy.albedoTextureIndex = albedoIt->second;
            }
        }
        {
            const ScopedRefPtr<Texture> roughnessTexture = material->GetMetallicRoughnessTexture();
            auto roughnessIt = std::find_if(
                textureIndices.begin(),
                textureIndices.end(),
                [&roughnessTexture](const auto& entry) { return entry.first == roughnessTexture; });
            if (roughnessIt != textureIndices.end()) {
                proxy.metallicRoughnessTextureIndex = roughnessIt->second;
            }
        }
        {
            const ScopedRefPtr<Texture> normalTexture = material->GetNormalTexture();
            auto normalIt = std::find_if(
                textureIndices.begin(),
                textureIndices.end(),
                [&normalTexture](const auto& entry) { return entry.first == normalTexture; });
            if (normalIt != textureIndices.end()) {
                proxy.normalTextureIndex = normalIt->second;
            }
        }
        material->SetMaterialId(materials.size());
        materials.push_back(proxy);
    }

    std::vector<ScopedRefPtr<Texture>> textures;
    for (const auto& entry : textureIndices) {
        textures.push_back(entry.first);
    }

    mCachedMaterialProxies = SceneMaterials{
        .materials = materials,
        .textures = textures,
    };
}

void Scene::Update() {
    for (ScopedRefPtr<Object> object : mObjects) {
        object->UpdateTransforms(glm::mat4(1.0f));
    }
}

void Scene::PackDrawData() {
    // Flatten scene into a single list
    std::function<void(const ScopedRefPtr<Object>&, std::vector<DrawData>&)> loadSubtree =
        [&](const ScopedRefPtr<Object>& object, std::vector<DrawData>& flattened) -> void {
        for (const ScopedRefPtr<Object>& child : object->GetChildren()) {
            std::vector<ScopedRefPtr<Mesh>> meshes = child->GetMeshes();
            for (ScopedRefPtr<Mesh>& mesh : meshes) {
                DrawData meshParameters{
                    .indexCount = mesh->GetIndexCount(),
                    .firstIndex = mesh->GetFirstIndex(),
                    .vertexOffset = static_cast<int32_t>(mesh->GetVertexOffset()),
                    .transform = child->GetAbsoluteTransform(),
                    .materialId = mesh->GetMaterial()->GetMaterialId(),
                    .normalTransform = glm::mat4(
                        glm::transpose(glm::inverse(glm::mat3(child->GetAbsoluteTransform())))),
                    .alphaMode = static_cast<uint32_t>(mesh->GetMaterial()->GetAlphaMode()),
                    .aabb = {
                        .minBounds = mesh->GetAABB().GetMin(),
                        .maxBounds = mesh->GetAABB().GetMax()}};
                flattened.push_back(meshParameters);
                mFlattenedMeshes.push_back(mesh);
            }
            loadSubtree(child, flattened);
        }
    };

    mPackedDrawData.clear();
    mFlattenedMeshes.clear();
    for (ScopedRefPtr<Object> object : mObjects) {
        loadSubtree(object, mPackedDrawData);
    }

    // Split per-material type
    std::sort(
        mPackedDrawData.begin(),
        mPackedDrawData.end(),
        [](const DrawData& a, const DrawData& b) {
            return static_cast<int>(a.alphaMode) < static_cast<int>(b.alphaMode);
        });

    for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
        mRenderPassResources[alphaMode].cachedDrawCallCount = 0;
        mRenderPassResources[alphaMode].cachedDrawOffset = 0;
    }

    for (const DrawData& drawData : mPackedDrawData) {
        mRenderPassResources[static_cast<Material::AlphaMode>(drawData.alphaMode)]
            .cachedDrawCallCount += 1;
    }

    for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
        uint32_t alphaModeIndex = static_cast<uint32_t>(alphaMode);
        if (static_cast<uint32_t>(alphaMode) > 0) {
            Material::AlphaMode previousAlphaMode =
                static_cast<Material::AlphaMode>(alphaModeIndex - 1);
            mRenderPassResources[alphaMode].cachedDrawOffset =
                mRenderPassResources[previousAlphaMode].cachedDrawOffset +
                mRenderPassResources[previousAlphaMode].cachedDrawCallCount;
        }
    }
}

Scene::~Scene() {}

}  // namespace VKRT