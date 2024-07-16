#include "Object.h"

#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include "nlohmann/json.hpp"
#include "tiny_gltf.h"

#include "DebugUtils.h"
#include "Scene.h"

namespace VKRT {

std::vector<ScopedRefPtr<Mesh>> LoadMeshes(
    ScopedRefPtr<Context> context,
    ScopedRefPtr<Scene> scene,
    tinygltf::Model& model,
    const tinygltf::Mesh& mesh) {
    std::vector<ScopedRefPtr<Mesh>> meshes;
    for (const tinygltf::Primitive& primitive : mesh.primitives) {
        const std::string positionName = "POSITION";
        const std::string normalName = "NORMAL";
        const std::string texCoordName = "TEXCOORD_0";

        const std::map<std::string, int>& attributes = primitive.attributes;
        const bool hasPositionAndNormals = attributes.find(positionName) != attributes.end() &&
                                           attributes.find(normalName) != attributes.end();
        const bool hasTexCoords = attributes.find(texCoordName) != attributes.end();

        if (!hasPositionAndNormals) {
            return {};
        }

        const tinygltf::Accessor& positionAccessor = model.accessors[attributes.at(positionName)];
        std::vector<glm::vec3> positions(positionAccessor.count);
        {
            const tinygltf::BufferView& positionBufferView =
                model.bufferViews[positionAccessor.bufferView];
            const tinygltf::Buffer& positionBuffer = model.buffers[positionBufferView.buffer];
            const size_t positionBufferOffset =
                positionBufferView.byteOffset + positionAccessor.byteOffset;
            size_t vetexStride = positionAccessor.ByteStride(positionBufferView);
            const unsigned char* positionData = &positionBuffer.data[positionBufferOffset];

            const uint32_t positionCount = static_cast<uint32_t>(positionAccessor.count);
            for (uint32_t positionIndex = 0; positionIndex < positionCount; ++positionIndex) {
                const float* positionDataFloat =
                    reinterpret_cast<const float*>(&positionData[vetexStride * positionIndex]);
                positions[positionIndex] =
                    glm::vec3(positionDataFloat[0], positionDataFloat[1], positionDataFloat[2]);
            }
        }

        const tinygltf::Accessor& normalAccessor = model.accessors[attributes.at(normalName)];
        std::vector<glm::vec3> normals(normalAccessor.count);
        {
            const tinygltf::BufferView& normalBufferView =
                model.bufferViews[normalAccessor.bufferView];
            const tinygltf::Buffer& normalBuffer = model.buffers[normalBufferView.buffer];
            const size_t normalBufferOffset =
                normalBufferView.byteOffset + normalAccessor.byteOffset;
            size_t vetexStride = normalAccessor.ByteStride(normalBufferView);
            const unsigned char* normalData = &normalBuffer.data[normalBufferOffset];

            const uint32_t normalCount = static_cast<uint32_t>(normalAccessor.count);
            for (uint32_t normalIndex = 0; normalIndex < normalCount; ++normalIndex) {
                const float* normalDataFloat =
                    reinterpret_cast<const float*>(&normalData[vetexStride * normalIndex]);
                normals[normalIndex] =
                    glm::vec3(normalDataFloat[0], normalDataFloat[1], normalDataFloat[2]);
            }
        }

        std::vector<uint32_t> texCoords;
        if (hasTexCoords) {
            const tinygltf::Accessor& texCoordAccessor =
                model.accessors[attributes.at(texCoordName)];
            texCoords = std::vector<uint32_t>(texCoordAccessor.count);

            const tinygltf::BufferView& texCoordBufferView =
                model.bufferViews[texCoordAccessor.bufferView];
            const tinygltf::Buffer& texCoordBuffer = model.buffers[texCoordBufferView.buffer];
            const size_t texCoordBufferOffset =
                texCoordBufferView.byteOffset + texCoordAccessor.byteOffset;
            size_t vetexStride = texCoordAccessor.ByteStride(texCoordBufferView);
            const unsigned char* texCoordData = &texCoordBuffer.data[texCoordBufferOffset];

            const uint32_t texCoordCount = static_cast<uint32_t>(texCoordAccessor.count);
            for (uint32_t texCoordIndex = 0; texCoordIndex < texCoordCount; ++texCoordIndex) {
                const float* texCoordDataFloat =
                    reinterpret_cast<const float*>(&texCoordData[vetexStride * texCoordIndex]);
                const auto packUnorm2x16 = [](const glm::vec2& input) -> uint32_t {
                    return static_cast<uint32_t>(input.x * 255.0f) << 16 |
                           static_cast<uint32_t>(input.x * 255.0f);
                };
                texCoords[texCoordIndex] =
                    packUnorm2x16(glm::vec2(texCoordDataFloat[0], texCoordDataFloat[1]));
            }
        } else {
            texCoords = std::vector<uint32_t>(positionAccessor.count, 0);
        }

        std::vector<glm::vec3> vertices;
        vertices.reserve(positions.size());
        for (size_t vertexIndex = 0; vertexIndex < positions.size(); ++vertexIndex) {
            vertices.push_back(positions[vertexIndex]);
        }

        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        std::vector<uint32_t> indices;
        {
            const uint32_t indexCount = static_cast<uint32_t>(indexAccessor.count);
            const tinygltf::BufferView& indexBufferView =
                model.bufferViews[indexAccessor.bufferView];
            const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];
            const size_t indexOffset = indexBufferView.byteOffset + indexAccessor.byteOffset;
            const uint16_t* pIndexData16Bit =
                reinterpret_cast<const uint16_t*>(&indexBuffer.data[indexOffset]);
            const uint32_t* pIndexData32Bit =
                reinterpret_cast<const uint32_t*>(&indexBuffer.data[indexOffset]);
            indices.reserve(indexCount);
            for (uint32_t i = 0; i < indexCount; ++i) {
                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    indices.push_back(static_cast<uint32_t>(pIndexData32Bit[i]));
                } else {
                    indices.push_back(pIndexData16Bit[i]);
                }
            }
        }

