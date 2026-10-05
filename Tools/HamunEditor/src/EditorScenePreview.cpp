#include "EditorScenePreview.hpp"

#include <Hamun/Assets/ImageAsset.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace Hamun::Editor {
namespace {

struct alignas(256) SceneConstants {
    Renderer::Mat4 model;
    Renderer::Mat4 viewProjection;

    float baseColorFactor[4]{
        1.0f, 1.0f, 1.0f, 1.0f
    };

    float lightDirection[4]{
        0.45f, -1.0f, 0.25f, 0.0f
    };

    float cameraPosition[4]{
        0.0f, 0.0f, -3.0f, 1.0f
    };

    float materialParams[4]{
        0.0f, 1.0f, 0.0f, 0.0f
    };
};

static_assert(sizeof(SceneConstants) == 256);

bool Fail(
    std::string* error,
    std::string message)
{
    if (error)
        *error = std::move(message);

    return false;
}

std::unique_ptr<RHI::ITexture> CreateMeshTexture(
    RHI::IBackend& backend,
    const Assets::MeshAsset& mesh,
    std::string* error)
{
    Assets::ImageAsset image;

    if (!mesh.baseColorTexture.empty()) {
        std::string imageError;

        const auto loaded =
            Assets::LoadImageRGBA8(
                mesh.baseColorTexture,
                &imageError);

        if (!loaded) {
            if (error) {
                *error =
                    "Could not load texture '" +
                    mesh.baseColorTexture.string() +
                    "': " +
                    imageError;
            }

            return {};
        }

        image = *loaded;
    } else {
        image.width = 1;
        image.height = 1;
        image.rgba8 = {
            255, 255, 255, 255
        };
    }

    RHI::TextureDesc desc;
    desc.width = image.width;
    desc.height = image.height;
    desc.format =
        RHI::TextureFormat::RGBA8_UNorm;
    desc.usage =
        RHI::TextureUsage::ShaderResource;
    desc.initialData = image.rgba8.data();
    desc.rowPitch = image.width * 4u;

    return backend.CreateTexture(desc);
}

SceneObjectTransform ExtractTransform(
    const Assets::SceneInstance& instance)
{
    SceneObjectTransform result;

    result.position = {
        instance.worldMatrix[12],
        instance.worldMatrix[13],
        instance.worldMatrix[14]
    };

    const auto rowLength =
        [&](std::size_t offset) {
            const float x =
                instance.worldMatrix[offset + 0];

            const float y =
                instance.worldMatrix[offset + 1];

            const float z =
                instance.worldMatrix[offset + 2];

            return std::sqrt(
                x * x +
                y * y +
                z * z);
        };

    result.scale = {
        rowLength(0),
        rowLength(4),
        rowLength(8)
    };

    return result;
}

void ApplyScaleToBasis(
    std::array<float, 16>& matrix,
    std::size_t offset,
    float scale)
{
    const float x =
        matrix[offset + 0];

    const float y =
        matrix[offset + 1];

    const float z =
        matrix[offset + 2];

    const float length =
        std::sqrt(
            x * x +
            y * y +
            z * z);

    if (length > 0.000001f) {
        const float factor =
            scale /
            length;

        matrix[offset + 0] *=
            factor;

        matrix[offset + 1] *=
            factor;

        matrix[offset + 2] *=
            factor;

        return;
    }

    matrix[offset + 0] = 0.0f;
    matrix[offset + 1] = 0.0f;
    matrix[offset + 2] = 0.0f;

    if (offset == 0)
        matrix[0] = scale;
    else if (offset == 4)
        matrix[5] = scale;
    else
        matrix[10] = scale;
}

const char* SceneShaderSource()
{
    return R"(
cbuffer SceneConstants : register(b0)
{
    row_major float4x4 model;
    row_major float4x4 viewProjection;
    float4 baseColorFactor;
    float4 lightDirection;
    float4 cameraPosition;
    float4 materialParams;
};

Texture2D BaseColor : register(t0);
SamplerState BaseSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normalWS : NORMAL0;
    float3 worldPosition : TEXCOORD1;
    float2 uv : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;

    const float4 worldPosition =
        mul(float4(input.position, 1.0f), model);

    output.position =
        mul(worldPosition, viewProjection);

    output.worldPosition =
        worldPosition.xyz;

    output.normalWS =
        normalize(
            mul(
                float4(input.normal, 0.0f),
                model).xyz);

    output.uv = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    const float4 sampled =
        BaseColor.Sample(
            BaseSampler,
            input.uv);

    const float3 albedo =
        saturate(
            sampled.rgb *
            baseColorFactor.rgb);

    const float3 normal =
        normalize(input.normalWS);

    const float3 lightDir =
        normalize(-lightDirection.xyz);

    const float3 viewDir =
        normalize(
            cameraPosition.xyz -
            input.worldPosition);

    const float3 halfDir =
        normalize(lightDir + viewDir);

    const float diffuse =
        saturate(
            dot(normal, lightDir));

    const float roughness =
        clamp(
            materialParams.y,
            0.05f,
            1.0f);

    const float metallic =
        saturate(materialParams.x);

    const float specularPower =
        lerp(
            96.0f,
            8.0f,
            roughness);

    const float specular =
        pow(
            saturate(
                dot(normal, halfDir)),
            specularPower);

    const float3 specularColor =
        lerp(
            float3(0.04f, 0.04f, 0.04f),
            albedo,
            metallic);

    float3 color =
        albedo * 0.12f +
        albedo * diffuse * 1.15f +
        specularColor * specular * 0.55f;

    color =
        color /
        (1.0f + color);

    color =
        pow(
            saturate(color),
            1.0f / 2.2f);

    return float4(
        color,
        sampled.a *
        baseColorFactor.a);
}
)";
}

} // namespace

