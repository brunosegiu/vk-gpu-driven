#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include "meshoptimizer.h"
#include "nlohmann/json.hpp"
#include "tiny_gltf.h"

#include "BakedSceneSerialization.h"

using namespace VKRTBaker;

BakedFile gBakedFile;

MeshData loadPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive) {
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
        exit(-1);
    }

    MeshData ret;

    const tinygltf::Accessor& positionAccessor = model.accessors[attributes.at(positionName)];
    ret.positions = std::vector<Vec3>(positionAccessor.count);
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
            ret.positions[positionIndex] =
                Vec3(positionDataFloat[0], positionDataFloat[1], positionDataFloat[2]);
        }
    }

    const tinygltf::Accessor& normalAccessor = model.accessors[attributes.at(normalName)];
    ret.normals = std::vector<uint32_t>(normalAccessor.count);
    {
        const tinygltf::BufferView& normalBufferView = model.bufferViews[normalAccessor.bufferView];
        const tinygltf::Buffer& normalBuffer = model.buffers[normalBufferView.buffer];
        const size_t normalBufferOffset = normalBufferView.byteOffset + normalAccessor.byteOffset;
        size_t vetexStride = normalAccessor.ByteStride(normalBufferView);
        const unsigned char* normalData = &normalBuffer.data[normalBufferOffset];

        const uint32_t normalCount = static_cast<uint32_t>(normalAccessor.count);
        for (uint32_t normalIndex = 0; normalIndex < normalCount; ++normalIndex) {
            const float* normalDataFloat =
                reinterpret_cast<const float*>(&normalData[vetexStride * normalIndex]);
            glm::vec3 normal =
                glm::vec3(normalDataFloat[0], normalDataFloat[1], normalDataFloat[2]);
            ret.normals[normalIndex] = glm::packSnorm4x8(glm::vec4(normal, 0.0f));
        }
    }

    const tinygltf::Accessor& texCoordAccessor = model.accessors[attributes.at(texCoordName)];
    ret.textureCoords = std::vector<uint32_t>(texCoordAccessor.count);
    {
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
            const glm::vec2 texCoord = glm::vec2((texCoordDataFloat[0]), (texCoordDataFloat[1]));
            ret.textureCoords[texCoordIndex] = glm::packHalf2x16(texCoord);
        }
    }

    ret.tangents = std::vector<uint32_t>(
        normalAccessor.count,
        0);  // Assume meshes without tangents will not use normal maps (lazy).
    if (hasTangents) {
        const tinygltf::Accessor& tangentAccessor = model.accessors[attributes.at(tangentName)];
        ret.tangents = std::vector<uint32_t>(tangentAccessor.count);
        {
            const tinygltf::BufferView& tangentBufferView =
                model.bufferViews[tangentAccessor.bufferView];
            const tinygltf::Buffer& tangentBuffer = model.buffers[tangentBufferView.buffer];
            const size_t tangentBufferOffset =
                tangentBufferView.byteOffset + tangentAccessor.byteOffset;
            size_t tangentStride = tangentAccessor.ByteStride(tangentBufferView);
            const unsigned char* tangentData = &tangentBuffer.data[tangentBufferOffset];

            const uint32_t tangentCount = static_cast<uint32_t>(tangentAccessor.count);
            for (uint32_t tangentIndex = 0; tangentIndex < tangentCount; ++tangentIndex) {
                const float* tangentDataFloat =
                    reinterpret_cast<const float*>(&tangentData[tangentStride * tangentIndex]);
                glm::vec4 tangent = glm::vec4(
                    tangentDataFloat[0],
                    tangentDataFloat[1],
                    tangentDataFloat[2],
                    tangentDataFloat[3]);
                ret.tangents[tangentIndex] = glm::packSnorm4x8(tangent);
            }
        }
    }

    const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
    {
        const uint32_t indexCount = static_cast<uint32_t>(indexAccessor.count);
        const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];
        const size_t indexOffset = indexBufferView.byteOffset + indexAccessor.byteOffset;
        const uint16_t* pIndexData16Bit =
            reinterpret_cast<const uint16_t*>(&indexBuffer.data[indexOffset]);
        const uint32_t* pIndexData32Bit =
            reinterpret_cast<const uint32_t*>(&indexBuffer.data[indexOffset]);
        ret.indices.reserve(indexCount);
        for (uint32_t i = 0; i < indexCount; ++i) {
            if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                ret.indices.push_back(static_cast<uint32_t>(pIndexData32Bit[i]));
            } else {
                ret.indices.push_back(pIndexData16Bit[i]);
            }
        }
    }

    return ret;
}

