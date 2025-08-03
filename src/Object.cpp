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

static uint32_t sMeshIdOffset = 0;

std::vector<ScopedRefPtr<Mesh>> LoadMeshes(
    ScopedRefPtr<Context> context,
    ScopedRefPtr<Scene> scene,
    tinygltf::Model& model,
    const tinygltf::Mesh& mesh) {
    std::vector<ScopedRefPtr<Mesh>> meshes;
    uint32_t maxIndexId = 0;
    for (const tinygltf::Primitive& primitive : mesh.primitives) {
        const std::string positionName = "POSITION";
        const std::string normalName = "NORMAL";
        const std::string texCoordName = "TEXCOORD_0";
        const std::string tangentName = "TANGENT";

        const std::map<std::string, int>& attributes = primitive.attributes;
        const bool hasPositionAndNormals = attributes.find(positionName) != attributes.end() &&
                                           attributes.find(normalName) != attributes.end();
        const bool hasTexCoords = attributes.find(texCoordName) != attributes.end();
        const bool hasTangents = attributes.find(tangentName) != attributes.end();

        if (!hasPositionAndNormals || !hasTexCoords) {
            continue;
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
        std::vector<uint32_t> normals(normalAccessor.count);
        {
            const tinygltf::BufferView& normalBufferView =
                model.bufferViews[normalAccessor.bufferView];
            const tinygltf::Buffer& normalBuffer = model.buffers[normalBufferView.buffer];
            const size_t normalBufferOffset =
                normalBufferView.byteOffset + normalAccessor.byteOffset;
            size_t vetexStride = normalAccessor.ByteStride(normalBufferView);
            const unsigned char* normalData = &normalBuffer.data[normalBufferOffset];

            const uint32_t normalCount = static_cast<uint32_t>(normalAccessor.count);
            VKRT_ASSERT(normalAccessor.type == TINYGLTF_TYPE_VEC3);
            for (uint32_t normalIndex = 0; normalIndex < normalCount; ++normalIndex) {
                const float* normalDataFloat =
                    reinterpret_cast<const float*>(&normalData[vetexStride * normalIndex]);
                glm::vec3 normal =
                    glm::vec3(normalDataFloat[0], normalDataFloat[1], normalDataFloat[2]);
                normals[normalIndex] = glm::packSnorm4x8(glm::vec4(normal, 0.0f));
            }
        }

        const tinygltf::Accessor& texCoordAccessor = model.accessors[attributes.at(texCoordName)];
        std::vector<uint32_t> texCoords(texCoordAccessor.count);
        {
            const tinygltf::BufferView& texCoordBufferView =
                model.bufferViews[texCoordAccessor.bufferView];
            const tinygltf::Buffer& texCoordBuffer = model.buffers[texCoordBufferView.buffer];
            const size_t texCoordBufferOffset =
                texCoordBufferView.byteOffset + texCoordAccessor.byteOffset;
            size_t vetexStride = texCoordAccessor.ByteStride(texCoordBufferView);
            const unsigned char* texCoordData = &texCoordBuffer.data[texCoordBufferOffset];
            VKRT_ASSERT(texCoordAccessor.type == TINYGLTF_TYPE_VEC2);
            const uint32_t texCoordCount = static_cast<uint32_t>(texCoordAccessor.count);
            for (uint32_t texCoordIndex = 0; texCoordIndex < texCoordCount; ++texCoordIndex) {
                const float* texCoordDataFloat =
                    reinterpret_cast<const float*>(&texCoordData[vetexStride * texCoordIndex]);
                const glm::vec2 texCoord =
                    glm::vec2((texCoordDataFloat[0]), (texCoordDataFloat[1]));
                texCoords[texCoordIndex] = glm::packHalf2x16(texCoord);
            }
        }

        std::vector<uint32_t> tangents(normalAccessor.count, 0); // Assume meshes without tangents will not use normal maps (lazy).
        if (hasTangents) {
            const tinygltf::Accessor& tangentAccessor = model.accessors[attributes.at(tangentName)];
            tangents = std::vector<uint32_t>(tangentAccessor.count);
            {
                const tinygltf::BufferView& tangentBufferView =
                    model.bufferViews[tangentAccessor.bufferView];
                const tinygltf::Buffer& tangentBuffer = model.buffers[tangentBufferView.buffer];
                const size_t tangentBufferOffset =
                    tangentBufferView.byteOffset + tangentAccessor.byteOffset;
                size_t tangentStride = tangentAccessor.ByteStride(tangentBufferView);
                const unsigned char* tangentData = &tangentBuffer.data[tangentBufferOffset];

                const uint32_t tangentCount = static_cast<uint32_t>(tangentAccessor.count);
                VKRT_ASSERT(tangentAccessor.type == TINYGLTF_TYPE_VEC4);
                for (uint32_t tangentIndex = 0; tangentIndex < tangentCount; ++tangentIndex) {
                    const float* tangentDataFloat =
                        reinterpret_cast<const float*>(&tangentData[tangentStride * tangentIndex]);
                    glm::vec4 tangent = glm::vec4(
                        tangentDataFloat[0],
                        tangentDataFloat[1],
                        tangentDataFloat[2],
                        tangentDataFloat[3]);
                    tangents[tangentIndex] = glm::packSnorm4x8(tangent);
                }
            }
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
            const float roughness = gltfMaterial.pbrMetallicRoughness.roughnessFactor;
            const float metallic = gltfMaterial.pbrMetallicRoughness.metallicFactor;

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

            const int32_t normalMapIndex = gltfMaterial.normalTexture.index;
            ScopedRefPtr<Texture> normalTexture = nullptr;
            if (normalMapIndex >= 0) {
                const tinygltf::Texture& texture = model.textures[normalMapIndex];
                const tinygltf::Image& image = model.images[texture.source];
                normalTexture = new Texture(
                    context,
                    image.width,
                    image.height,
                    vk::Format::eR8G8B8A8Unorm,
                    image.image.data(),
                    image.image.size());
            }

            Material::AlphaMode alphaMode = Material::AlphaMode::Opaque;
            if (gltfMaterial.alphaMode == "OPAQUE") {
                alphaMode = Material::AlphaMode::Opaque;
            } else if (gltfMaterial.alphaMode == "MASK") {
                alphaMode = Material::AlphaMode::Masked;
            } else if (gltfMaterial.alphaMode == "BLEND") {
                alphaMode = Material::AlphaMode::Blended;
            }

            material = new Material(
                alphaMode,
                albedo,
                roughness,
                metallic,
                albedoTexture,
                roughnessTexture,
                normalTexture);
        } else {
            material = new Material();
        }
        ScopedRefPtr<Mesh> newMesh = scene->GetMeshSystem()->GetOrCreate(
            sMeshIdOffset + primitive.indices,
            positions,
            indices,
            texCoords,
            normals,
            tangents,
            material);
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
    uint32_t meshIdOffset = 0;
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
                    node.rotation[3],
                    node.rotation[0],
                    node.rotation[1],
                    node.rotation[2]);
                object->SetRotation(rotation);
            }

            if (node.mesh != InvalidIndex) {
                std::vector<ScopedRefPtr<Mesh>> objectMeshes =
                    LoadMeshes(context, scene, model, model.meshes[node.mesh]);
                meshIdOffset += objectMeshes.size();
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
            ScopedRefPtr<Object> root = new Object();
            scene->AddObject(root);
            tinygltf::Scene& gltfScene =
                model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];
            for (const int& nodeIndex : gltfScene.nodes) {
                if (nodeIndex >= 0) {
                    ScopedRefPtr<Object> object = loadSubtree(model.nodes[nodeIndex]);
                    root->AddChild(object);
                }
            }
            sMeshIdOffset += meshIdOffset;
            return root;
        }
    }
    return nullptr;
}

Object::Object()
    : mLocalTransform(1.0f),
      mAbsoluteTranform(1.0f),
      mPosition(0.0f),
      mRotation(glm::vec3(0.0f)),
      mScale(1.0f, 1.0f, 1.0f) {}

void Object::SetTranslation(const glm::vec3& position) {
    mPosition = position;
}

void Object::Translate(const glm::vec3& delta) {
    mPosition += delta;
}

void Object::Rotate(const glm::vec3& delta) {
    mRotation *= glm::quat(glm::radians(delta));
}

void Object::SetRotation(const glm::quat& rotation) {
    mRotation = rotation;
}

void Object::Scale(const glm::vec3& delta) {
    mScale += delta;
}

void Object::SetScale(const glm::vec3& scale) {
    mScale = scale;
}

void Object::UpdateTransforms(const glm::mat4& parentTransform) {
    glm::mat4 translate = glm::translate(glm::mat4(1.0f), mPosition);
    glm::mat4 rotate = glm::toMat4(mRotation);
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