bool ScenePreview::Initialize(
    RHI::IBackend& backend,
    const std::filesystem::path& scenePath,
    std::uint32_t width,
    std::uint32_t height,
    std::string* error)
{
    Reset();

    width_ = std::max(width, 1u);
    height_ = std::max(height, 1u);
    scenePath_ = scenePath;

    std::string assetError;

    asset_ =
        Assets::LoadGltf(
            scenePath,
            &assetError);

    if (!asset_ ||
        asset_->meshes.empty() ||
        asset_->instances.empty()) {
        Reset();

        return Fail(
            error,
            "Could not load editor preview scene '" +
                scenePath.string() +
                "': " +
                assetError);
    }

    meshes_.reserve(
        asset_->meshes.size());

    for (const Assets::MeshAsset& mesh :
         asset_->meshes) {
        RHI::BufferDesc vertexDesc;
        vertexDesc.size =
            mesh.vertices.size() *
            sizeof(Assets::MeshVertex);
        vertexDesc.usage =
            RHI::BufferUsage::Vertex;
        vertexDesc.initialData =
            mesh.vertices.data();

        RHI::BufferDesc indexDesc;
        indexDesc.size =
            mesh.indices.size() *
            sizeof(std::uint32_t);
        indexDesc.usage =
            RHI::BufferUsage::Index;
        indexDesc.initialData =
            mesh.indices.data();

        RenderMesh renderMesh;

        renderMesh.vertexBuffer =
            backend.CreateBuffer(
                vertexDesc);

        renderMesh.indexBuffer =
            backend.CreateBuffer(
                indexDesc);

        renderMesh.texture =
            CreateMeshTexture(
                backend,
                mesh,
                error);

        renderMesh.baseColorFactor =
            mesh.baseColorFactor;

        renderMesh.metallic =
            mesh.metallicFactor;

        renderMesh.roughness =
            mesh.roughnessFactor;

        renderMesh.indexCount =
            static_cast<std::uint32_t>(
                mesh.indices.size());

        if (!renderMesh.vertexBuffer ||
            !renderMesh.indexBuffer ||
            !renderMesh.texture ||
            renderMesh.indexCount == 0) {
            Reset();

            return Fail(
                error,
                "Could not create GPU mesh resources for the editor scene.");
        }

        meshes_.push_back(
            std::move(renderMesh));
    }

    RHI::SamplerDesc samplerDesc;
    samplerDesc.filter =
        RHI::SamplerFilter::Linear;
    samplerDesc.addressU =
        RHI::SamplerAddressMode::Repeat;
    samplerDesc.addressV =
        RHI::SamplerAddressMode::Repeat;

    sampler_ =
        backend.CreateSampler(
            samplerDesc);

    if (!sampler_) {
        Reset();

        return Fail(
            error,
            "Could not create editor scene sampler.");
    }

    RHI::ShaderDesc vsDesc;
    vsDesc.stage =
        RHI::ShaderStage::Vertex;
    vsDesc.source =
        SceneShaderSource();
    vsDesc.entryPoint =
        "VSMain";

    RHI::ShaderDesc psDesc;
    psDesc.stage =
        RHI::ShaderStage::Pixel;
    psDesc.source =
        SceneShaderSource();
    psDesc.entryPoint =
        "PSMain";

    vertexShader_ =
        backend.CreateShader(vsDesc);

    pixelShader_ =
        backend.CreateShader(psDesc);

    if (!vertexShader_ ||
        !pixelShader_) {
        Reset();

        return Fail(
            error,
            "Could not compile editor scene shaders.");
    }

    RHI::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.vertexShader =
        vertexShader_.get();
    pipelineDesc.pixelShader =
        pixelShader_.get();
    pipelineDesc.vertexStride =
        sizeof(Assets::MeshVertex);
    pipelineDesc.constantBufferCount = 1;
    pipelineDesc.textureCount = 1;
    pipelineDesc.samplerCount = 1;
    pipelineDesc.renderTargetFormat =
        RHI::TextureFormat::RGBA8_UNorm;
    pipelineDesc.depthFormat =
        RHI::TextureFormat::R32_Float;
    pipelineDesc.depthTest = true;

    pipelineDesc.vertexAttributes = {
        {
            RHI::VertexSemantic::Position,
            0,
            RHI::VertexFormat::Float3,
            static_cast<std::uint32_t>(
                offsetof(
                    Assets::MeshVertex,
                    position))
        },
        {
            RHI::VertexSemantic::Normal,
            0,
            RHI::VertexFormat::Float3,
            static_cast<std::uint32_t>(
                offsetof(
                    Assets::MeshVertex,
                    normal))
        },
        {
            RHI::VertexSemantic::TexCoord,
            0,
            RHI::VertexFormat::Float2,
            static_cast<std::uint32_t>(
                offsetof(
                    Assets::MeshVertex,
                    uv))
        }
    };

    pipeline_ =
        backend.CreateGraphicsPipeline(
            pipelineDesc);

    if (!pipeline_) {
        Reset();

        return Fail(
            error,
            "Could not create editor scene graphics pipeline.");
    }

    Renderer::FrameResourcesDesc frameDesc;
    frameDesc.frameCount = 2;
    frameDesc.constantBufferCount =
        asset_->instances.size();
    frameDesc.constantBufferSize =
        sizeof(SceneConstants);

    if (!frameResources_.Initialize(
            backend,
            frameDesc)) {
        Reset();

        return Fail(
            error,
            "Could not create editor scene frame resources.");
    }

    draws_.reserve(
        asset_->instances.size());

    for (const Assets::SceneInstance& instance :
         asset_->instances) {
        if (instance.meshIndex >=
            meshes_.size()) {
            Reset();

            return Fail(
                error,
                "Editor scene contains an invalid mesh instance.");
        }

        const RenderMesh& mesh =
            meshes_[instance.meshIndex];

        Renderer::IndexedDraw draw;
        draw.vertexBuffer =
            mesh.vertexBuffer.get();
        draw.indexBuffer =
            mesh.indexBuffer.get();
        draw.texture =
            mesh.texture.get();
        draw.vertexStride =
            sizeof(Assets::MeshVertex);
        draw.indexCount =
            mesh.indexCount;
        draw.indexType =
            RHI::IndexType::UInt32;

        draws_.push_back(draw);
    }

    return true;
}

