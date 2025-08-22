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
    // TODO: Support updates
    if (mPackedDrawData.persistentDrawData.empty()) {
        PackDrawData();
    }
}

void Scene::PackDrawData() {
    // Flatten scene into a single list
    mPackedDrawData = {};
    mPackedDrawData.perMeshData.reserve(mFlatObjects.size() * 4);
    mPackedDrawData.persistentDrawData.reserve(mFlatObjects.size() * 200);

    uint32_t meshIndex = 0;
    for (const ScopedRefPtr<Object>& object : mFlatObjects) {
        const glm::mat4 normalTransform =
            glm::mat4(glm::transpose(glm::inverse(glm::mat3(object->GetAbsoluteTransform()))));
        const std::vector<ScopedRefPtr<Mesh>>& meshes = object->GetMeshes();
        for (const ScopedRefPtr<Mesh>& mesh : meshes) {
            MeshData meshData{
                .transform = object->GetAbsoluteTransform(),
                .materialId = mesh->GetMaterial()->GetMaterialId(),
                .normalTransform = normalTransform,
            };
            mPackedDrawData.perMeshData.push_back(meshData);
            for (Meshlet& meshlet : mesh->mMeshlets) {
                PersistentDrawData meshParameters{
                    .meshIndex = meshIndex,
                    .indexCount = meshlet.indexCount,
                    .firstIndex = meshlet.indexOffset,
                    .vertexOffset = static_cast<int32_t>(meshlet.vertexOffset),
                    .alphaMode = static_cast<uint32_t>(mesh->GetMaterial()->GetAlphaMode()),
                    .minBounds = meshlet.minBounds,
                    .maxBounds = meshlet.maxBounds,
                    .coneApex = meshlet.coneApex,
                    .coneAxis = meshlet.coneAxis,
                    .coneCutoff = meshlet.coneCutoff,
                };
                mPackedDrawData.persistentDrawData.push_back(meshParameters);
            }
            meshIndex++;
        }
    }

    // Split per-material type
    std::sort(
        mPackedDrawData.persistentDrawData.begin(),
        mPackedDrawData.persistentDrawData.end(),
        [](const PersistentDrawData& a, const PersistentDrawData& b) {
            return static_cast<int>(a.alphaMode) < static_cast<int>(b.alphaMode);
        });

    for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
        mRenderPassResources[alphaMode].cachedDrawCallCount = 0;
        mRenderPassResources[alphaMode].cachedDrawOffset = 0;
    }

    for (const PersistentDrawData& drawData : mPackedDrawData.persistentDrawData) {
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

    // Create BLAS/TLAS
    {
        vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
        {
            struct TemporaryBlasBuildData {
                ScopedRefPtr<VulkanBuffer> scratchBuffer;
                uint64_t blasBufferSize;
                uint64_t blasBufferOffset;
                uint64_t scratchSize;
                vk::AccelerationStructureGeometryTrianglesDataKHR triangleData;
                vk::AccelerationStructureGeometryKHR accelerationStructureGeometry;
                vk::AccelerationStructureBuildGeometryInfoKHR
                    accelerationStructureBuildGeometryInfo;
                vk::AccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo;
            };
            std::vector<TemporaryBlasBuildData> blasBuildData(mPackedDrawData.persistentDrawData.size());
            mRaytracingScene.blasResources =
                std::vector<BlasResources>(mPackedDrawData.persistentDrawData.size());

            size_t blasBufferSize = 0;
            vk::CommandBuffer commandBuffer = mContext->GetDevice()->CreateCommandBuffer();
            VKRT_ASSERT_VK(commandBuffer.begin(vk::CommandBufferBeginInfo{}));
            for (uint32_t index = 0; index < mPackedDrawData.persistentDrawData.size(); ++index) {
                BlasResources& blasResources = mRaytracingScene.blasResources[index];
                TemporaryBlasBuildData& tempBlasData = blasBuildData[index];
                const PersistentDrawData& drawData = mPackedDrawData.persistentDrawData[index];

                if (drawData.alphaMode == static_cast<uint32_t>(Material::AlphaMode::Blended)) {
                    continue;
                }

                const uint32_t primitiveCount = drawData.indexCount / 3;

                vk::DeviceAddress indexAddress = mMeshSystem->GetIndexBuffer()->GetDeviceAddress() +
                                                 drawData.firstIndex * sizeof(uint32_t);
                tempBlasData.triangleData =
                    vk::AccelerationStructureGeometryTrianglesDataKHR()
                        .setVertexFormat(vk::Format::eR32G32B32Sfloat)
                        .setVertexData(mMeshSystem->GetVertexBuffer()->GetDeviceAddress())
                        .setMaxVertex(mMeshSystem->GetVertexCount() - 1)
                        .setVertexStride(sizeof(glm::vec3))
                        .setIndexType(vk::IndexType::eUint32)
                        .setIndexData(indexAddress)
                        .setTransformData(nullptr);

                tempBlasData.accelerationStructureGeometry =
                    vk::AccelerationStructureGeometryKHR()
                        .setFlags(vk::GeometryFlagBitsKHR::eOpaque)
                        .setGeometryType(vk::GeometryTypeKHR::eTriangles)
                        .setGeometry(tempBlasData.triangleData);

                tempBlasData.accelerationStructureBuildGeometryInfo =
                    vk::AccelerationStructureBuildGeometryInfoKHR()
                        .setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
                        .setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
                        .setGeometries(tempBlasData.accelerationStructureGeometry);

                vk::AccelerationStructureBuildSizesInfoKHR buildSizesInfo =
                    logicalDevice.getAccelerationStructureBuildSizesKHR(
                        vk::AccelerationStructureBuildTypeKHR::eDevice,
                        tempBlasData.accelerationStructureBuildGeometryInfo,
                        primitiveCount,
                        mContext->GetDevice()->GetDispatcher());

                const size_t handleAlignment = 256;
                tempBlasData.blasBufferSize = buildSizesInfo.accelerationStructureSize;
                tempBlasData.blasBufferOffset = blasBufferSize;
                tempBlasData.scratchSize = buildSizesInfo.buildScratchSize;
                blasBufferSize += (buildSizesInfo.accelerationStructureSize + handleAlignment - 1) &
                                  ~(handleAlignment - 1);
            }

            mRaytracingScene.mBLASBuffer = mContext->GetDevice()->CreateBuffer(
                blasBufferSize,
                vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                    vk::BufferUsageFlagBits::eShaderDeviceAddress,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

            std::vector<vk::AccelerationStructureBuildGeometryInfoKHR>
                accelerationBuildGeometryInfos;
            accelerationBuildGeometryInfos.reserve(mPackedDrawData.persistentDrawData.size());
            std::vector<vk::AccelerationStructureBuildRangeInfoKHR*>
                accelerationStructureBuildRangeInfos;

            for (uint32_t index = 0; index < mPackedDrawData.persistentDrawData.size(); ++index) {
                BlasResources& blasResources = mRaytracingScene.blasResources[index];
                const PersistentDrawData& drawData = mPackedDrawData.persistentDrawData[index];
                TemporaryBlasBuildData& tempBlasData = blasBuildData[index];
                if (drawData.alphaMode == static_cast<uint32_t>(Material::AlphaMode::Blended)) {
                    continue;
                }
                vk::AccelerationStructureCreateInfoKHR accelerationStructureCreateInfo =
                    vk::AccelerationStructureCreateInfoKHR()
                        .setBuffer(mRaytracingScene.mBLASBuffer->GetBufferHandle())
                        .setSize(tempBlasData.blasBufferSize)
                        .setOffset(tempBlasData.blasBufferOffset)
                        .setType(vk::AccelerationStructureTypeKHR::eBottomLevel);

                blasResources.mBLAS = VKRT_ASSERT_VK(logicalDevice.createAccelerationStructureKHR(
                    accelerationStructureCreateInfo,
                    nullptr,
                    mContext->GetDevice()->GetDispatcher()));

                tempBlasData.scratchBuffer = mContext->GetDevice()->CreateBuffer(
                    tempBlasData.scratchSize,
                    vk::BufferUsageFlagBits::eStorageBuffer |
                        vk::BufferUsageFlagBits::eShaderDeviceAddress,
                    VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                        VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT);

                accelerationBuildGeometryInfos.push_back(
                    vk::AccelerationStructureBuildGeometryInfoKHR()
                        .setType(vk::AccelerationStructureTypeKHR::eBottomLevel)
                        .setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
                        .setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
                        .setDstAccelerationStructure(blasResources.mBLAS)
                        .setGeometries(tempBlasData.accelerationStructureGeometry)
                        .setScratchData(tempBlasData.scratchBuffer->GetDeviceAddress()));

                const uint32_t primitiveCount = drawData.indexCount / 3;
                tempBlasData.accelerationStructureBuildRangeInfo =
                    vk::AccelerationStructureBuildRangeInfoKHR()
                        .setPrimitiveCount(primitiveCount)
                        .setPrimitiveOffset(0)
                        .setFirstVertex(drawData.vertexOffset)
                        .setTransformOffset(0);
                accelerationStructureBuildRangeInfos.push_back(
                    &tempBlasData.accelerationStructureBuildRangeInfo);
            }

            commandBuffer.buildAccelerationStructuresKHR(
                accelerationBuildGeometryInfos,
                accelerationStructureBuildRangeInfos,
                mContext->GetDevice()->GetDispatcher());

            VKRT_ASSERT_VK(commandBuffer.end());
            mContext->GetDevice()->SubmitCommandAndFlush(commandBuffer);
            mContext->GetDevice()->DestroyCommand(commandBuffer);
        }

        {
            vk::CommandBuffer commandBuffer = mContext->GetDevice()->CreateCommandBuffer();
            VKRT_ASSERT_VK(commandBuffer.begin(vk::CommandBufferBeginInfo{}));

            std::vector<vk::AccelerationStructureInstanceKHR> instances;
            instances.reserve(mPackedDrawData.persistentDrawData.size());
            for (uint32_t index = 0; index < mPackedDrawData.persistentDrawData.size(); ++index) {
                BlasResources& blasResources = mRaytracingScene.blasResources[index];
                const PersistentDrawData& drawData = mPackedDrawData.persistentDrawData[index];
                const MeshData& meshData = mPackedDrawData.perMeshData[drawData.meshIndex];
                if (drawData.alphaMode == static_cast<uint32_t>(Material::AlphaMode::Blended)) {
                    continue;
                }

                vk::AccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo =
                    vk::AccelerationStructureDeviceAddressInfoKHR().setAccelerationStructure(
                        blasResources.mBLAS);
                blasResources.mBLASAddress = logicalDevice.getAccelerationStructureAddressKHR(
                    accelerationDeviceAddressInfo,
                    mContext->GetDevice()->GetDispatcher());

                const glm::mat4& transform = glm::transpose(meshData.transform);
                VkTransformMatrixKHR transformMatrix =
                    *(reinterpret_cast<const VkTransformMatrixKHR*>(&transform));
                instances.emplace_back(
                    vk::AccelerationStructureInstanceKHR()
                        .setTransform(transformMatrix)
                        .setInstanceCustomIndex(index)
                        .setAccelerationStructureReference(blasResources.mBLASAddress)
                        .setMask(0xFF)
                        .setInstanceShaderBindingTableRecordOffset(0)
                        .setFlags(vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable));
            }

            const size_t instanceDataSize =
                instances.size() * sizeof(vk::AccelerationStructureInstanceKHR);
            mRaytracingScene.mInstancesBuffer = mContext->GetDevice()->CreateBuffer(
                instanceDataSize,
                vk::BufferUsageFlagBits::eShaderDeviceAddress |
                    vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                    VMA_ALLOCATION_CREATE_MAPPED_BIT |
                    VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT);
            uint8_t* instanceData = mRaytracingScene.mInstancesBuffer->MapBuffer();
            std::copy_n(
                reinterpret_cast<uint8_t*>(instances.data()),
                instanceDataSize,
                instanceData);
            mRaytracingScene.mInstancesBuffer->UnmapBuffer();
            const vk::DeviceAddress instanceBufferAddress =
                mRaytracingScene.mInstancesBuffer->GetDeviceAddress();

            vk::AccelerationStructureGeometryInstancesDataKHR instancesData =
                vk::AccelerationStructureGeometryInstancesDataKHR()
                    .setArrayOfPointers(false)
                    .setData(instanceBufferAddress);
            vk::AccelerationStructureGeometryKHR accelerationStructureGeometry =
                vk::AccelerationStructureGeometryKHR()
                    .setGeometryType(vk::GeometryTypeKHR::eInstances)
                    .setFlags(vk::GeometryFlagBitsKHR::eOpaque)
                    .setGeometry(instancesData);

            vk::AccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo =
                vk::AccelerationStructureBuildGeometryInfoKHR()
                    .setType(vk::AccelerationStructureTypeKHR::eTopLevel)
                    .setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
                    .setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
                    .setGeometries(accelerationStructureGeometry);

            uint32_t instanceCount = static_cast<uint32_t>(instances.size());
            vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
            vk::AccelerationStructureBuildSizesInfoKHR buildSizesInfo =
                logicalDevice.getAccelerationStructureBuildSizesKHR(
                    vk::AccelerationStructureBuildTypeKHR::eDevice,
                    accelerationStructureBuildGeometryInfo,
                    instanceCount,
                    mContext->GetDevice()->GetDispatcher());

            {
                mRaytracingScene.mTLASBuffer = mContext->GetDevice()->CreateBuffer(
                    buildSizesInfo.accelerationStructureSize,
                    vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                        vk::BufferUsageFlagBits::eShaderDeviceAddress,
                    VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                        VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT);

                vk::AccelerationStructureCreateInfoKHR accelerationStructureCreateInfo =
                    vk::AccelerationStructureCreateInfoKHR()
                        .setBuffer(mRaytracingScene.mTLASBuffer->GetBufferHandle())
                        .setSize(buildSizesInfo.accelerationStructureSize)
                        .setType(vk::AccelerationStructureTypeKHR::eTopLevel);
                mRaytracingScene.mTLAS =
                    VKRT_ASSERT_VK(logicalDevice.createAccelerationStructureKHR(
                        accelerationStructureCreateInfo,
                        nullptr,
                        mContext->GetDevice()->GetDispatcher()));
            }

            ScopedRefPtr<VulkanBuffer> scratchBuffer = mContext->GetDevice()->CreateBuffer(
                buildSizesInfo.buildScratchSize,
                vk::BufferUsageFlagBits::eStorageBuffer |
                    vk::BufferUsageFlagBits::eShaderDeviceAddress,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                    VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT);

            vk::AccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo =
                vk::AccelerationStructureBuildGeometryInfoKHR()
                    .setType(vk::AccelerationStructureTypeKHR::eTopLevel)
                    .setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace)
                    .setMode(vk::BuildAccelerationStructureModeKHR::eBuild)
                    .setDstAccelerationStructure(mRaytracingScene.mTLAS)
                    .setGeometries(accelerationStructureGeometry)
                    .setScratchData(scratchBuffer->GetDeviceAddress())
                    .setSrcAccelerationStructure(nullptr);

            vk::AccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo =
                vk::AccelerationStructureBuildRangeInfoKHR()
                    .setPrimitiveCount(instanceCount)
                    .setPrimitiveOffset(0)
                    .setFirstVertex(0)
                    .setTransformOffset(0);

            commandBuffer.buildAccelerationStructuresKHR(
                accelerationBuildGeometryInfo,
                &accelerationStructureBuildRangeInfo,
                mContext->GetDevice()->GetDispatcher());

            vk::AccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo =
                vk::AccelerationStructureDeviceAddressInfoKHR().setAccelerationStructure(
                    mRaytracingScene.mTLAS);
            mRaytracingScene.mTLASAddress = logicalDevice.getAccelerationStructureAddressKHR(
                accelerationDeviceAddressInfo,
                mContext->GetDevice()->GetDispatcher());

            VKRT_ASSERT_VK(commandBuffer.end());
            mContext->GetDevice()->SubmitCommandAndFlush(commandBuffer);
            mContext->GetDevice()->DestroyCommand(commandBuffer);
        }
    }
}

Scene::~Scene() {
    vk::Device& logicalDevice = mContext->GetDevice()->GetLogicalDevice();
    logicalDevice.destroyAccelerationStructureKHR(
        mRaytracingScene.mTLAS,
        nullptr,
        mContext->GetDevice()->GetDispatcher());
    for (BlasResources& blasResources : mRaytracingScene.blasResources) {
        logicalDevice.destroyAccelerationStructureKHR(
            blasResources.mBLAS,
            nullptr,
            mContext->GetDevice()->GetDispatcher());
    }
}

}  // namespace VKRT