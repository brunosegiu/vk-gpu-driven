#include "Renderer.h"

#include <random>

#include "DebugUtils.h"
#include "Texture.h"

namespace VKRT {

struct CameraData {
    glm::mat4 projection;
    glm::mat4 viewProjection;
    glm::mat4 invViewProjection;
    glm::mat4 invProjection;
    glm::mat4 invView;
    glm::vec4 cameraPos;
};

struct LightData {
    glm::vec3 radiance;
    glm::vec3 direction;
    glm::mat4 viewProjection;
    uint32_t shadowTaps;
};

struct CullData {
    uint32_t ortho;
    glm::vec3 viewDirectionOrCameraPos;
    std::array<glm::vec4, 6> frustumPlanes;
    uint32_t globalDrawOffset;
    uint32_t maxDrawCount;
};

constexpr uint32_t raysPerProbe = 512;
constexpr glm::uvec3 probeGridCount(24u, 11u, 24u);
constexpr glm::uvec2 probeResolution(32u, 32u);
constexpr glm::vec3 probeSpacing(2.0f, 2.0f, 2.0f);
const glm::vec3 probeOrigin =
    glm::vec3(1.0f, 11.5f, 1.0f) - glm::vec3(probeGridCount) * probeSpacing * 0.5f;
constexpr float probeMaxRayLength = 1000.0f;
constexpr float probeMinRayLength = 0.01f;

struct DDGIData {
    glm::uvec3 probeGridCount;
    glm::vec3 probeGridOrigin;
    glm::vec3 probeSpacing;
    float minRayLength;
    float maxRayLength;
    glm::mat3 randomRotation;
};

DDGIData ddgiData{
    .probeGridCount = probeGridCount,
    .probeGridOrigin = probeOrigin,
    .probeSpacing = probeSpacing,
    .minRayLength = probeMinRayLength,
    .maxRayLength = probeMaxRayLength,
};

Renderer::Renderer(ScopedRefPtr<Context> context, ScopedRefPtr<Scene> scene)
    : mContext(context),
      mScene(scene),
      mCurrentFrameIndex(0),
      mMaterialsBuffer(nullptr),
      mScenePersistentDataBuffer(),
      mFreezeCulling(false),
      mHasResouces(false),
      mHasBoundResources(false) {
    ScopedRefPtr<InputManager> inputManager = mContext->GetWindow()->GetInputManager();
    inputManager->Subscribe(this);

    mCommandRing = new CommandRing(mContext);

    AddRenderTargets();
    AddPipelines();

    mUIRenderer = new UIRenderer(mContext, mMainRenderTarget);
}

void Renderer::AddRenderTargets() {
    const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();

    // Base pass resouces
    {
        mDepthBuffer = new Texture(
            mContext,
            imageSize.width,
            imageSize.height,
            vk::Format::eD32Sfloat,
            vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled);
        mDepthRenderTarget = new RenderTarget(mContext, mDepthBuffer);

        mMainRenderTarget =
            new RenderTarget(mContext, mContext->GetSwapchain()->GetRenderTargets());

        mVisibilityBuffer = new Texture(
            mContext,
            imageSize.width,
            imageSize.height,
            vk::Format::eR32Uint,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);
        mVisibilityBufferRT = new RenderTarget(mContext, mVisibilityBuffer);

        mGeometryPass = new RenderPass(
            mContext,
            {{.renderTarget = mVisibilityBufferRT,
              .loadOp = vk::AttachmentLoadOp::eClear,
              .initialLayout = vk::ImageLayout::eUndefined,
              .storeOp = vk::AttachmentStoreOp::eStore,
              .finalLayout = vk::ImageLayout::eColorAttachmentOptimal},
             {.renderTarget = mDepthRenderTarget,
              .loadOp = vk::AttachmentLoadOp::eClear,
              .initialLayout = vk::ImageLayout::eUndefined,
              .storeOp = vk::AttachmentStoreOp::eStore,
              .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal}});
    }

    // SSAO resources
    {
        mSSAOBuffer = new Texture(
            mContext,
            imageSize.width,
            imageSize.height,
            vk::Format::eR16Unorm,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
        mSSAORenderTarget = new RenderTarget(mContext, mSSAOBuffer);

        mSSAOPass = new RenderPass(
            mContext,
            {{.renderTarget = mSSAORenderTarget,
              .loadOp = vk::AttachmentLoadOp::eClear,
              .initialLayout = vk::ImageLayout::eUndefined,
              .storeOp = vk::AttachmentStoreOp::eStore,
              .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal}});

        mSSAOBlurredBuffer = new Texture(
            mContext,
            imageSize.width,
            imageSize.height,
            vk::Format::eR16Unorm,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
        mSSAOBlurredRenderTarget = new RenderTarget(mContext, mSSAOBlurredBuffer);

        mSSAOBlurPass = new RenderPass(
            mContext,
            {{.renderTarget = mSSAOBlurredRenderTarget,
              .loadOp = vk::AttachmentLoadOp::eClear,
              .initialLayout = vk::ImageLayout::eUndefined,
              .storeOp = vk::AttachmentStoreOp::eStore,
              .finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal}});
    }

    // Shading + transparent render targets
    {
        mShadePass = new RenderPass(
            mContext,
            {{.renderTarget = mMainRenderTarget,
              .loadOp = vk::AttachmentLoadOp::eClear,
              .initialLayout = vk::ImageLayout::eUndefined,
              .storeOp = vk::AttachmentStoreOp::eStore,
              .finalLayout = vk::ImageLayout::eColorAttachmentOptimal}});

        mTransparentPass = new RenderPass(
            mContext,
            {
                {.renderTarget = mMainRenderTarget,
                 .loadOp = vk::AttachmentLoadOp::eLoad,
                 .initialLayout = vk::ImageLayout::eColorAttachmentOptimal,
                 .storeOp = vk::AttachmentStoreOp::eStore,
                 .finalLayout = vk::ImageLayout::eColorAttachmentOptimal},
                {.renderTarget = mDepthRenderTarget,
                 .loadOp = vk::AttachmentLoadOp::eLoad,
                 .initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                 .storeOp = vk::AttachmentStoreOp::eStore,
                 .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal},
            });
    }

    // Shadow pass
    {
        mShadowMap = new Texture(
            mContext,
            4096,
            4096,
            vk::Format::eD16Unorm,
            vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);
        mDepthOnlyPassRenderTarget = new RenderTarget(mContext, mShadowMap);
        mDepthOnlyPass = new RenderPass(
            mContext,
            {.renderTarget = mDepthOnlyPassRenderTarget,
             .loadOp = vk::AttachmentLoadOp::eClear,
             .initialLayout = vk::ImageLayout::eUndefined,
             .storeOp = vk::AttachmentStoreOp::eStore,
             .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal});
    }

    // Probes
    {
        const uint32_t probeCount =
            ddgiData.probeGridCount.x * ddgiData.probeGridCount.y * ddgiData.probeGridCount.z;
        mProbeRayRadianceBuffer = new Texture(
            mContext,
            raysPerProbe,
            probeCount,
            1,
            vk::Format::eB10G11R11UfloatPack32,
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        mProbeRayDirectionDepthBuffer = new Texture(
            mContext,
            raysPerProbe,
            probeCount,
            1,
            vk::Format::eR16G16B16A16Sfloat,
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        mProbeIrradianceBuffer = new Texture(
            mContext,
            probeResolution.x,
            probeResolution.y,
            probeCount,
            vk::Format::eB10G11R11UfloatPack32,
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        mProbeDepthBuffer = new Texture(
            mContext,
            probeResolution.x,
            probeResolution.y,
            probeCount,
            vk::Format::eR16G16Sfloat,
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal);
    }
}

void Renderer::AddPipelines() {
    // Global resources
    {
        mScenePersistentDataParameter = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eStorageBuffer,
            ShaderParameter::UpdateFrequency::Once,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment |
                vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eRaygenKHR |
                vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR);

        mPerMeshParameters = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eStorageBuffer,
            ShaderParameter::UpdateFrequency::PerFrame,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment |
                vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eRaygenKHR |
                vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR);
    }

    // Culling resources
    {
        // Shadow culling resources
        auto createCullingResources = [&](Renderer::CullingPipelineResources& resources) {
            resources.cullingParameters = new ShaderParameterCollection(mContext);
            resources.cullingDataUniform = new ShaderParameterBuffer(
                mContext,
                vk::DescriptorType::eUniformBuffer,
                ShaderParameter::UpdateFrequency::PerFrame,
                vk::ShaderStageFlagBits::eCompute,
                sizeof(CullData),
                vk::BufferUsageFlagBits::eUniformBuffer,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                    VMA_ALLOCATION_CREATE_MAPPED_BIT);

            resources.indirectDrawBufferParameter = new ShaderParameterBuffer(
                mContext,
                vk::DescriptorType::eStorageBuffer,
                ShaderParameter::UpdateFrequency::PerFrame,
                vk::ShaderStageFlagBits::eCompute);

            resources.additionalDrawDataBufferParameter = new ShaderParameterBuffer(
                mContext,
                vk::DescriptorType::eStorageBuffer,
                ShaderParameter::UpdateFrequency::PerFrame,
                vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex |
                    vk::ShaderStageFlagBits::eFragment);

            resources.drawCallCountBufferParameter = new ShaderParameterBuffer(
                mContext,
                vk::DescriptorType::eStorageBuffer,
                ShaderParameter::UpdateFrequency::PerFrame,
                vk::ShaderStageFlagBits::eCompute);

            resources.cullingParameters->AddParameter(resources.cullingDataUniform);
            resources.cullingParameters->AddParameter(mPerMeshParameters);
            resources.cullingParameters->AddParameter(resources.indirectDrawBufferParameter);
            resources.cullingParameters->AddParameter(resources.drawCallCountBufferParameter);
            resources.cullingParameters->AddParameter(resources.additionalDrawDataBufferParameter);
            resources.cullingParameters->AddParameter(mScenePersistentDataParameter);

            resources.cullingPipeline = new ComputePipeline(
                mContext,
                resources.cullingParameters,
                {vk::ShaderStageFlagBits::eCompute, Resource::Id::CullingShader});
        };

        createCullingResources(mShadowPassCulling[Material::AlphaMode::Opaque]);
        createCullingResources(mShadowPassCulling[Material::AlphaMode::Masked]);
        createCullingResources(mBasePassCulling[Material::AlphaMode::Opaque]);
        createCullingResources(mBasePassCulling[Material::AlphaMode::Masked]);
        createCullingResources(mBasePassCulling[Material::AlphaMode::Blended]);
    }

    // Shadow pass parameters
    {
        mShadowCameraUniform = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eUniformBuffer,
            ShaderParameter::UpdateFrequency::PerFrame,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex |
                vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR |
                vk::ShaderStageFlagBits::eMissKHR,
            sizeof(LightData),
            vk::BufferUsageFlagBits::eUniformBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);

        const float anisotropy =
            mContext->GetDevice()->GetDeviceProperties().limits.maxSamplerAnisotropy;
        vk::SamplerCreateInfo materialSamplerCreateInfo =
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                .setMipLodBias(0.0f)
                .setCompareOp(vk::CompareOp::eNever)
                .setMinLod(0.0f)
                .setMaxLod(0.0f)
                .setAnisotropyEnable(true)
                .setMaxAnisotropy(anisotropy);

        mMaterialSampler = new ShaderParameterSampler(
            mContext,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eRaygenKHR |
                vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR,
            materialSamplerCreateInfo);

        vk::SamplerCreateInfo frameBufferSamplerCreateInfo =
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eNearest)
                .setMinFilter(vk::Filter::eNearest)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                .setMipLodBias(0.0f)
                .setCompareOp(vk::CompareOp::eNever)
                .setMinLod(0.0f)
                .setMaxLod(0.0f)
                .setAnisotropyEnable(false);

        mFrameBufferSampler = new ShaderParameterSampler(
            mContext,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eRaygenKHR |
                vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR |
                vk::ShaderStageFlagBits::eCompute,
            frameBufferSamplerCreateInfo);

        vk::SamplerCreateInfo irradianceSamplerCreateInfo =
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                .setMipLodBias(0.0f)
                .setCompareOp(vk::CompareOp::eNever)
                .setMinLod(0.0f)
                .setMaxLod(0.0f)
                .setAnisotropyEnable(false);
        mIrradianceSampler = new ShaderParameterSampler(
            mContext,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eRaygenKHR |
                vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR |
                vk::ShaderStageFlagBits::eCompute,
            irradianceSamplerCreateInfo);

        mMaterialsUniform = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eStorageBuffer,
            ShaderParameter::UpdateFrequency::Once,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex |
                vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR |
                vk::ShaderStageFlagBits::eMissKHR);

        mMaterialsTextures = new ShaderParameterImage(
            mContext,
            vk::DescriptorType::eSampledImage,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eRaygenKHR |
                vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR,
            4096,
            true);

        std::unordered_map<
            Material::AlphaMode,
            std::unordered_map<vk::ShaderStageFlagBits, Resource::Id>>
            stages{
                {Material::AlphaMode::Opaque,
                 {
                     {vk::ShaderStageFlagBits::eVertex, Resource::Id::DepthOnlyOpaqueVertexShader},
                     {vk::ShaderStageFlagBits::eFragment,
                      Resource::Id::DepthOnlyOpaqueFragmentShader},
                 }},
                {Material::AlphaMode::Masked,
                 {
                     {vk::ShaderStageFlagBits::eVertex,
                      Resource::Id::DepthOnlyAlphaMaskedVertexShader},
                     {vk::ShaderStageFlagBits::eFragment,
                      Resource::Id::DepthOnlyAlphaMaskedFragmentShader},
                 }}};

        for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
            if (alphaMode !=
                Material::AlphaMode::Blended) {  // Blended materials aren't shadow casters
                ScopedRefPtr<ShaderParameterCollection>& parameters =
                    mShadowPassPipeline[alphaMode].parameters;
                ScopedRefPtr<GraphicsPipeline>& pipeline = mShadowPassPipeline[alphaMode].pipeline;

                parameters = new ShaderParameterCollection(mContext);
                parameters->AddParameter(mShadowCameraUniform);
                parameters->AddParameter(mPerMeshParameters);
                parameters->AddParameter(
                    mShadowPassCulling[alphaMode].additionalDrawDataBufferParameter);
                parameters->AddParameter(mScenePersistentDataParameter);
                parameters->AddParameter(mMaterialSampler);
                parameters->AddParameter(mMaterialsUniform);
                parameters->AddParameter(mMaterialsTextures);

                VertexAttributeFlag geometryFlags =
                    alphaMode == Material::AlphaMode::Opaque
                        ? VertexAttributeFlag::Position
                        : VertexAttributeFlag(
                              VertexAttributeFlag::Position | VertexAttributeFlag::TexCoord);
                const std::vector<GeometryLayout> geometryLayout =
                    MeshSystem::GetGeometryLayout(geometryFlags);

                pipeline = new GraphicsPipeline(
                    mContext,
                    parameters,
                    stages[alphaMode],
                    mDepthOnlyPass,
                    geometryLayout,
                    GraphicsPipelineOptionals{
                        .enableDepthBias = true,
                        .depthBias = 1.0f,
                        .depthSlope = 1.0f * (2 + 1)});
            }
        }
    }

