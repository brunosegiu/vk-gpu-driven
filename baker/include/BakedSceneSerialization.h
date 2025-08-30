#pragma once

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <type_traits>
#include <concepts>

namespace VKRTBaker {

template <typename T>
void writeBinary(std::ostream& os, const T& value) {
    os.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
void readBinary(std::istream& is, T& value) {
    is.read(reinterpret_cast<char*>(&value), sizeof(T));
}

template <typename T>
concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

template <typename T>
concept HasSerialize = requires(const T& t, std::ostream& os) {
    { t.serialize(os) } -> std::same_as<void>;
};

template <typename T>
concept HasDeserialize = requires(T& t, std::istream& is) {
    { t.deserialize(is) } -> std::same_as<void>;
};

template <typename T>
void writeVector(std::ostream& os, const std::vector<T>& vec) {
    uint64_t size = static_cast<uint64_t>(vec.size());
    writeBinary(os, size);

    if constexpr (TriviallyCopyable<T>) {
        if (!vec.empty()) {
            os.write(reinterpret_cast<const char*>(vec.data()), sizeof(T) * vec.size());
        }
    } else if constexpr (HasSerialize<T>) {
        for (const auto& v : vec)
            v.serialize(os);
    } else {
        static_assert(false, "Cannot serialize this vector");
    }
}

template <typename T>
void readVector(std::istream& is, std::vector<T>& vec) {
    uint64_t size = 0;
    readBinary(is, size);
    vec.resize(static_cast<size_t>(size));

    if constexpr (TriviallyCopyable<T>) {
        if (size) {
            is.read(reinterpret_cast<char*>(vec.data()), sizeof(T) * size);
        }
    } else if constexpr (HasDeserialize<T>) {
        for (auto& v : vec)
            v.deserialize(is);
    } else {
        static_assert(false, "Cannot deserialize this vector");
    }
}

constexpr uint32_t MeshletSize = 128;

struct Vec3 {
    float x, y, z;
    void serialize(std::ostream& os) const { writeBinary(os, *this); }
    void deserialize(std::istream& is) { readBinary(is, *this); }
};

struct Vec4 {
    float x, y, z, w;
    void serialize(std::ostream& os) const { writeBinary(os, *this); }
    void deserialize(std::istream& is) { readBinary(is, *this); }
};

struct Object {
    Vec3 translation{0.0f, 0.0f, 0.0f};
    Vec4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};
    std::vector<uint32_t> meshes;
    std::vector<uint32_t> children;

    void serialize(std::ostream& os) const {
        translation.serialize(os);
        rotation.serialize(os);
        scale.serialize(os);
        writeVector(os, meshes);
        writeVector(os, children);
    }
    void deserialize(std::istream& is) {
        translation.deserialize(is);
        rotation.deserialize(is);
        scale.deserialize(is);
        readVector(is, meshes);
        readVector(is, children);
    }
};

struct Mesh {
    uint32_t material = 0;
    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    std::vector<uint32_t> meshlets;

    void serialize(std::ostream& os) const {
        writeBinary(os, material);
        writeBinary(os, vertexOffset);
        writeBinary(os, indexOffset);
        writeBinary(os, indexCount);
        writeVector(os, meshlets);
    }
    void deserialize(std::istream& is) {
        readBinary(is, material);
        readBinary(is, vertexOffset);
        readBinary(is, indexOffset);
        readBinary(is, indexCount);
        readVector(is, meshlets);
    }
};

enum MaterialType : uint32_t { Opaque, Masked, Blended };

struct Material {
    MaterialType materialType = MaterialType::Opaque;
    Vec3 albedo{0.0f, 0.0f, 0.0f};
    float roughness = 0.0f;
    float metallic = 0.0f;
    Vec3 emissive{0.0f, 0.0f, 0.0f};
    int32_t albedoTextureIndex = -1;
    int32_t metallicRoughnessTextureIndex = -1;
    int32_t normalTextureIndex = -1;
    int32_t emissiveTextureIndex = -1;

    void serialize(std::ostream& os) const {
        writeBinary(os, materialType);
        albedo.serialize(os);
        writeBinary(os, roughness);
        writeBinary(os, metallic);
        emissive.serialize(os);
        writeBinary(os, albedoTextureIndex);
        writeBinary(os, metallicRoughnessTextureIndex);
        writeBinary(os, normalTextureIndex);
        writeBinary(os, emissiveTextureIndex);
    }
    void deserialize(std::istream& is) {
        readBinary(is, materialType);
        albedo.deserialize(is);
        readBinary(is, roughness);
        readBinary(is, metallic);
        emissive.deserialize(is);
        readBinary(is, albedoTextureIndex);
        readBinary(is, metallicRoughnessTextureIndex);
        readBinary(is, normalTextureIndex);
        readBinary(is, emissiveTextureIndex);
    }
};

struct Meshlet {
    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    Vec3 minBounds;
    Vec3 maxBounds;
    Vec3 coneApex;
    Vec3 coneAxis;
    float coneCutoff;

    void serialize(std::ostream& os) const {
        writeBinary(os, vertexOffset);
        writeBinary(os, indexOffset);
        writeBinary(os, indexCount);
        minBounds.serialize(os);
        maxBounds.serialize(os);
        coneApex.serialize(os);
        coneAxis.serialize(os);
        writeBinary(os, coneCutoff);
    }
    void deserialize(std::istream& is) {
        readBinary(is, vertexOffset);
        readBinary(is, indexOffset);
        readBinary(is, indexCount);
        minBounds.deserialize(is);
        maxBounds.deserialize(is);
        coneApex.deserialize(is);
        coneAxis.deserialize(is);
        readBinary(is, coneCutoff);
    }
};

struct Texture {
    uint32_t width = 0;
    uint32_t height = 0;
    // Note: you used uint32_t here; keep it if that's intended.
    std::vector<uint32_t> data;

    void serialize(std::ostream& os) const {
        writeBinary(os, width);
        writeBinary(os, height);
        writeVector(os, data);
    }
    void deserialize(std::istream& is) {
        readBinary(is, width);
        readBinary(is, height);
        readVector(is, data);
    }
};

struct MeshData {
    std::vector<Vec3> positions;
    std::vector<uint32_t> normals;
    std::vector<uint32_t> textureCoords;
    std::vector<uint32_t> tangents;
    std::vector<uint32_t> indices;

    void serialize(std::ostream& os) const {
        writeVector(os, positions);
        writeVector(os, normals);
        writeVector(os, textureCoords);
        writeVector(os, tangents);
        writeVector(os, indices);
    }
    void deserialize(std::istream& is) {
        readVector(is, positions);
        readVector(is, normals);
        readVector(is, textureCoords);
        readVector(is, tangents);
        readVector(is, indices);
    }
};

struct BakedFile {
    uint32_t meshletSize = MeshletSize;
    std::vector<Object> objects;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Meshlet> meshlets;
    std::vector<Texture> textures;
    MeshData unifiedGeometryBuffer;

    void serialize(std::ostream& os) const {
        writeBinary(os, meshletSize);
        writeVector(os, objects);
        writeVector(os, meshes);
        writeVector(os, materials);
        writeVector(os, meshlets);
        writeVector(os, textures);
        unifiedGeometryBuffer.serialize(os);
    }
    void deserialize(std::istream& is) {
        readBinary(is, meshletSize);
        readVector(is, objects);
        readVector(is, meshes);
        readVector(is, materials);
        readVector(is, meshlets);
        readVector(is, textures);
        unifiedGeometryBuffer.deserialize(is);
    }
};

}  // namespace VKRTBaker