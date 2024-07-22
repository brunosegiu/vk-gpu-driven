#include "Scene.h"

#include "DebugUtils.h"

#undef MemoryBarrier

namespace VKRT {

Scene::Scene(ScopedRefPtr<Context> context) : mContext(context), mObjects() {
    uint64_t dummyData = 0;
    mMeshSystem = new MeshSystem(context);
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
    // Flatten object tree structure
    FlattenedObjects();
    // Create unified geometry buffers
    GetMeshSystem()->Upload();
    // Create material proxies
    GenerateMaterialProxies();
}

void Scene::GenerateMaterialProxies() {
    // Gather textures first
    std::vector<std::pair<ScopedRefPtr<Texture>, int32_t>> textureIndices;
    textureIndices.push_back({mDummyTexture, 1});
    int32_t currentTextureIndex = 1;
    for (const ScopedRefPtr<Object>& object : GetFlattenedObjects()) {
        std::vector<ScopedRefPtr<Mesh>> meshes = object->GetMeshes();
        for (ScopedRefPtr<Mesh>& mesh : meshes) {
            const Material* material = mesh->GetMaterial();
            std::vector<ScopedRefPtr<Texture>> meshTextures{
                material->GetAlbedoTexture(),
                material->GetRoughnessTexture()};
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
    }

    // Gather materials and generate texture indices if applicable
    std::vector<MaterialProxy> materials;
    for (const ScopedRefPtr<Object>& object : GetFlattenedObjects()) {
        std::vector<ScopedRefPtr<Mesh>> meshes = object->GetMeshes();
        for (ScopedRefPtr<Mesh>& mesh : meshes) {
            const ScopedRefPtr<Material> material = mesh->GetMaterial();
            MaterialProxy proxy{
                .albedo = material->GetAlbedo(),
                .emissive = material->GetEmissive(),
                .roughness = material->GetRoughness(),
                .metallic = material->GetMetallic(),
                .transmission = material->GetTransmission(),
                .indexOfRefraction = material->GetIndexOfRefraction(),
                .albedoTextureIndex = -1,
                .roughnessTextureIndex = -1,
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
                const ScopedRefPtr<Texture> roughnessTexture = material->GetRoughnessTexture();
                auto roughnessIt = std::find_if(
                    textureIndices.begin(),
                    textureIndices.end(),
                    [&roughnessTexture](const auto& entry) {
                        return entry.first == roughnessTexture;
                    });
                if (roughnessIt != textureIndices.end()) {
                    proxy.roughnessTextureIndex = roughnessIt->second;
                }
            }
            material->SetMaterialId(materials.size());
            materials.push_back(proxy);
        }
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

void Scene::Draw(
    vk::CommandBuffer& commandBuffer,
    std::function<void(vk::CommandBuffer, ScopedRefPtr<Object>, ScopedRefPtr<Mesh>)> onDrawMesh) {
    for (ScopedRefPtr<Object> object : mObjects) {
        object->UpdateTransforms(glm::mat4(1.0f));
    }
    std::vector<ScopedRefPtr<Object>> objects = GetFlattenedObjects();
    commandBuffer.bindVertexBuffers(0, GetMeshSystem()->GetVertexBuffer()->GetBufferHandle(), {0});
    commandBuffer.bindVertexBuffers(
        1,
        GetMeshSystem()->GetTexCoordBuffer()->GetBufferHandle(),
        {0});
    commandBuffer.bindVertexBuffers(2, GetMeshSystem()->GetNormalBuffer()->GetBufferHandle(), {0});
    commandBuffer.bindIndexBuffer(
        GetMeshSystem()->GetIndexBuffer()->GetBufferHandle(),
        {0},
        vk::IndexType::eUint32);
    for (const ScopedRefPtr<Object>& object : objects) {
        std::vector<ScopedRefPtr<Mesh>> meshes = object->GetMeshes();
        for (ScopedRefPtr<Mesh>& mesh : meshes) {
            onDrawMesh(commandBuffer, object, mesh);
            commandBuffer.drawIndexed(
                mesh->GetIndexCount(),
                1,
                mesh->GetFirstIndex(),
                mesh->GetVertexOffset(),
                0);
        }
    }
}

void Scene::FlattenedObjects() {
    std::function<void(const ScopedRefPtr<Object>&, std::vector<ScopedRefPtr<Object>>&)>
        loadSubtree;
    loadSubtree = [&](const ScopedRefPtr<Object>& object,
                      std::vector<ScopedRefPtr<Object>>& flattened) -> void {
        for (const ScopedRefPtr<Object>& child : object->GetChildren()) {
            flattened.push_back(child);
            loadSubtree(child, flattened);
        }
    };

    mCachedFlattenedObjects.clear();
    for (ScopedRefPtr<Object> object : mObjects) {
        loadSubtree(object, mCachedFlattenedObjects);
    }
}

Scene::~Scene() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
}

}  // namespace VKRT