    // Base pass resources
    {
        mCameraUniform = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eUniformBuffer,
            ShaderParameter::UpdateFrequency::PerFrame,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex |
                vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR |
                vk::ShaderStageFlagBits::eMissKHR,
            sizeof(CameraData),
            vk::BufferUsageFlagBits::eUniformBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);

        mShadowMapUniform = new ShaderParameterImage(
            mContext,
            vk::DescriptorType::eSampledImage,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eRaygenKHR |
                vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR);

        std::unordered_map<
            Material::AlphaMode,
            std::unordered_map<vk::ShaderStageFlagBits, Resource::Id>>
            stages{
                {Material::AlphaMode::Opaque,
                 {
                     {vk::ShaderStageFlagBits::eVertex,
                      Resource::Id::GeometryPassOpaqueVertexShader},
                     {vk::ShaderStageFlagBits::eFragment,
                      Resource::Id::GeometryPassOpaqueFragmentShader},
                 }},
                {Material::AlphaMode::Masked,
                 {
                     {vk::ShaderStageFlagBits::eVertex,
                      Resource::Id::GeometryPassAlphaMaskedVertexShader},
                     {vk::ShaderStageFlagBits::eFragment,
                      Resource::Id::GeometryPassAlphaMaskedFragmentShader},
                 }},
                {Material::AlphaMode::Blended,
                 {
                     {vk::ShaderStageFlagBits::eVertex, Resource::Id::TransparentPassVertexShader},
                     {vk::ShaderStageFlagBits::eFragment,
                      Resource::Id::TransparentPassFragmentShader},
                 }}};