        const int32_t materialIndex = primitive.material;
        ScopedRefPtr<Material> material = nullptr;
        if (materialIndex >= 0) {
            const tinygltf::Material& gltfMaterial = model.materials[materialIndex];

            const std::vector<double>& baseColor =
                gltfMaterial.pbrMetallicRoughness.baseColorFactor;
            glm::vec3 albedo = glm::vec3(baseColor[0], baseColor[1], baseColor[2]);
            float roughness = gltfMaterial.pbrMetallicRoughness.roughnessFactor;

            const int32_t albedoTextureIndex =
                gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
            ScopedRefPtr<Texture> albedoTexture = nullptr;
            if (albedoTextureIndex >= 0) {
                const tinygltf::Texture& texture = model.textures[albedoTextureIndex];
                const tinygltf::Image& image = model.images[texture.source];
                albedoTexture = new Texture(
                    context,
                    image.width,
                    image.height,
                    vk::Format::eR8G8B8A8Unorm,
                    image.image.data(),
                    image.image.size());
            }

            const int32_t roughnessTextureIndex =
                gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
            gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
            ScopedRefPtr<Texture> roughnessTexture = nullptr;
            if (roughnessTextureIndex >= 0) {
                const tinygltf::Texture& texture = model.textures[roughnessTextureIndex];
                const tinygltf::Image& image = model.images[texture.source];
                roughnessTexture = new Texture(
                    context,
                    image.width,
                    image.height,
                    vk::Format::eR8G8B8A8Unorm,
                    image.image.data(),
                    image.image.size());
            }

            material = new Material(
                albedo,
                glm::vec3(0.0f),
                roughness,
                0.0f,
                0.0f,
                1.0f,
                albedoTexture,
                roughnessTexture);
        } else {
            material = new Material();
        }
        ScopedRefPtr<Mesh> newMesh = scene->GetMeshSystem()->GetOrCreate(primitive.indices, vertices, indices, material);
        meshes.push_back(newMesh);
    }
    return meshes;
}