std::vector<uint32_t> generateMeshlets(
    MeshData& mesh,
    uint32_t& outVertexOffset,
    uint32_t& outIndexOffset,
    uint32_t& outIndexCount) {
    MeshData& ugb = gBakedFile.unifiedGeometryBuffer;

    outVertexOffset = ugb.positions.size();
    outIndexOffset = ugb.indices.size();
    outIndexCount = 0;

    std::vector<uint32_t> outMeshletIndices;

    const size_t maxVertices = 64;
    const size_t maxTriangles = MeshletSize;
    const float coneWeight = 0.0f;

    size_t maxMeshlets = meshopt_buildMeshletsBound(mesh.indices.size(), maxVertices, maxTriangles);
    std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
    std::vector<unsigned int> meshletVertices(maxMeshlets * maxVertices);
    std::vector<unsigned char> meshletTriangles(maxMeshlets * maxTriangles * 3);

    size_t meshletCount = meshopt_buildMeshlets(
        meshlets.data(),
        meshletVertices.data(),
        meshletTriangles.data(),
        mesh.indices.data(),
        mesh.indices.size(),
        &mesh.positions[0].x,
        mesh.positions.size(),
        sizeof(Vec3),
        maxVertices,
        maxTriangles,
        coneWeight);

    const meshopt_Meshlet& last = meshlets[meshletCount - 1];
    meshletVertices.resize(last.vertex_offset + last.vertex_count);
    meshletTriangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
    meshlets.resize(meshletCount);

    const uint32_t vertexOffset = gBakedFile.unifiedGeometryBuffer.positions.size();
    {
        ugb.positions.insert(ugb.positions.end(), mesh.positions.begin(), mesh.positions.end());
        ugb.normals.insert(ugb.normals.end(), mesh.normals.begin(), mesh.normals.end());
        ugb.textureCoords.insert(
            ugb.textureCoords.end(),
            mesh.textureCoords.begin(),
            mesh.textureCoords.end());
        ugb.tangents.insert(ugb.tangents.end(), mesh.tangents.begin(), mesh.tangents.end());
    }

    for (size_t mi = 0; mi < meshletCount; ++mi) {
        outMeshletIndices.push_back(gBakedFile.meshlets.size());
        Meshlet& outMeshlet = gBakedFile.meshlets.emplace_back();
        outMeshlet.indexOffset = ugb.indices.size();
        outMeshlet.vertexOffset = vertexOffset;

        const meshopt_Meshlet& meshlet = meshlets[mi];
        const uint32_t* verts = &meshletVertices[meshlet.vertex_offset];
        const uint8_t* tris = &meshletTriangles[meshlet.triangle_offset];

        meshopt_Bounds bounds = meshopt_computeMeshletBounds(
            &meshletVertices[meshlet.vertex_offset],
            &meshletTriangles[meshlet.triangle_offset],
            meshlet.triangle_count,
            &mesh.positions[0].x,
            mesh.positions.size(),
            sizeof(Vec3));

        Vec3 minBounds = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()};
        Vec3 maxBounds{
            std::numeric_limits<float>::min(),
            std::numeric_limits<float>::min(),
            std::numeric_limits<float>::min()};
        for (uint32_t t = 0; t < meshlet.triangle_count * 3; ++t) {
            const uint8_t localIndex = tris[t];
            const uint32_t globalIndex = verts[localIndex];
            Vec3 vertex = mesh.positions[globalIndex];
            minBounds = Vec3{
                glm::min<float>(minBounds.x, vertex.x),
                glm::min<float>(minBounds.y, vertex.y),
                glm::min<float>(minBounds.z, vertex.z)};
            maxBounds = Vec3{
                glm::max<float>(maxBounds.x, vertex.x),
                glm::max<float>(maxBounds.y, vertex.y),
                glm::max<float>(maxBounds.z, vertex.z)};
        }

        outMeshlet.indexCount = 3 * meshlet.triangle_count;
        outMeshlet.minBounds = minBounds;
        outMeshlet.maxBounds = maxBounds;
        outMeshlet.coneApex = Vec3{bounds.cone_apex[0], bounds.cone_apex[1], bounds.cone_apex[2]};
        outMeshlet.coneAxis = Vec3{bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2]};
        outMeshlet.coneCutoff = bounds.cone_cutoff;

        outIndexCount += 3 * meshlet.triangle_count;

        for (uint32_t t = 0; t < meshlet.triangle_count; ++t) {
            const uint8_t i0 = tris[t * 3 + 0];
            const uint8_t i1 = tris[t * 3 + 1];
            const uint8_t i2 = tris[t * 3 + 2];

            const uint32_t g0 = verts[i0];
            const uint32_t g1 = verts[i1];
            const uint32_t g2 = verts[i2];

            ugb.indices.push_back(g0);
            ugb.indices.push_back(g1);
            ugb.indices.push_back(g2);
        }
    }

    return outMeshletIndices;
}