        for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
            bool isTransparentPass = alphaMode == Material::AlphaMode::Blended;
            bool isAlphaMaskedPass = alphaMode == Material::AlphaMode::Masked;
            ScopedRefPtr<ShaderParameterCollection>& parameters =
                mGeometryPassPipeline[alphaMode].parameters;
            ScopedRefPtr<GraphicsPipeline>& pipeline = mGeometryPassPipeline[alphaMode].pipeline;

            parameters = new ShaderParameterCollection(mContext);
            parameters->AddParameter(mCameraUniform);
            if (isTransparentPass) {
                parameters->AddParameter(mShadowCameraUniform);
            }
            parameters->AddParameter(mPerMeshParameters);
            parameters->AddParameter(mBasePassCulling[alphaMode].additionalDrawDataBufferParameter);
            parameters->AddParameter(mScenePersistentDataParameter);
            if (isTransparentPass || isAlphaMaskedPass) {
                parameters->AddParameter(mMaterialSampler);
                parameters->AddParameter(mMaterialsUniform);
                if (isTransparentPass) {
                    parameters->AddParameter(mShadowMapUniform);
                }
                parameters->AddParameter(mMaterialsTextures);
            }
            const VertexAttributeFlag geometryFlags =
                isTransparentPass ? VertexAttributeFlag::All
                : isAlphaMaskedPass
                    ? VertexAttributeFlag(
                          VertexAttributeFlag::Position | VertexAttributeFlag::TexCoord)
                    : VertexAttributeFlag::Position;
            const std::vector<GeometryLayout> geometryLayout =
                MeshSystem::GetGeometryLayout(geometryFlags);

            pipeline = new GraphicsPipeline(
                mContext,
                parameters,
                stages[alphaMode],
                isTransparentPass ? mTransparentPass : mGeometryPass,
                geometryLayout,
                {.enableCulling = !isAlphaMaskedPass, .enableBlending = isTransparentPass});
        }
    }

    // Shade pass resources
    {
        std::unordered_map<vk::ShaderStageFlagBits, Resource::Id> stages{
            {vk::ShaderStageFlagBits::eVertex, Resource::Id::VisibilityBufferShadeVertexShader},
            {vk::ShaderStageFlagBits::eFragment, Resource::Id::VisibilityBufferShadeFragmentShader},
        };

        mVisibilityBufferUniform = new ShaderParameterImage(
            mContext,
            vk::DescriptorType::eSampledImage,
            vk::ShaderStageFlagBits::eFragment);

        mSSAOTextureParameter = new ShaderParameterImage(
            mContext,
            vk::DescriptorType::eSampledImage,
            vk::ShaderStageFlagBits::eFragment);

        mDDGIProbeDataParameter = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eUniformBuffer,
            ShaderParameter::UpdateFrequency::PerFrame,
            vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eFragment,
            sizeof(DDGIData),
            vk::BufferUsageFlagBits::eUniformBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);

        mIndexBufferUniform = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eStorageBuffer,
            ShaderParameter::UpdateFrequency::Once,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eRaygenKHR |
                vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR);

        mPositionBufferUniform = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eStorageBuffer,
            ShaderParameter::UpdateFrequency::Once,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eRaygenKHR |
                vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR);
        mTexCoordBufferUniform = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eStorageBuffer,
            ShaderParameter::UpdateFrequency::Once,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eRaygenKHR |
                vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR);
        mNormalBufferUniform = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eStorageBuffer,
            ShaderParameter::UpdateFrequency::Once,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eRaygenKHR |
                vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR);
        mTangentBufferUniform = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eStorageBuffer,
            ShaderParameter::UpdateFrequency::Once,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eRaygenKHR |
                vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eMissKHR);

        mReadOnlyProbeIrradianceParameter = new ShaderParameterImage(
            mContext,
            vk::DescriptorType::eSampledImage,
            vk::ShaderStageFlagBits::eFragment);

        mReadOnlyProbeDepthParameter = new ShaderParameterImage(
            mContext,
            vk::DescriptorType::eSampledImage,
            vk::ShaderStageFlagBits::eFragment);

        mShadePassParameters = new ShaderParameterCollection(mContext);
        mShadePassParameters->AddParameter(mCameraUniform);
        mShadePassParameters->AddParameter(mShadowCameraUniform);
        mShadePassParameters->AddParameter(mPerMeshParameters);
        mShadePassParameters->AddParameter(mDDGIProbeDataParameter);
        mShadePassParameters->AddParameter(mScenePersistentDataParameter);
        mShadePassParameters->AddParameter(mMaterialSampler);
        mShadePassParameters->AddParameter(mFrameBufferSampler);
        mShadePassParameters->AddParameter(mIrradianceSampler);
        mShadePassParameters->AddParameter(mMaterialsUniform);
        mShadePassParameters->AddParameter(mShadowMapUniform);
        mShadePassParameters->AddParameter(mVisibilityBufferUniform);
        mShadePassParameters->AddParameter(mSSAOTextureParameter);
        mShadePassParameters->AddParameter(mReadOnlyProbeIrradianceParameter);
        mShadePassParameters->AddParameter(mReadOnlyProbeDepthParameter);
        mShadePassParameters->AddParameter(mIndexBufferUniform);
        mShadePassParameters->AddParameter(mPositionBufferUniform);
        mShadePassParameters->AddParameter(mTexCoordBufferUniform);
        mShadePassParameters->AddParameter(mNormalBufferUniform);
        mShadePassParameters->AddParameter(mTangentBufferUniform);
        mShadePassParameters->AddParameter(mMaterialsTextures);

        mShadePassPipeline = new GraphicsPipeline(
            mContext,
            mShadePassParameters,
            stages,
            mShadePass,
            std::vector<GeometryLayout>{},
            {.enableDepthTest = false});
    }

    // SSAO resources
    {
        std::unordered_map<vk::ShaderStageFlagBits, Resource::Id> stages{
            {vk::ShaderStageFlagBits::eVertex, Resource::Id::SSAOVertexShader},
            {vk::ShaderStageFlagBits::eFragment, Resource::Id::SSAOFragmentShader},
        };

        mDepthBufferParameter = new ShaderParameterImage(
            mContext,
            vk::DescriptorType::eSampledImage,
            vk::ShaderStageFlagBits::eFragment);

        mSSAOControlParameter = new ShaderParameterBuffer(
            mContext,
            vk::DescriptorType::eUniformBuffer,
            ShaderParameter::UpdateFrequency::PerFrame,
            vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex,
            sizeof(SSAOControlData),
            vk::BufferUsageFlagBits::eUniformBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);

        mSSAOParameters = new ShaderParameterCollection(mContext);
        mSSAOParameters->AddParameter(mCameraUniform);
        mSSAOParameters->AddParameter(mSSAOControlParameter);
        mSSAOParameters->AddParameter(mFrameBufferSampler);
        mSSAOParameters->AddParameter(mDepthBufferParameter);
        mSSAOPipeline = new GraphicsPipeline(
            mContext,
            mSSAOParameters,
            stages,
            mSSAOPass,
            std::vector<GeometryLayout>{},
            {.enableDepthTest = false});
    }

    // SSAO blur resources
    {
        std::unordered_map<vk::ShaderStageFlagBits, Resource::Id> stages{
            {vk::ShaderStageFlagBits::eVertex, Resource::Id::EdgeAwareBoxBlurVertexShader},
            {vk::ShaderStageFlagBits::eFragment, Resource::Id::EdgeAwareBoxBlurFragmentShader},
        };

        mSSAOBufferParameter = new ShaderParameterImage(
            mContext,
            vk::DescriptorType::eSampledImage,
            vk::ShaderStageFlagBits::eFragment);

        mSSAOBlurParameters = new ShaderParameterCollection(mContext);
        mSSAOBlurParameters->AddParameter(mCameraUniform);
        mSSAOBlurParameters->AddParameter(mFrameBufferSampler);
        mSSAOBlurParameters->AddParameter(mSSAOControlParameter);
        mSSAOBlurParameters->AddParameter(mDepthBufferParameter);
        mSSAOBlurParameters->AddParameter(mSSAOBufferParameter);
        mSSAOBlurPipeline = new GraphicsPipeline(
            mContext,
            mSSAOBlurParameters,
            stages,
            mSSAOBlurPass,
            std::vector<GeometryLayout>{},
            {.enableDepthTest = false});
    }

    // Raytracing resources
    mASParamater = new ShaderParameterAccelerationStructure(
        mContext,
        vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR);

    mWriteProbeRayRadianceParameter = new ShaderParameterImage(
        mContext,
        vk::DescriptorType::eStorageImage,
        vk::ShaderStageFlagBits::eRaygenKHR);

    mWriteProbeRayDirectionDepthParameter = new ShaderParameterImage(
        mContext,
        vk::DescriptorType::eStorageImage,
        vk::ShaderStageFlagBits::eRaygenKHR);

    const std::unordered_map<vk::ShaderStageFlagBits, std::vector<Resource::Id>> raytracingStages{
        {vk::ShaderStageFlagBits::eRaygenKHR, {Resource::Id::RaytraceProbeGenShader}},
        {vk::ShaderStageFlagBits::eClosestHitKHR, {Resource::Id::RaytraceProbeHitShader}},
        {vk::ShaderStageFlagBits::eMissKHR,
         std::vector<Resource::Id>{
             Resource::Id::RaytraceProbeMissShader,
             Resource::Id::RaytraceProbeShadowMissShader}}};

    mProbeRaytracingParameters = new ShaderParameterCollection(mContext);
    mProbeRaytracingParameters->AddParameter(mCameraUniform);
    mProbeRaytracingParameters->AddParameter(mShadowCameraUniform);
    mProbeRaytracingParameters->AddParameter(mPerMeshParameters);
    mProbeRaytracingParameters->AddParameter(mDDGIProbeDataParameter);

    mProbeRaytracingParameters->AddParameter(mScenePersistentDataParameter);
    mProbeRaytracingParameters->AddParameter(mASParamater);
    mProbeRaytracingParameters->AddParameter(mWriteProbeRayRadianceParameter);
    mProbeRaytracingParameters->AddParameter(mWriteProbeRayDirectionDepthParameter);
    mProbeRaytracingParameters->AddParameter(mMaterialSampler);
    mProbeRaytracingParameters->AddParameter(mFrameBufferSampler);
    mProbeRaytracingParameters->AddParameter(mMaterialsUniform);
    mProbeRaytracingParameters->AddParameter(mIndexBufferUniform);
    mProbeRaytracingParameters->AddParameter(mPositionBufferUniform);
    mProbeRaytracingParameters->AddParameter(mTexCoordBufferUniform);
    mProbeRaytracingParameters->AddParameter(mNormalBufferUniform);
    mProbeRaytracingParameters->AddParameter(mTangentBufferUniform);
    mProbeRaytracingParameters->AddParameter(mMaterialsTextures);

    mProbeRaytracingPipeline =
        new RaytracingPipeline(mContext, mProbeRaytracingParameters, raytracingStages);

    mReadOnlyProbeRadianceParameter = new ShaderParameterImage(
        mContext,
        vk::DescriptorType::eSampledImage,
        vk::ShaderStageFlagBits::eCompute);

    mReadOnlyProbeDirectionDepthParameter = new ShaderParameterImage(
        mContext,
        vk::DescriptorType::eSampledImage,
        vk::ShaderStageFlagBits::eCompute);

    mWriteProbeIrradianceParameter = new ShaderParameterImage(
        mContext,
        vk::DescriptorType::eStorageImage,
        vk::ShaderStageFlagBits::eCompute);

    mWriteProbeDepthParameter = new ShaderParameterImage(
        mContext,
        vk::DescriptorType::eStorageImage,
        vk::ShaderStageFlagBits::eCompute);

    mUpdateProbeParameters = new ShaderParameterCollection(mContext);
    mUpdateProbeParameters->AddParameter(mDDGIProbeDataParameter);
    mUpdateProbeParameters->AddParameter(mFrameBufferSampler);
    mUpdateProbeParameters->AddParameter(mReadOnlyProbeRadianceParameter);
    mUpdateProbeParameters->AddParameter(mReadOnlyProbeDirectionDepthParameter);
    mUpdateProbeParameters->AddParameter(mWriteProbeIrradianceParameter);
    mUpdateProbeParameters->AddParameter(mWriteProbeDepthParameter);

    mUpdateProbePipeline = new ComputePipeline(
        mContext,
        mUpdateProbeParameters,
        {vk::ShaderStageFlagBits::eCompute, Resource::Id::UpdateProbesShader});
}