ScopedRefPtr<Object> Object::Load(
    ScopedRefPtr<Context> context,
    ScopedRefPtr<Scene> scene,
    const std::string& path) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    bool isProperlyLoaded = false;
    if (path.ends_with(".gltf")) {
        isProperlyLoaded = loader.LoadASCIIFromFile(&model, &err, &warn, path);
    } else {
        isProperlyLoaded = loader.LoadBinaryFromFile(&model, &err, &warn, path);
    }
    constexpr int32_t InvalidIndex = -1;
    if (isProperlyLoaded) {
        std::function<ScopedRefPtr<Object>(tinygltf::Node&)> loadSubtree;
        loadSubtree = [&](tinygltf::Node& node) -> ScopedRefPtr<Object> {
            ScopedRefPtr<Object> object = new Object();
            if (!node.translation.empty()) {
                glm::vec3 translation(
                    node.translation[0],
                    node.translation[1],
                    node.translation[2]);
                object->SetTranslation(translation);
            }

            if (!node.scale.empty()) {
                glm::vec3 scale(node.scale[0], node.scale[1], node.scale[2]);
                object->SetScale(scale);
            }

            if (!node.rotation.empty()) {
                glm::quat rotation(
                    node.rotation[0],
                    node.rotation[1],
                    node.rotation[2],
                    node.rotation[3]);
                glm::vec3 rotationEuler = glm::degrees(glm::eulerAngles(rotation));
                object->SetRotation(rotationEuler);
            }

            if (node.mesh != InvalidIndex) {
                std::vector<ScopedRefPtr<Mesh>> objectMeshes =
                    LoadMeshes(context, scene, model, model.meshes[node.mesh]);
                for (const ScopedRefPtr<Mesh>& mesh : objectMeshes) {
                    object->AddMesh(mesh);
                }
            }

            for (uint32_t nodeIndex : node.children) {
                ScopedRefPtr<Object> child = loadSubtree(model.nodes[nodeIndex]);
                object->AddChild(child);
            }

            return object;
        };

        if (!model.nodes.empty()) {
            ScopedRefPtr<Object> object = loadSubtree(model.nodes.front());
            scene->AddObject(object);
            return object;
        }
    }
    return nullptr;
}

Object::Object()
    : mLocalTransform(1.0f),
      mAbsoluteTranform(1.0f),
      mPosition(0.0f),
      mEulerRotation(0.0f),
      mScale(1.0f, 1.0f, 1.0f) {}

void Object::SetTranslation(const glm::vec3& position) {
    mPosition = position;
}

void Object::Translate(const glm::vec3& delta) {
    mPosition += delta;
}

void Object::Rotate(const glm::vec3& delta) {
    mEulerRotation += delta;
}

void Object::SetRotation(const glm::vec3& rotation) {
    mEulerRotation = rotation;
}

void Object::Scale(const glm::vec3& delta) {
    mScale += delta;
}

void Object::SetScale(const glm::vec3& scale) {
    mScale = scale;
}

void Object::UpdateTransforms(const glm::mat4& parentTransform) {
    glm::mat4 translate = glm::translate(glm::mat4(1.0f), mPosition);
    glm::mat4 rotate = glm::eulerAngleYXZ(
        glm::radians(mEulerRotation.y),
        glm::radians(mEulerRotation.x),
        glm::radians(mEulerRotation.z));
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), mScale);
    mLocalTransform = translate * rotate * scale;
    mAbsoluteTranform = parentTransform * mLocalTransform;
    for (ScopedRefPtr<Object> child : mChildren) {
        child->UpdateTransforms(mAbsoluteTranform);
    }
}

void Object::AddChild(ScopedRefPtr<Object> child) {
    mChildren.push_back(child);
}

Object::~Object() {}

}  // namespace VKRT