uint32_t getOrCreateTexture(const tinygltf::Model& model, int index) {
    static std::unordered_map<int, uint32_t> gTextureCache;
    auto cachedIt = gTextureCache.find(index);
    if (cachedIt != gTextureCache.end()) {
        return cachedIt->second;
    } else {
        uint32_t outTextureIndex = gBakedFile.textures.size();
        Texture& outTexture = gBakedFile.textures.emplace_back();
        const tinygltf::Image& image = model.images[index];
        outTexture.width = image.width;
        outTexture.height = image.height;
        outTexture.data = std::vector<uint32_t>(image.image.size() / 4);
        uint8_t const* dataBuffer = reinterpret_cast<uint8_t const*>(image.image.data());
        std::copy_n(
            dataBuffer,
            image.image.size(),
            reinterpret_cast<uint8_t*>(outTexture.data.data()));
        gTextureCache.emplace(index, outTextureIndex);
        return outTextureIndex;
    }
}

uint32_t loadMaterial(const tinygltf::Model& model, const tinygltf::Primitive& primitive) {
    uint32_t outMaterialIndex = gBakedFile.materials.size();
    Material& outMaterial = gBakedFile.materials.emplace_back();

    if (primitive.material >= 0) {
        const tinygltf::Material& gltfMaterial = model.materials[primitive.material];

        const std::vector<double>& baseColor = gltfMaterial.pbrMetallicRoughness.baseColorFactor;
        Vec3 albedo = Vec3(baseColor[0], baseColor[1], baseColor[2]);
        const float roughness = gltfMaterial.pbrMetallicRoughness.roughnessFactor;
        const float metallic = gltfMaterial.pbrMetallicRoughness.metallicFactor;
        const std::vector<double>& emissiveFactor = gltfMaterial.emissiveFactor;
        Vec3 emissive = Vec3(emissiveFactor[0], emissiveFactor[1], emissiveFactor[2]);

        const int32_t albedoTextureIndex = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
        if (albedoTextureIndex >= 0) {
            const tinygltf::Texture& texture = model.textures[albedoTextureIndex];
            outMaterial.albedoTextureIndex = getOrCreateTexture(model, texture.source);
        }

        const int32_t roughnessTextureIndex =
            gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
        gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
        if (roughnessTextureIndex >= 0) {
            const tinygltf::Texture& texture = model.textures[roughnessTextureIndex];
            outMaterial.metallicRoughnessTextureIndex = getOrCreateTexture(model, texture.source);
        }

        const int32_t normalMapIndex = gltfMaterial.normalTexture.index;
        if (normalMapIndex >= 0) {
            const tinygltf::Texture& texture = model.textures[normalMapIndex];
            outMaterial.normalTextureIndex = getOrCreateTexture(model, texture.source);
        }

        const int32_t emissiveTextureIndex = gltfMaterial.emissiveTexture.index;
        if (emissiveTextureIndex >= 0) {
            const tinygltf::Texture& texture = model.textures[emissiveTextureIndex];
            outMaterial.emissiveTextureIndex = getOrCreateTexture(model, texture.source);
        }

        MaterialType alphaMode = MaterialType::Opaque;
        if (gltfMaterial.alphaMode == "OPAQUE") {
            alphaMode = MaterialType::Opaque;
        } else if (gltfMaterial.alphaMode == "MASK") {
            alphaMode = MaterialType::Masked;
        } else if (gltfMaterial.alphaMode == "BLEND") {
            alphaMode = MaterialType::Blended;
        }

        outMaterial.materialType = alphaMode;
        outMaterial.albedo = albedo;
        outMaterial.roughness = roughness;
        outMaterial.metallic = metallic;
        outMaterial.emissive = emissive;
    }

    return outMaterialIndex;
}