void ScenePreview::Reset() noexcept
{
    draws_.clear();
    frameResources_.Reset();
    pipeline_.reset();
    pixelShader_.reset();
    vertexShader_.reset();
    sampler_.reset();
    meshes_.clear();
    asset_.reset();
    scenePath_.clear();

    camera_ =
        Renderer::FreeCamera{};

    width_ = 1;
    height_ = 1;
}

bool ScenePreview::RenderFrame(
    RHI::IBackend& backend)
{
    if (!Ready())
        return false;

    RHI::ISwapChain* swapChain =
        backend.SwapChain();

    if (!swapChain ||
        !frameResources_.SelectFrame(
            swapChain->FrameIndex())) {
        return false;
    }

    width_ =
        std::max(
            swapChain->Width(),
            1u);

    height_ =
        std::max(
            swapChain->Height(),
            1u);

    const float aspect =
        static_cast<float>(width_) /
        static_cast<float>(height_);

    const Renderer::Mat4 viewProjection =
        camera_.ViewProjection(
            aspect);

    for (std::size_t instanceIndex = 0;
         instanceIndex <
            asset_->instances.size();
         ++instanceIndex) {
        const Assets::SceneInstance& instance =
            asset_->instances[instanceIndex];

        if (instance.meshIndex >=
            meshes_.size()) {
            return false;
        }

        const RenderMesh& mesh =
            meshes_[instance.meshIndex];

        SceneConstants constants;

        for (std::size_t i = 0;
             i < 16;
             ++i) {
            constants.model.m[i] =
                instance.worldMatrix[i];
        }

        constants.viewProjection =
            viewProjection;

        for (std::size_t i = 0;
             i < 4;
             ++i) {
            constants.baseColorFactor[i] =
                mesh.baseColorFactor[i];
        }

        constants.materialParams[0] =
            mesh.metallic;

        constants.materialParams[1] =
            mesh.roughness;

        const Renderer::Vec3& cameraPosition =
            camera_.Position();

        constants.cameraPosition[0] =
            cameraPosition.x;

        constants.cameraPosition[1] =
            cameraPosition.y;

        constants.cameraPosition[2] =
            cameraPosition.z;

        if (!frameResources_.UpdateConstantBuffer(
                instanceIndex,
                &constants,
                sizeof(constants))) {
            return false;
        }

        draws_[instanceIndex]
            .constantBuffer =
                frameResources_.ConstantBuffer(
                    instanceIndex);

        if (!draws_[instanceIndex]
                .constantBuffer) {
            return false;
        }
    }

    Renderer::RenderFrameSubmission
        submission;

    submission.scenePipeline =
        pipeline_.get();

    submission.sceneSampler =
        sampler_.get();

    submission.draws =
        draws_;

    submission.clearColor = {
        0.020f,
        0.027f,
        0.040f,
        1.0f
    };

    return renderer_.RenderFrame(
        backend,
        submission);
}