void Renderer::AddResources() {
    mHasResouces = true;

    // Persistent resouces
    const Scene::PackedDrawData& drawData = mScene->GetPackedDrawData();
    {
        const size_t perDrawBufferSize =
            drawData.persistentDrawData.size() * sizeof(Scene::PersistentDrawData);
        VKRT_ASSERT(perDrawBufferSize > 0);
        mScenePersistentDataBuffer = mContext->GetDevice()->CreateBuffer(
            perDrawBufferSize,
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);
    }

    // Update materials
    {
        Scene::SceneMaterials sceneMaterials = mScene->GetMaterialProxies();
        size_t materialBufferSize = sceneMaterials.materials.size() * sizeof(Scene::MaterialProxy);
        VKRT_ASSERT(materialBufferSize > 0);
        mMaterialsBuffer = mContext->GetDevice()->CreateBuffer(
            materialBufferSize,
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);
    }

    // Per-frame resources
    {
        const size_t perMeshBufferSize = drawData.perMeshData.size() * sizeof(Scene::MeshData);

        const uint32_t bufferCount = mContext->GetMaxInFlightFrameCount();
        VKRT_ASSERT(perMeshBufferSize > 0);
        for (uint32_t bufferIndex = 0; bufferIndex < bufferCount; ++bufferIndex) {
            ScopedRefPtr<VulkanBuffer> perMeshBuffer = mContext->GetDevice()->CreateBuffer(
                perMeshBufferSize,
                vk::BufferUsageFlagBits::eStorageBuffer,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                    VMA_ALLOCATION_CREATE_MAPPED_BIT);
            mPerMeshBuffers.push_back(perMeshBuffer);
        }

        mPerMeshParameters->BindBuffers(mPerMeshBuffers);
    }

    auto createCullingResources = [&](Renderer::CullingPipelineResources& resources,
                                      Material::AlphaMode alphaMode) {
        const uint32_t drawCallCount = mScene->GetDrawCallCount(alphaMode);
        if (drawCallCount == 0) {
            return;
        }
        const uint32_t bufferCount = mContext->GetMaxInFlightFrameCount();

        resources.indirectDrawBuffers = mContext->GetDevice()->CreateBuffers(
            bufferCount,
            drawCallCount * sizeof(vk::DrawIndexedIndirectCommand),
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

        resources.additionalDrawDataBuffers = mContext->GetDevice()->CreateBuffers(
            bufferCount,
            drawCallCount * sizeof(uint32_t),
            vk::BufferUsageFlagBits::eStorageBuffer,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

        resources.drawCallCountBuffer = mContext->GetDevice()->CreateBuffers(
            bufferCount,
            sizeof(uint32_t),
            vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eTransferDst,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    };

    for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
        if (alphaMode != Material::AlphaMode::Blended) {  // Blended materials aren't shadow casters
            createCullingResources(mShadowPassCulling[alphaMode], alphaMode);
        }
        createCullingResources(mBasePassCulling[alphaMode], alphaMode);
    }
}

void Renderer::RemoveRenderTargets() {
    mShadePass = nullptr;
    mTransparentPass = nullptr;
    mShadowMap = nullptr;
    mVisibilityBuffer = nullptr;
    mGeometryPass = nullptr;
    mProbeDepthBuffer = nullptr;
    mProbeRayRadianceBuffer = nullptr;
    mProbeIrradianceBuffer = nullptr;

    mMainRenderTarget = nullptr;

    mSSAOBlurPass = nullptr;
    mSSAOBlurredRenderTarget = nullptr;
    mSSAOBlurredBuffer = nullptr;

    mSSAOPass = nullptr;
    mSSAORenderTarget = nullptr;
    mSSAOBuffer = nullptr;

    mDepthRenderTarget = nullptr;
    mDepthBuffer = nullptr;
}

void Renderer::RemovePipelines() {}

void Renderer::RemoveResources() {}

void Renderer::UpdatePersistentUniforms() {
    mHasBoundResources = true;

    const Scene::PackedDrawData& drawData = mScene->GetPackedDrawData();
    {
        const size_t perDrawBufferSize =
            drawData.persistentDrawData.size() * sizeof(Scene::PersistentDrawData);

        {
            uint8_t* buffer = mScenePersistentDataBuffer->MapBuffer();
            std::copy_n(
                reinterpret_cast<const uint8_t*>(drawData.persistentDrawData.data()),
                drawData.persistentDrawData.size() * sizeof(Scene::PersistentDrawData),
                buffer);
            mScenePersistentDataBuffer->UnmapBuffer();
        }

        mScenePersistentDataParameter->BindBuffer(mScenePersistentDataBuffer);
    }

    mShadowMapUniform->Bind(mShadowMap);
    mVisibilityBufferUniform->Bind(mVisibilityBuffer);

    // Update materials
    {
        uint32_t descriptorCount = 0;
        Scene::SceneMaterials sceneMaterials = mScene->GetMaterialProxies();
        uint8_t* buffer = mMaterialsBuffer->MapBuffer();
        std::copy_n(
            reinterpret_cast<const uint8_t*>(sceneMaterials.materials.data()),
            mMaterialsBuffer->GetBufferSize(),
            buffer);
        mMaterialsBuffer->UnmapBuffer();
        descriptorCount = sceneMaterials.textures.size();
        for (const ScopedRefPtr<Texture>& materialTexture : sceneMaterials.textures) {
            mMaterialsTextures->Bind(materialTexture);
        }
        mMaterialsUniform->BindBuffer(mMaterialsBuffer);
    }

    // Updte geometry buffers
    {
        mIndexBufferUniform->BindBuffer(mScene->GetMeshSystem()->GetIndexBuffer());
        mPositionBufferUniform->BindBuffer(mScene->GetMeshSystem()->GetVertexBuffer());
        mTexCoordBufferUniform->BindBuffer(mScene->GetMeshSystem()->GetTexCoordBuffer());
        mNormalBufferUniform->BindBuffer(mScene->GetMeshSystem()->GetNormalBuffer());
        mTangentBufferUniform->BindBuffer(mScene->GetMeshSystem()->GetTangentBuffer());
    }

    // Update SSAO bindings
    mDepthBufferParameter->Bind(mDepthBuffer);
    mSSAOBufferParameter->Bind(mSSAOBuffer);
    mSSAOTextureParameter->Bind(mSSAOBlurredBuffer);

    // DDGI
    {
        mASParamater->Bind(mScene->GetTLAS());

        // Shading
        mReadOnlyProbeIrradianceParameter->Bind(mProbeIrradianceBuffer);
        mReadOnlyProbeDepthParameter->Bind(mProbeDepthBuffer);

        // RT
        mWriteProbeRayRadianceParameter->Bind(mProbeRayRadianceBuffer);
        mWriteProbeRayDirectionDepthParameter->Bind(mProbeRayDirectionDepthBuffer);

        // Compute
        mReadOnlyProbeRadianceParameter->Bind(mProbeRayRadianceBuffer);
        mReadOnlyProbeDirectionDepthParameter->Bind(mProbeRayDirectionDepthBuffer);
        mWriteProbeIrradianceParameter->Bind(mProbeIrradianceBuffer);
        mWriteProbeDepthParameter->Bind(mProbeDepthBuffer);
    }
}

void Renderer::UpdateUniforms(Camera* camera, uint32_t imageIndex) {
    // Update content of per-draw parameters
    {
        const Scene::PackedDrawData& drawData = mScene->GetPackedDrawData();
        {
            ScopedRefPtr<VulkanBuffer> currentBuffer = mPerMeshBuffers[imageIndex];
            uint8_t* buffer = currentBuffer->MapBuffer();
            std::copy_n(
                reinterpret_cast<const uint8_t*>(drawData.perMeshData.data()),
                drawData.perMeshData.size() * sizeof(Scene::MeshData),
                buffer);
            currentBuffer->UnmapBuffer();
        }
    }

    // Update culling parameters
    auto updateCullingResources = [&](Renderer::CullingPipelineResources& resources,
                                      Material::AlphaMode alphaMode,
                                      CullData cullData) {
        const uint32_t drawCallCount = mScene->GetDrawCallCount(alphaMode);
        if (drawCallCount == 0) {
            return;
        }
        if (!mFreezeCulling) {
            resources.cullingDataUniform->Write(
                imageIndex,
                reinterpret_cast<uint8_t*>(&cullData),
                sizeof(CullData));
        }
        resources.indirectDrawBufferParameter->BindBuffers(resources.indirectDrawBuffers);
        resources.additionalDrawDataBufferParameter->BindBuffers(
            resources.additionalDrawDataBuffers);
        resources.drawCallCountBufferParameter->BindBuffers(resources.drawCallCountBuffer);
    };

    for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
        if (alphaMode != Material::AlphaMode::Blended) {  // Blended materials aren't shadow casters
            CullData shadowCullingData{
                .ortho = 1,
                .viewDirectionOrCameraPos = mScene->GetLight().GetDirection(),
                .frustumPlanes = ViewFrustum(mScene->GetLight().ComputeShadowMatrix()).GetPlanes(),
                .globalDrawOffset = static_cast<uint32_t>(mScene->GetDrawCallOffset(alphaMode)),
                .maxDrawCount = static_cast<uint32_t>(mScene->GetDrawCallCount(alphaMode))};
            updateCullingResources(mShadowPassCulling[alphaMode], alphaMode, shadowCullingData);
        }

        CullData mainCameraCullingData{
            .ortho = 0,
            .viewDirectionOrCameraPos = camera->GetPosition(),
            .frustumPlanes = camera->GetViewFrustum().GetPlanes(),
            .globalDrawOffset = static_cast<uint32_t>(mScene->GetDrawCallOffset(alphaMode)),
            .maxDrawCount = static_cast<uint32_t>(mScene->GetDrawCallCount(alphaMode))};
        updateCullingResources(mBasePassCulling[alphaMode], alphaMode, mainCameraCullingData);
    }

    // Shadow camera parameters
    {
        DirectionalLight& light = mScene->GetLight();
        glm::mat4 shadowMatrix = light.ComputeShadowMatrix();
        LightData cameraMatrices{
            .radiance = light.GetRadiance(),
            .direction = light.GetDirection(),
            .viewProjection = shadowMatrix,
            .shadowTaps = mUIRenderer->GetShadowTaps()};
        mShadowCameraUniform->Write(
            imageIndex,
            reinterpret_cast<uint8_t*>(&cameraMatrices),
            sizeof(LightData));
    }

    // Update camera uniform
    {
        CameraData cameraMatrices{
            .projection = camera->GetProjectionTransform(),
            .viewProjection = camera->GetProjectionTransform() * camera->GetViewTransform(),
            .invViewProjection =
                glm::inverse(camera->GetProjectionTransform() * camera->GetViewTransform()),
            .invProjection = glm::inverse(camera->GetProjectionTransform()),
            .invView = glm::inverse(camera->GetViewTransform()),
            .cameraPos = glm::vec4(camera->GetPosition(), 0.0f)};
        mCameraUniform->Write(
            imageIndex,
            reinterpret_cast<uint8_t*>(&cameraMatrices),
            sizeof(CameraData));
    }

    // SSAO
    {
        SSAOControlData ssaoControl = mUIRenderer->GetSSAOControlData();
        mSSAOControlParameter->Write(
            imageIndex,
            reinterpret_cast<uint8_t*>(&ssaoControl),
            sizeof(SSAOControlData));
    }

    // DDGI
    {
        auto randomRotation = []() {
            static std::mt19937 gen{std::random_device{}()};
            static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            float u1 = dist(gen);
            float u2 = dist(gen);
            float u3 = dist(gen);
            glm::quat quat = glm::quat(glm::vec3(u1, u2, u3) * 2.0f * glm::pi<float>());
            return glm::toMat3(quat);
        };

        ddgiData.randomRotation = randomRotation();
        mDDGIProbeDataParameter->Write(
            imageIndex,
            reinterpret_cast<uint8_t*>(&ddgiData),
            sizeof(DDGIData));
    }
}

void Renderer::BeginMarker(const vk::CommandBuffer& commandBuffer, const std::string& name) {
#ifdef VKRT_ENABLE_VALIDATION
    commandBuffer.beginDebugUtilsLabelEXT(
        vk::DebugUtilsLabelEXT().setPLabelName(name.c_str()),
        mContext->GetDevice()->GetDispatcher());
#endif
}

void Renderer::EndMarker(const vk::CommandBuffer& commandBuffer) {
#ifdef VKRT_ENABLE_VALIDATION
    commandBuffer.endDebugUtilsLabelEXT(mContext->GetDevice()->GetDispatcher());
#endif
}

std::string AlphaModeToStr(const Material::AlphaMode& alphaMode) {
    switch (alphaMode) {
        case Material::AlphaMode::Opaque:
            return "Opaque";
        case Material::AlphaMode::Masked:
            return "Masked";
        case Material::AlphaMode::Blended:
            return "Blended";
    }
    return "";
}

void Renderer::Render(Camera* camera) {
    mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mContext->GetMaxInFlightFrameCount();
    CommandRing::CommandResources command = mCommandRing->Cycle();

    mContext->GetSwapchain()->AcquireNextImage(mCurrentFrameIndex);

    mScene->Update();
    mUIRenderer->Update();

    if (!mHasResouces) {
        AddResources();
    }
    if (!mHasBoundResources) {
        UpdatePersistentUniforms();
    }
    UpdateUniforms(camera, mCurrentFrameIndex);

    {
        VKRT_ASSERT_VK(command.buffer.begin(vk::CommandBufferBeginInfo{}));

        const vk::Extent2D& imageSize = mContext->GetSwapchain()->GetExtent();

        const auto dispatchCulling = [&](CullingPipelineResources& resources,
                                         uint32_t maxDrawCallCount) {
            if (maxDrawCallCount == 0) {
                return;
            }

            ScopedRefPtr<VulkanBuffer> drawCallCountBuffer =
                resources.drawCallCountBuffer[mCurrentFrameIndex];
            ScopedRefPtr<VulkanBuffer> indirectDrawBuffer =
                resources.indirectDrawBuffers[mCurrentFrameIndex];
            ScopedRefPtr<VulkanBuffer> additionalDrawDataBuffer =
                resources.additionalDrawDataBuffers[mCurrentFrameIndex];
            // Write 0 in drawCallCountBuffer
            {
                std::vector<vk::BufferMemoryBarrier> bufferBarriers = VulkanBuffer::GetBarriers(
                    {drawCallCountBuffer},
                    vk::PipelineStageFlagBits::eDrawIndirect,
                    vk::PipelineStageFlagBits::eTransfer);

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eDrawIndirect,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::DependencyFlags{},
                    {},
                    bufferBarriers,
                    {});

                command.buffer
                    .fillBuffer(drawCallCountBuffer->GetBufferHandle(), 0, sizeof(uint32_t), 0);
            }

            {
                std::vector<vk::BufferMemoryBarrier> bufferBarriers = VulkanBuffer::GetBarriers(
                    {drawCallCountBuffer},
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eComputeShader);

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::DependencyFlags{},
                    {},
                    bufferBarriers,
                    {});
            }

            {
                std::vector<vk::BufferMemoryBarrier> bufferBarriers = VulkanBuffer::GetBarriers(
                    {indirectDrawBuffer, additionalDrawDataBuffer},
                    vk::PipelineStageFlagBits::eDrawIndirect,
                    vk::PipelineStageFlagBits::eComputeShader);

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eDrawIndirect,
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::DependencyFlags{},
                    {},
                    bufferBarriers,
                    {});
            }

            command.buffer.bindPipeline(
                vk::PipelineBindPoint::eCompute,
                resources.cullingPipeline->GetPipelineHandle());

            std::vector<vk::DescriptorSet> cullingDescriptorSets =
                resources.cullingParameters->GetDescriptorSets(mCurrentFrameIndex);

            command.buffer.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute,
                resources.cullingPipeline->GetPipelineLayout(),
                0,
                cullingDescriptorSets,
                nullptr);

            command.buffer.dispatch((maxDrawCallCount / 64) + 1, 1, 1);

            // TODO: Wait until before actual draws are dispatched, this is too early
            {
                std::vector<vk::BufferMemoryBarrier> bufferBarriers = VulkanBuffer::GetBarriers(
                    {indirectDrawBuffer, additionalDrawDataBuffer, drawCallCountBuffer},
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eComputeShader);

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eAllGraphics,
                    vk::DependencyFlags{},
                    {},
                    bufferBarriers,
                    {});
            }
        };

        BeginMarker(command.buffer, "DDGI");
        {
            BeginMarker(command.buffer, "Trace rays");
            {
                {
                    std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                        {{mProbeRayRadianceBuffer,
                          vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::ImageLayout::eGeneral},
                         {mProbeRayDirectionDepthBuffer,
                          vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::ImageLayout::eGeneral}});

                    command.buffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                        vk::DependencyFlags{},
                        {},
                        {},
                        imageBarriers);
                }

                command.buffer.bindPipeline(
                    vk::PipelineBindPoint::eRayTracingKHR,
                    mProbeRaytracingPipeline->GetPipelineHandle());
                std::vector<vk::DescriptorSet> descriptorSets =
                    mProbeRaytracingParameters->GetDescriptorSets(mCurrentFrameIndex);
                command.buffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eRayTracingKHR,
                    mProbeRaytracingPipeline->GetPipelineLayout(),
                    0,
                    descriptorSets,
                    nullptr);

                const uint32_t probeCount = ddgiData.probeGridCount.x * ddgiData.probeGridCount.y *
                                            ddgiData.probeGridCount.z;

                const RaytracingPipeline::RayTracingTablesRef& tableRef =
                    mProbeRaytracingPipeline->GetTablesRef();
                command.buffer.traceRaysKHR(
                    tableRef.rayGen,
                    tableRef.rayMiss,
                    tableRef.rayHit,
                    tableRef.callable,
                    raysPerProbe,
                    probeCount,
                    1,
                    mContext->GetDevice()->GetDispatcher());

                {
                    std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                        vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                        vk::PipelineStageFlagBits::eComputeShader,
                        {{mProbeRayRadianceBuffer,
                          vk::ImageLayout::eGeneral,
                          vk::ImageLayout::eShaderReadOnlyOptimal},
                         {mProbeRayDirectionDepthBuffer,
                          vk::ImageLayout::eGeneral,
                          vk::ImageLayout::eShaderReadOnlyOptimal}});

                    command.buffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::DependencyFlags{},
                        {},
                        {},
                        imageBarriers);
                }
            }
            EndMarker(command.buffer);

            BeginMarker(command.buffer, "Update probes");
            {
                {
                    std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::PipelineStageFlagBits::eComputeShader,
                        {
                            {mProbeIrradianceBuffer,
                             vk::ImageLayout::eShaderReadOnlyOptimal,
                             vk::ImageLayout::eGeneral},
                            {mProbeDepthBuffer,
                             vk::ImageLayout::eShaderReadOnlyOptimal,
                             vk::ImageLayout::eGeneral},
                        });

                    command.buffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::DependencyFlags{},
                        {},
                        {},
                        imageBarriers);
                }
                const uint32_t probeCount = ddgiData.probeGridCount.x * ddgiData.probeGridCount.y *
                                            ddgiData.probeGridCount.z;
                {
                    command.buffer.bindPipeline(
                        vk::PipelineBindPoint::eCompute,
                        mUpdateProbePipeline->GetPipelineHandle());

                    std::vector<vk::DescriptorSet> updateProbeDescriptors =
                        mUpdateProbeParameters->GetDescriptorSets(mCurrentFrameIndex);

                    command.buffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eCompute,
                        mUpdateProbePipeline->GetPipelineLayout(),
                        0,
                        updateProbeDescriptors,
                        nullptr);

                    static_assert(probeResolution.x % 8 == 0 && probeResolution.y % 8 == 0);
                    command.buffer.dispatch(
                        probeResolution.x / 8,
                        probeResolution.y / 8,
                        probeCount);
                }

                {
                    std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::PipelineStageFlagBits::eFragmentShader,
                        {
                            {mProbeIrradianceBuffer,
                             vk::ImageLayout::eGeneral,
                             vk::ImageLayout::eShaderReadOnlyOptimal},
                            {mProbeDepthBuffer,
                             vk::ImageLayout::eGeneral,
                             vk::ImageLayout::eShaderReadOnlyOptimal},
                        });

                    command.buffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::DependencyFlags{},
                        {},
                        {},
                        imageBarriers);
                }
            }
            EndMarker(command.buffer);
        }
        EndMarker(command.buffer);

        BeginMarker(command.buffer, "Shadow culling");
        for (const Material::AlphaMode alphaMode : Material::AlphaModes) {
            if (alphaMode !=
                Material::AlphaMode::Blended) {  // Blended materials aren't shadow casters
                BeginMarker(command.buffer, AlphaModeToStr(alphaMode));
                dispatchCulling(mShadowPassCulling[alphaMode], mScene->GetDrawCallCount(alphaMode));
                EndMarker(command.buffer);
            }
        }
        EndMarker(command.buffer);

        BeginMarker(command.buffer, "Basepass culling");
        for (const Material::AlphaMode& alphaMode : Material::AlphaModes) {
            BeginMarker(command.buffer, AlphaModeToStr(alphaMode));
            dispatchCulling(mBasePassCulling[alphaMode], mScene->GetDrawCallCount(alphaMode));
            EndMarker(command.buffer);
        }
        EndMarker(command.buffer);

        // Shadow pass
        {
            BeginMarker(command.buffer, "Shadowmap Rendering");
            {
                // Transition shadow map to depth attachment
                std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eVertexShader,
                    {{mShadowMap,
                      vk::ImageLayout::eShaderReadOnlyOptimal,
                      vk::ImageLayout::eDepthAttachmentOptimal}});

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eVertexShader,
                    vk::DependencyFlags{},
                    {},
                    {},
                    imageBarriers);
            }

            const std::vector<vk::ClearValue> clearValues{
                vk::ClearDepthStencilValue(1.0f, 0),
            };
            const vk::RenderPassBeginInfo renderPassBeginInfo =
                vk::RenderPassBeginInfo()
                    .setRenderPass(mDepthOnlyPass->GetRenderPassHandle())
                    .setFramebuffer(mDepthOnlyPass->GetFramebufferHandle())
                    .setRenderArea({vk::Offset2D{0, 0}, mShadowMap->GetExtent()})
                    .setClearValues(clearValues);
            command.buffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

            {
                const vk::Viewport viewport{
                    0.0f,
                    0.0f,
                    static_cast<float>(mShadowMap->GetWidth()),
                    static_cast<float>(mShadowMap->GetHeight()),
                    0.0f,
                    1.0f};
                command.buffer.setViewport(0, viewport);

                const vk::Rect2D scissor =
                    vk::Rect2D().setOffset(0).setExtent(mShadowMap->GetExtent());
                command.buffer.setScissor(0, scissor);
            }

            for (const Material::AlphaMode alphaMode : Material::AlphaModes) {
                // Skip transparent objects while rendering shadows
                uint32_t drawCallCount = mScene->GetDrawCallCount(alphaMode);
                if (alphaMode == Material::AlphaMode::Blended || drawCallCount == 0) {
                    continue;
                }
                BeginMarker(command.buffer, AlphaModeToStr(alphaMode));

                command.buffer.bindPipeline(
                    vk::PipelineBindPoint::eGraphics,
                    mShadowPassPipeline[alphaMode].pipeline->GetPipelineHandle());

                std::vector<vk::DescriptorSet> descriptorSets =
                    mShadowPassPipeline[alphaMode].parameters->GetDescriptorSets(
                        mCurrentFrameIndex);

                command.buffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    mShadowPassPipeline[alphaMode].pipeline->GetPipelineLayout(),
                    0,
                    descriptorSets,
                    nullptr);

                {
                    mScene->GetMeshSystem()->BindBuffers(
                        command.buffer,
                        alphaMode == Material::AlphaMode::Opaque
                            ? VertexAttributeFlag::Position
                            : VertexAttributeFlag(
                                  VertexAttributeFlag::Position | VertexAttributeFlag::TexCoord));

                    uint32_t maxDrawIndirectCount =
                        mContext->GetDevice()->GetDeviceProperties().limits.maxDrawIndirectCount;
                    VKRT_ASSERT(maxDrawIndirectCount >= drawCallCount);

                    command.buffer.drawIndexedIndirectCount(
                        mShadowPassCulling[alphaMode]
                            .indirectDrawBuffers[mCurrentFrameIndex]
                            ->GetBufferHandle(),
                        0,
                        mShadowPassCulling[alphaMode]
                            .drawCallCountBuffer[mCurrentFrameIndex]
                            ->GetBufferHandle(),
                        0,
                        drawCallCount,
                        sizeof(VkDrawIndexedIndirectCommand));
                }
                EndMarker(command.buffer);
            }

            command.buffer.endRenderPass();
            EndMarker(command.buffer);
        }

        // Base pass
        {
            BeginMarker(command.buffer, "Base pass");

            {
                std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    {{mVisibilityBuffer,
                      vk::ImageLayout::eShaderReadOnlyOptimal,
                      vk::ImageLayout::eColorAttachmentOptimal}});

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::DependencyFlags{},
                    {},
                    {},
                    imageBarriers);
            }

            const std::vector<vk::ClearValue> clearValues{
                vk::ClearColorValue(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF),
                vk::ClearDepthStencilValue(1.0f, 0),
            };
            const vk::RenderPassBeginInfo renderPassBeginInfo =
                vk::RenderPassBeginInfo()
                    .setRenderPass(mGeometryPass->GetRenderPassHandle())
                    .setFramebuffer(mGeometryPass->GetFramebufferHandle())
                    .setRenderArea({vk::Offset2D{0, 0}, imageSize})
                    .setClearValues(clearValues);
            command.buffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

            {
                const vk::Viewport viewport{
                    0.0f,
                    0.0f,
                    static_cast<float>(imageSize.width),
                    static_cast<float>(imageSize.height),
                    0.0f,
                    1.0f};
                command.buffer.setViewport(0, viewport);

                const vk::Rect2D scissor = vk::Rect2D().setOffset(0).setExtent(
                    vk::Extent2D{imageSize.width, imageSize.height});
                command.buffer.setScissor(0, scissor);
            }

            for (const Material::AlphaMode alphaMode : Material::AlphaModes) {
                uint32_t drawCallCount = mScene->GetDrawCallCount(alphaMode);
                if (alphaMode == Material::AlphaMode::Blended || drawCallCount == 0) {
                    continue;
                }
                BeginMarker(command.buffer, AlphaModeToStr(alphaMode));
                command.buffer.bindPipeline(
                    vk::PipelineBindPoint::eGraphics,
                    mGeometryPassPipeline[alphaMode].pipeline->GetPipelineHandle());

                std::vector<vk::DescriptorSet> descriptorSets =
                    mGeometryPassPipeline[alphaMode].parameters->GetDescriptorSets(
                        mCurrentFrameIndex);

                command.buffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    mGeometryPassPipeline[alphaMode].pipeline->GetPipelineLayout(),
                    0,
                    descriptorSets,
                    nullptr);

                {
                    mScene->GetMeshSystem()->BindBuffers(
                        command.buffer,
                        alphaMode == Material::AlphaMode::Opaque
                            ? VertexAttributeFlag::Position
                            : VertexAttributeFlag(
                                  VertexAttributeFlag::Position | VertexAttributeFlag::TexCoord));

                    uint32_t maxDrawIndirectCount =
                        mContext->GetDevice()->GetDeviceProperties().limits.maxDrawIndirectCount;
                    VKRT_ASSERT(maxDrawIndirectCount >= drawCallCount);

                    command.buffer.drawIndexedIndirectCount(
                        mBasePassCulling[alphaMode]
                            .indirectDrawBuffers[mCurrentFrameIndex]
                            ->GetBufferHandle(),
                        0,
                        mBasePassCulling[alphaMode]
                            .drawCallCountBuffer[mCurrentFrameIndex]
                            ->GetBufferHandle(),
                        0,
                        drawCallCount,
                        sizeof(VkDrawIndexedIndirectCommand));
                }
                EndMarker(command.buffer);
            }
            command.buffer.endRenderPass();
            EndMarker(command.buffer);
        }

        // SSAO pass
        {
            BeginMarker(command.buffer, "SSAO");
            {
                BeginMarker(command.buffer, "SSAO draw");
                {
                    std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                        vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits::eFragmentShader,
                        {{mDepthBuffer,
                          vk::ImageLayout::eDepthAttachmentOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal}});

                    command.buffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::DependencyFlags{},
                        {},
                        {},
                        imageBarriers);
                }

                const std::vector<vk::ClearValue> clearValues{
                    vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f),
                };
                const vk::RenderPassBeginInfo renderPassBeginInfo =
                    vk::RenderPassBeginInfo()
                        .setRenderPass(mSSAOPass->GetRenderPassHandle())
                        .setFramebuffer(mSSAOPass->GetFramebufferHandle())
                        .setRenderArea({vk::Offset2D{0, 0}, imageSize})
                        .setClearValues(clearValues);
                command.buffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

                {
                    const vk::Viewport viewport{
                        0.0f,
                        0.0f,
                        static_cast<float>(imageSize.width),
                        static_cast<float>(imageSize.height),
                        0.0f,
                        1.0f};
                    command.buffer.setViewport(0, viewport);

                    const vk::Rect2D scissor = vk::Rect2D().setOffset(0).setExtent(
                        vk::Extent2D{imageSize.width, imageSize.height});
                    command.buffer.setScissor(0, scissor);
                }

                command.buffer.bindPipeline(
                    vk::PipelineBindPoint::eGraphics,
                    mSSAOPipeline->GetPipelineHandle());

                std::vector<vk::DescriptorSet> descriptorSets =
                    mSSAOParameters->GetDescriptorSets(mCurrentFrameIndex);

                command.buffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    mSSAOPipeline->GetPipelineLayout(),
                    0,
                    descriptorSets,
                    nullptr);

                command.buffer.draw(3, 1, 0, 0);

                command.buffer.endRenderPass();
                EndMarker(command.buffer);
            }

            BeginMarker(command.buffer, "SSAO blur");
            {
                const std::vector<vk::ClearValue> clearValues{
                    vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f),
                };
                const vk::RenderPassBeginInfo renderPassBeginInfo =
                    vk::RenderPassBeginInfo()
                        .setRenderPass(mSSAOBlurPass->GetRenderPassHandle())
                        .setFramebuffer(mSSAOBlurPass->GetFramebufferHandle())
                        .setRenderArea({vk::Offset2D{0, 0}, imageSize})
                        .setClearValues(clearValues);
                command.buffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

                {
                    const vk::Viewport viewport{
                        0.0f,
                        0.0f,
                        static_cast<float>(imageSize.width),
                        static_cast<float>(imageSize.height),
                        0.0f,
                        1.0f};
                    command.buffer.setViewport(0, viewport);

                    const vk::Rect2D scissor = vk::Rect2D().setOffset(0).setExtent(
                        vk::Extent2D{imageSize.width, imageSize.height});
                    command.buffer.setScissor(0, scissor);
                }

                command.buffer.bindPipeline(
                    vk::PipelineBindPoint::eGraphics,
                    mSSAOBlurPipeline->GetPipelineHandle());

                std::vector<vk::DescriptorSet> descriptorSets =
                    mSSAOBlurParameters->GetDescriptorSets(mCurrentFrameIndex);

                command.buffer.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    mSSAOBlurPipeline->GetPipelineLayout(),
                    0,
                    descriptorSets,
                    nullptr);

                command.buffer.draw(3, 1, 0, 0);

                command.buffer.endRenderPass();
                EndMarker(command.buffer);
            }
            EndMarker(command.buffer);
        }

        // Shade pass
        {
            BeginMarker(command.buffer, "Shade pass");

            {
                std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    {{mShadowMap,
                      vk::ImageLayout::eDepthAttachmentOptimal,
                      vk::ImageLayout::eShaderReadOnlyOptimal},
                     {mVisibilityBuffer,
                      vk::ImageLayout::eColorAttachmentOptimal,
                      vk::ImageLayout::eShaderReadOnlyOptimal}});

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::DependencyFlags{},
                    {},
                    {},
                    imageBarriers);
            }

            const std::vector<vk::ClearValue> clearValues{
                vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f)};
            const vk::RenderPassBeginInfo renderPassBeginInfo =
                vk::RenderPassBeginInfo()
                    .setRenderPass(mShadePass->GetRenderPassHandle())
                    .setFramebuffer(mShadePass->GetFramebufferHandle(
                        mContext->GetSwapchain()->GetCurrentIndex()))
                    .setRenderArea({vk::Offset2D{0, 0}, imageSize})
                    .setClearValues(clearValues);
            command.buffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

            {
                const vk::Viewport viewport{
                    0.0f,
                    0.0f,
                    static_cast<float>(imageSize.width),
                    static_cast<float>(imageSize.height),
                    0.0f,
                    1.0f};
                command.buffer.setViewport(0, viewport);

                const vk::Rect2D scissor = vk::Rect2D().setOffset(0).setExtent(
                    vk::Extent2D{imageSize.width, imageSize.height});
                command.buffer.setScissor(0, scissor);
            }

            command.buffer.bindPipeline(
                vk::PipelineBindPoint::eGraphics,
                mShadePassPipeline->GetPipelineHandle());

            std::vector<vk::DescriptorSet> descriptorSets =
                mShadePassParameters->GetDescriptorSets(mCurrentFrameIndex);

            command.buffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                mShadePassPipeline->GetPipelineLayout(),
                0,
                descriptorSets,
                nullptr);

            command.buffer.draw(3, 1, 0, 0);

            command.buffer.endRenderPass();
            EndMarker(command.buffer);
        }

        // Render transparencies
        {
            BeginMarker(command.buffer, "Transparent pass");

            {
                std::vector<vk::ImageMemoryBarrier> imageBarriers = Texture::GetBarriers(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    {{mDepthBuffer,
                      vk::ImageLayout::eShaderReadOnlyOptimal,
                      vk::ImageLayout::eDepthAttachmentOptimal}});

                command.buffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    vk::DependencyFlags{},
                    {},
                    {},
                    imageBarriers);
            }

            const std::vector<vk::ClearValue> clearValues{
                vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
                vk::ClearDepthStencilValue(1.0f, 0),
            };
            const vk::RenderPassBeginInfo renderPassBeginInfo =
                vk::RenderPassBeginInfo()
                    .setRenderPass(mTransparentPass->GetRenderPassHandle())
                    .setFramebuffer(mTransparentPass->GetFramebufferHandle(
                        mContext->GetSwapchain()->GetCurrentIndex()))
                    .setRenderArea({vk::Offset2D{0, 0}, imageSize})
                    .setClearValues(clearValues);
            command.buffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

            {
                const vk::Viewport viewport{
                    0.0f,
                    0.0f,
                    static_cast<float>(imageSize.width),
                    static_cast<float>(imageSize.height),
                    0.0f,
                    1.0f};
                command.buffer.setViewport(0, viewport);

                const vk::Rect2D scissor = vk::Rect2D().setOffset(0).setExtent(
                    vk::Extent2D{imageSize.width, imageSize.height});
                command.buffer.setScissor(0, scissor);
            }

            const Material::AlphaMode alphaMode = Material::AlphaMode::Blended;
            {
                uint32_t drawCallCount = mScene->GetDrawCallCount(alphaMode);
                if (drawCallCount > 0) {
                    command.buffer.bindPipeline(
                        vk::PipelineBindPoint::eGraphics,
                        mGeometryPassPipeline[alphaMode].pipeline->GetPipelineHandle());

                    std::vector<vk::DescriptorSet> descriptorSets =
                        mGeometryPassPipeline[alphaMode].parameters->GetDescriptorSets(
                            mCurrentFrameIndex);

                    command.buffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics,
                        mGeometryPassPipeline[alphaMode].pipeline->GetPipelineLayout(),
                        0,
                        descriptorSets,
                        nullptr);

                    {
                        mScene->GetMeshSystem()->BindBuffers(
                            command.buffer,
                            VertexAttributeFlag::All);

                        uint32_t maxDrawIndirectCount = mContext->GetDevice()
                                                            ->GetDeviceProperties()
                                                            .limits.maxDrawIndirectCount;
                        VKRT_ASSERT(maxDrawIndirectCount >= drawCallCount);

                        command.buffer.drawIndexedIndirectCount(
                            mBasePassCulling[alphaMode]
                                .indirectDrawBuffers[mCurrentFrameIndex]
                                ->GetBufferHandle(),
                            0,
                            mBasePassCulling[alphaMode]
                                .drawCallCountBuffer[mCurrentFrameIndex]
                                ->GetBufferHandle(),
                            0,
                            drawCallCount,
                            sizeof(VkDrawIndexedIndirectCommand));
                    }
                }
            }
            command.buffer.endRenderPass();
            EndMarker(command.buffer);
        }

        {
            BeginMarker(command.buffer, "Render UI");
            mUIRenderer->Render(command.buffer);
            EndMarker(command.buffer);
        }

        VKRT_ASSERT_VK(command.buffer.end());
    }

    mContext->GetSwapchain()->Present(command.buffer, command.fence, mCurrentFrameIndex);
}

void Renderer::OnKeyPressed(int key) {
    if (key == GLFW_KEY_P) {
        mFreezeCulling = !mFreezeCulling;
    }
}

void Renderer::OnKeyReleased(int key) {}

void Renderer::OnMouseMoved(glm::vec2 newPos) {}

void Renderer::OnLeftMouseButtonPressed() {}

void Renderer::OnLeftMouseButtonReleased() {}

void Renderer::OnRightMouseButtonPressed() {}

void Renderer::OnRightMouseButtonReleased() {}

Renderer::~Renderer() {
    mCommandRing->Flush();
    ScopedRefPtr<InputManager> inputManager = mContext->GetWindow()->GetInputManager();
    inputManager->Unsuscribe(this);
}

}  // namespace VKRT