std::vector<uint32_t> loadMeshes(const tinygltf::Model& model, const tinygltf::Mesh& mesh) {
    std::vector<uint32_t> meshes;
    for (const tinygltf::Primitive& primitive : mesh.primitives) {
        meshes.push_back(gBakedFile.meshes.size());
        Mesh& mesh = gBakedFile.meshes.emplace_back();
        MeshData meshPrimitive = loadPrimitive(model, primitive);
        mesh.meshlets =
            generateMeshlets(meshPrimitive, mesh.vertexOffset, mesh.indexOffset, mesh.indexCount);
        mesh.material = loadMaterial(model, primitive);
    }
    return meshes;
}

std::vector<uint32_t> getOrCreateMeshes(
    const tinygltf::Model& model,
    const tinygltf::Mesh& mesh,
    int index) {
    static std::unordered_map<int, std::vector<uint32_t>> gMeshCache;
    auto cachedIt = gMeshCache.find(index);
    if (cachedIt != gMeshCache.end()) {
        return cachedIt->second;
    } else {
        std::vector<uint32_t> meshes = loadMeshes(model, mesh);
        gMeshCache.emplace(index, meshes);
        return meshes;
    }
}

uint32_t loadSubtree(const tinygltf::Model& model, const tinygltf::Node& node) {
    const uint32_t objectIndex = gBakedFile.objects.size();
    Object& object = gBakedFile.objects.emplace_back();

    Vec3 translation = Vec3{0.0f, 0.0f, 0.0f};
    if (!node.translation.empty()) {
        translation = Vec3(node.translation[0], node.translation[1], node.translation[2]);
    }
    object.translation = translation;

    Vec3 scale = Vec3{1.0f, 1.0f, 1.0f};
    if (!node.scale.empty()) {
        scale = Vec3(node.scale[0], node.scale[1], node.scale[2]);
    }
    object.scale = scale;

    Vec4 rotation = Vec4{0.0f, 0.0f, 0.0f, 1.0f};
    if (!node.rotation.empty()) {
        // GLTF is wxyz
        rotation = Vec4(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
    }
    object.rotation = rotation;

    if (node.mesh >= 0) {
        object.meshes = getOrCreateMeshes(model, model.meshes[node.mesh], node.mesh);
    }

    for (uint32_t nodeIndex : node.children) {
        uint32_t childIndex = loadSubtree(model, model.nodes[nodeIndex]);
        gBakedFile.objects[objectIndex].children.push_back(childIndex);
    }

    return objectIndex;
}

bool loadGLTF(std::string path, tinygltf::Model& model) {
    bool isProperlyLoaded = false;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    if (path.ends_with(".gltf")) {
        isProperlyLoaded = loader.LoadASCIIFromFile(&model, &err, &warn, path);
    } else {
        isProperlyLoaded = loader.LoadBinaryFromFile(&model, &err, &warn, path);
    }
    return isProperlyLoaded;
}

int main(int argc, char* argv[]) {
    if (argc <= 2) {
        return -1;
    }
    std::string sourceFileName(argv[1]);
    std::string dstFileName(argv[2]);

    tinygltf::Model model;
    bool isProperlyLoaded = loadGLTF(sourceFileName, model);
    if (isProperlyLoaded) {
        if (!model.nodes.empty()) {
            gBakedFile.objects.push_back({});
            tinygltf::Scene& gltfScene =
                model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];
            for (const int& nodeIndex : gltfScene.nodes) {
                if (nodeIndex >= 0) {
                    uint32_t childIndex = loadSubtree(model, model.nodes[nodeIndex]);
                    gBakedFile.objects[0].children.push_back(childIndex);
                }
            }
        }
    }

    std::ofstream ofs(dstFileName, std::ios::binary);
    gBakedFile.serialize(ofs);
    ofs.close();

    return 0;
}