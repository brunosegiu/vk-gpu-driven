#include "Scene.h"

#include "DebugUtils.h"

#include "../baker/include/BakedSceneSerialization.h"

#undef MemoryBarrier

namespace VKRT {

Scene::Scene(ScopedRefPtr<Context> context) : mContext(context), mObjects() {
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

void Scene::Load(std::string path) {
    VKRTBaker::BakedFile fileIn;
    std::ifstream ifs(path, std::ios::binary);
    fileIn.deserialize(ifs);
    ifs.close();

    const auto toVec3 = [](const VKRTBaker::Vec3& a) -> glm::vec3 {
        return glm::vec3(a.x, a.y, a.z);
    };

    // Upload textures to GPU
    for (VKRTBaker::Texture& unpackedTexture : fileIn.textures) {
        ScopedRefPtr<Texture> texture = new Texture(
            mContext,
            unpackedTexture.width,
            unpackedTexture.height,
            vk::Format::eR8G8B8A8Unorm,
            reinterpret_cast<const uint8_t*>(unpackedTexture.data.data()),
            unpackedTexture.data.size() * sizeof(unpackedTexture.data[0]));
        mSceneMaterials.textures.push_back(texture);
    }

    if (fileIn.textures.empty()) {
        mSceneMaterials.textures.push_back(mDummyTexture);
    }

    // Create material proxies
    for (VKRTBaker::Material& unpackedMaterial : fileIn.materials) {
        ScopedRefPtr<Texture> albedoTexture =
            unpackedMaterial.albedoTextureIndex >= 0
                ? mSceneMaterials.textures[unpackedMaterial.albedoTextureIndex]
                : nullptr;
        ScopedRefPtr<Texture> metallicRoughnessTexture =
            unpackedMaterial.metallicRoughnessTextureIndex >= 0
                ? mSceneMaterials.textures[unpackedMaterial.metallicRoughnessTextureIndex]
                : nullptr;
        ScopedRefPtr<Texture> normalTexture =
            unpackedMaterial.normalTextureIndex >= 0
                ? mSceneMaterials.textures[unpackedMaterial.normalTextureIndex]
                : nullptr;

        ScopedRefPtr<Material> material = new Material(
            static_cast<Material::AlphaMode>(unpackedMaterial.materialType),
            toVec3(unpackedMaterial.albedo),
            unpackedMaterial.roughness,
            unpackedMaterial.metallic,
            albedoTexture,
            metallicRoughnessTexture,
            normalTexture);
        material->SetMaterialId(mMaterials.size());
        mMaterials.push_back(material);

        MaterialProxy proxy{
            .albedo = material->GetAlbedo(),
            .roughness = material->GetRoughness(),
            .metallic = material->GetMetallic(),
            .albedoTextureIndex = unpackedMaterial.albedoTextureIndex,
            .metallicRoughnessTextureIndex = unpackedMaterial.metallicRoughnessTextureIndex,
            .normalTextureIndex = unpackedMaterial.normalTextureIndex,
        };

        mSceneMaterials.materials.push_back(proxy);
    }

    for (const VKRTBaker::Mesh& unpackedMesh : fileIn.meshes) {
        std::vector<Meshlet> meshlets;
        for (const uint32_t& unpackedMeshletIndex : unpackedMesh.meshlets) {
            VKRTBaker::Meshlet unpackedMeshlet = fileIn.meshlets[unpackedMeshletIndex];
            meshlets.push_back({
                .vertexOffset = unpackedMeshlet.vertexOffset,
                .indexOffset = unpackedMeshlet.indexOffset,
                .indexCount = unpackedMeshlet.indexCount,
                .minBounds = toVec3(unpackedMeshlet.minBounds),
                .maxBounds = toVec3(unpackedMeshlet.maxBounds),
                .coneApex = toVec3(unpackedMeshlet.coneApex),
                .coneAxis = toVec3(unpackedMeshlet.coneAxis),
                .coneCutoff = unpackedMeshlet.coneCutoff,
            });
        }
        ScopedRefPtr<Mesh> mesh = new Mesh(mMaterials[unpackedMesh.material], meshlets);
        mMeshes.push_back(mesh);
    }

    for (const VKRTBaker::Object& unpackedObject : fileIn.objects) {
        ScopedRefPtr<Object> object = new Object();
        mFlatObjects.push_back(object);

        glm::vec3 translation(
            unpackedObject.translation.x,
            unpackedObject.translation.y,
            unpackedObject.translation.z);
        object->SetTranslation(translation);

        glm::vec3 scale(unpackedObject.scale.x, unpackedObject.scale.y, unpackedObject.scale.z);
        object->SetScale(scale);

        glm::quat rotation(
            unpackedObject.rotation.w,
            unpackedObject.rotation.x,
            unpackedObject.rotation.y,
            unpackedObject.rotation.z);
        object->SetRotation(rotation);

        for (const uint32_t meshIndex : unpackedObject.meshes) {
            object->AddMesh(mMeshes[meshIndex]);
        }
    }

    uint32_t objectIndex = 0;
    mObjects.push_back(mFlatObjects[0]);
    for (ScopedRefPtr<Object>& object : mFlatObjects) {
        for (const uint32_t childIndex : fileIn.objects[objectIndex].children) {
            object->AddChild(mFlatObjects[childIndex]);
        }
        ++objectIndex;
    }

    // Upload geometry buffer to GPU
    const std::vector<VKRTBaker::Vec3>& vertices = fileIn.unifiedGeometryBuffer.positions;
    const std::vector<uint32_t>& texCoord = fileIn.unifiedGeometryBuffer.textureCoords;
    const std::vector<uint32_t>& normals = fileIn.unifiedGeometryBuffer.normals;
    const std::vector<uint32_t>& tangents = fileIn.unifiedGeometryBuffer.tangents;
    const std::vector<uint32_t>& indices = fileIn.unifiedGeometryBuffer.indices;
    GetMeshSystem()->Upload(vertices, texCoord, normals, tangents, indices);
}

void Scene::Update() {
    for (ScopedRefPtr<Object> object : mObjects) {
        object->UpdateTransforms(glm::mat4(1.0f));
    }
    PackDrawData();
}

void Scene::PackDrawData() {
    // Flatten scene into a single list
    mPackedDrawData.clear();

    for (const ScopedRefPtr<Object>& object : mFlatObjects) {
        std::vector<ScopedRefPtr<Mesh>> meshes = object->GetMeshes();
        for (ScopedRefPtr<Mesh>& mesh : meshes) {
            for (Meshlet& meshlet : mesh->mMeshlets) {
                DrawData meshParameters{
                    .indexCount = meshlet.indexCount,
                    .firstIndex = meshlet.indexOffset,
                    .vertexOffset = static_cast<int32_t>(meshlet.vertexOffset),
                    .transform = object->GetAbsoluteTransform(),
                    .materialId = mesh->GetMaterial()->GetMaterialId(),
                    .normalTransform = glm::mat4(
                        glm::transpose(glm::inverse(glm::mat3(object->GetAbsoluteTransform())))),
                    .alphaMode = static_cast<uint32_t>(mesh->GetMaterial()->GetAlphaMode()),
                    .minBounds = meshlet.minBounds,
                    .maxBounds = meshlet.maxBounds,
                    .coneApex = meshlet.coneApex,
                    .coneAxis = meshlet.coneAxis,
                    .coneCutoff = meshlet.coneCutoff};
                mPackedDrawData.push_back(meshParameters);
            }
        }
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