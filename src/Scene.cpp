#include "Scene.h"

#include "DebugUtils.h"

#undef MemoryBarrier

namespace VKRT {

Scene::Scene(ScopedRefPtr<Context> context)
    : mContext(context), mObjects() {
    uint64_t dummyData = 0;
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

Scene::SceneMaterials Scene::GetMaterialProxies() {
    // Gather textures first
    std::vector<std::pair<ScopedRefPtr<Texture>, int32_t>> textureIndices;
    textureIndices.push_back({mDummyTexture, 1});
    int32_t currentTextureIndex = 1;
    for (const ScopedRefPtr<Object>& object : mObjects) {
        for (const ScopedRefPtr<Mesh>& mesh : object->GetModel()->GetMeshes()) {
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
    for (const ScopedRefPtr<Object>& object : mObjects) {
        for (const ScopedRefPtr<Mesh>& mesh : object->GetModel()->GetMeshes()) {
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
            materials.push_back(proxy);
        }
    }

    std::vector<ScopedRefPtr<Texture>> textures;
    for (const auto& entry : textureIndices) {
        textures.push_back(entry.first);
    }

    Scene::SceneMaterials sceneMaterials{
        .materials = materials,
        .textures = textures,
    };
    return sceneMaterials;
}

void Scene::Draw(vk::CommandBuffer& commandBuffer) {
    for (const ScopedRefPtr<Object>& object : mObjects) {
        const ScopedRefPtr<Model>& model = object->GetModel();
        for (const ScopedRefPtr<Mesh>& mesh : model->GetMeshes()) {
            ScopedRefPtr<VulkanBuffer> vertexBuffer = mesh->GetVertexBuffer();
            ScopedRefPtr<VulkanBuffer> indexBuffer = mesh->GetIndexBuffer();
            commandBuffer.bindVertexBuffers(0, vertexBuffer->GetBufferHandle(), {0});
            commandBuffer.bindIndexBuffer(
                indexBuffer->GetBufferHandle(),
                {0},
                vk::IndexType::eUint32);
            commandBuffer.drawIndexed(mesh->GetIndexCount() * 3, 1, 0, 0, 0);
        }
    }
}

Scene::~Scene() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
}

}  // namespace VKRT