bool ScenePreview::Ready() const noexcept
{
    return
        asset_.has_value() &&
        !draws_.empty() &&
        pipeline_ &&
        sampler_;
}

std::size_t ScenePreview::InstanceCount() const noexcept
{
    return asset_
        ? asset_->instances.size()
        : 0;
}

std::optional<SceneObjectInfo>
ScenePreview::ObjectInfo(
    std::size_t index) const
{
    if (!asset_ ||
        index >=
            asset_->instances.size()) {
        return std::nullopt;
    }

    const Assets::SceneInstance& instance =
        asset_->instances[index];

    if (instance.meshIndex >=
        meshes_.size()) {
        return std::nullopt;
    }

    const RenderMesh& mesh =
        meshes_[instance.meshIndex];

    SceneObjectInfo result;
    result.index = index;

    result.hierarchyDepth =
        instance.hierarchyDepth;

    result.name =
        instance.name.empty()
            ? "SceneObject_" +
                std::to_string(index)
            : instance.name;

    if (instance.meshIndex <
        asset_->meshes.size()) {
        const std::string& meshName =
            asset_->meshes[
                instance.meshIndex]
                .name;

        result.meshName =
            meshName.empty()
                ? "Mesh_" +
                    std::to_string(
                        instance.meshIndex)
                : meshName;
    }

    result.transform =
        ExtractTransform(instance);

    result.baseColorFactor =
        mesh.baseColorFactor;

    result.metallic =
        mesh.metallic;

    result.roughness =
        mesh.roughness;

    return result;
}

bool ScenePreview::SetTransform(
    std::size_t index,
    const SceneObjectTransform& transform)
{
    if (!asset_ ||
        index >=
            asset_->instances.size()) {
        return false;
    }

    Assets::SceneInstance& instance =
        asset_->instances[index];

    instance.worldMatrix[12] =
        transform.position[0];

    instance.worldMatrix[13] =
        transform.position[1];

    instance.worldMatrix[14] =
        transform.position[2];

    ApplyScaleToBasis(
        instance.worldMatrix,
        0,
        std::max(
            transform.scale[0],
            0.0001f));

    ApplyScaleToBasis(
        instance.worldMatrix,
        4,
        std::max(
            transform.scale[1],
            0.0001f));

    ApplyScaleToBasis(
        instance.worldMatrix,
        8,
        std::max(
            transform.scale[2],
            0.0001f));

    return true;
}

const std::filesystem::path&
ScenePreview::ScenePath() const noexcept
{
    return scenePath_;
}

} // namespace Hamun::Editor
