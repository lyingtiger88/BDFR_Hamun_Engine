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

constexpr float Pi =
    3.14159265358979323846f;

float ToDegrees(
    float radians)
{
    return
        radians *
        (180.0f / Pi);
}

float ToRadians(
    float degrees)
{
    return
        degrees *
        (Pi / 180.0f);
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

    const float sx =
        std::max(
            result.scale[0],
            0.000001f);

    const float sy =
        std::max(
            result.scale[1],
            0.000001f);

    const float sz =
        std::max(
            result.scale[2],
            0.000001f);

    const float rr00 =
        instance.worldMatrix[0] / sx;
    const float rr01 =
        instance.worldMatrix[1] / sx;
    const float rr02 =
        instance.worldMatrix[2] / sx;

    const float rr10 =
        instance.worldMatrix[4] / sy;
    const float rr11 =
        instance.worldMatrix[5] / sy;
    const float rr12 =
        instance.worldMatrix[6] / sy;

    const float rr20 =
        instance.worldMatrix[8] / sz;
    const float rr21 =
        instance.worldMatrix[9] / sz;
    const float rr22 =
        instance.worldMatrix[10] / sz;

    // The stored matrix follows Hamun's row-vector convention.
    // Transpose it to a conventional column-vector rotation matrix
    // before extracting XYZ Euler angles (Rz * Ry * Rx).
    const float c00 = rr00;
    const float c01 = rr10;
    const float c02 = rr20;
    const float c10 = rr01;
    const float c11 = rr11;
    const float c12 = rr21;
    const float c20 = rr02;
    const float c21 = rr12;
    const float c22 = rr22;

    const float y =
        std::asin(
            std::clamp(
                -c20,
                -1.0f,
                1.0f));

    const float cosY =
        std::cos(y);

    float x = 0.0f;
    float z = 0.0f;

    if (std::abs(cosY) >
        0.00001f) {
        x =
            std::atan2(
                c21,
                c22);

        z =
            std::atan2(
                c10,
                c00);
    } else {
        x =
            std::atan2(
                -c12,
                c11);

        z = 0.0f;
    }

    result.rotationDegrees = {
        ToDegrees(x),
        ToDegrees(y),
        ToDegrees(z)
    };

    return result;
}

void BuildTransformMatrix(
    std::array<float, 16>& matrix,
    const SceneObjectTransform& transform)
{
    const float x =
        ToRadians(
            transform.rotationDegrees[0]);

    const float y =
        ToRadians(
            transform.rotationDegrees[1]);

    const float z =
        ToRadians(
            transform.rotationDegrees[2]);

    const float cx = std::cos(x);
    const float sx = std::sin(x);
    const float cy = std::cos(y);
    const float sy = std::sin(y);
    const float cz = std::cos(z);
    const float sz = std::sin(z);

    // Conventional column-vector rotation matrix Rz * Ry * Rx.
    const float c00 =
        cz * cy;
    const float c01 =
        cz * sy * sx -
        sz * cx;
    const float c02 =
        cz * sy * cx +
        sz * sx;

    const float c10 =
        sz * cy;
    const float c11 =
        sz * sy * sx +
        cz * cx;
    const float c12 =
        sz * sy * cx -
        cz * sx;

    const float c20 =
        -sy;
    const float c21 =
        cy * sx;
    const float c22 =
        cy * cx;

    const float scaleX =
        std::max(
            transform.scale[0],
            0.0001f);

    const float scaleY =
        std::max(
            transform.scale[1],
            0.0001f);

    const float scaleZ =
        std::max(
            transform.scale[2],
            0.0001f);

    // Transpose to Hamun's row-vector convention and apply scale.
    matrix = {
        c00 * scaleX,
        c10 * scaleX,
        c20 * scaleX,
        0.0f,

        c01 * scaleY,
        c11 * scaleY,
        c21 * scaleY,
        0.0f,

        c02 * scaleZ,
        c12 * scaleZ,
        c22 * scaleZ,
        0.0f,

        transform.position[0],
        transform.position[1],
        transform.position[2],
        1.0f
    };
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

    objectMaterials_.clear();
    objectMaterials_.reserve(
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

        SceneMaterialState material;
        material.baseColorFactor =
            mesh.baseColorFactor;
        material.metallic =
            mesh.metallic;
        material.roughness =
            mesh.roughness;

        objectMaterials_.push_back(
            material);
    }

    if (!RebuildSceneRuntime(
            backend,
            error)) {
        Reset();
        return false;
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
    entityIds_.clear();
    objectMaterials_.clear();
    world_.Clear();
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

        if (instanceIndex >=
            objectMaterials_.size()) {
            return false;
        }

        const SceneMaterialState& material =
            objectMaterials_[
                instanceIndex];

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
                material.baseColorFactor[i];
        }

        constants.materialParams[0] =
            material.metallic;

        constants.materialParams[1] =
            material.roughness;

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

    if (index <
        entityIds_.size()) {
        result.entityId =
            entityIds_[index];

        const World::EntityRecord* entity =
            world_.Find(
                result.entityId);

        if (entity) {
            result.parentEntityId =
                entity->parent;
        }
    }

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

    result.meshIndex =
        instance.meshIndex;

    result.transform =
        ExtractTransform(instance);

    if (index <
        objectMaterials_.size()) {
        result.material =
            objectMaterials_[index];
    } else {
        result.material.baseColorFactor =
            mesh.baseColorFactor;
        result.material.metallic =
            mesh.metallic;
        result.material.roughness =
            mesh.roughness;
    }

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

    BuildTransformMatrix(
        instance.worldMatrix,
        transform);

    if (index <
        entityIds_.size()) {
        World::EntityRecord* entity =
            world_.Find(
                entityIds_[index]);

        if (entity) {
            entity->transform
                .position
                .local = {
                    transform.position[0],
                    transform.position[1],
                    transform.position[2]
                };

            entity->transform.rotationDegrees =
                transform.rotationDegrees;

            entity->transform.scale =
                transform.scale;
        }
    }

    return true;
}

bool ScenePreview::SetMaterial(
    std::size_t index,
    const SceneMaterialState& material)
{
    if (!asset_ ||
        index >=
            objectMaterials_.size()) {
        return false;
    }

    SceneMaterialState sanitized =
        material;

    for (float& component :
         sanitized.baseColorFactor) {
        component =
            std::clamp(
                component,
                0.0f,
                1.0f);
    }

    sanitized.metallic =
        std::clamp(
            sanitized.metallic,
            0.0f,
            1.0f);

    sanitized.roughness =
        std::clamp(
            sanitized.roughness,
            0.045f,
            1.0f);

    objectMaterials_[index] =
        sanitized;

    return true;
}

std::vector<SceneObjectState>
ScenePreview::CaptureObjects() const
{
    std::vector<SceneObjectState>
        objects;

    if (!asset_)
        return objects;

    objects.reserve(
        asset_->instances.size());

    for (std::size_t index = 0;
         index <
            asset_->instances.size();
         ++index) {
        const Assets::SceneInstance& instance =
            asset_->instances[index];

        SceneObjectState state;
        state.name =
            instance.name;
        state.meshIndex =
            instance.meshIndex;
        state.hierarchyDepth =
            instance.hierarchyDepth;
        state.transform =
            ExtractTransform(
                instance);

        if (index <
            objectMaterials_.size()) {
            state.material =
                objectMaterials_[index];
        } else if (
            instance.meshIndex <
                meshes_.size()) {
            const RenderMesh& mesh =
                meshes_[
                    instance.meshIndex];

            state.material.baseColorFactor =
                mesh.baseColorFactor;
            state.material.metallic =
                mesh.metallic;
            state.material.roughness =
                mesh.roughness;
        }

        objects.push_back(
            std::move(state));
    }

    return objects;
}

bool ScenePreview::ReplaceObjects(
    RHI::IBackend& backend,
    const std::vector<SceneObjectState>& objects,
    std::string* error)
{
    if (!asset_ ||
        objects.empty()) {
        return Fail(
            error,
            "Scene object list is empty.");
    }

    std::vector<Assets::SceneInstance>
        instances;

    std::vector<SceneMaterialState>
        materials;

    instances.reserve(
        objects.size());

    materials.reserve(
        objects.size());

    for (const SceneObjectState& object :
         objects) {
        if (object.meshIndex >=
            meshes_.size()) {
            return Fail(
                error,
                "Scene object references an invalid mesh index.");
        }

        Assets::SceneInstance instance;
        instance.name =
            object.name.empty()
                ? "SceneObject"
                : object.name;

        instance.meshIndex =
            object.meshIndex;

        instance.hierarchyDepth =
            object.hierarchyDepth;

        BuildTransformMatrix(
            instance.worldMatrix,
            object.transform);

        instances.push_back(
            std::move(instance));

        SceneMaterialState material =
            object.material;

        for (float& component :
             material.baseColorFactor) {
            component =
                std::clamp(
                    component,
                    0.0f,
                    1.0f);
        }

        material.metallic =
            std::clamp(
                material.metallic,
                0.0f,
                1.0f);

        material.roughness =
            std::clamp(
                material.roughness,
                0.045f,
                1.0f);

        materials.push_back(
            material);
    }

    asset_->instances =
        std::move(instances);

    objectMaterials_ =
        std::move(materials);

    return RebuildSceneRuntime(
        backend,
        error);
}

bool ScenePreview::DuplicateObject(
    RHI::IBackend& backend,
    std::size_t index,
    std::size_t* duplicatedIndex,
    std::string* error)
{
    if (!asset_ ||
        index >=
            asset_->instances.size() ||
        index >=
            objectMaterials_.size()) {
        return Fail(
            error,
            "Selected scene object cannot be duplicated.");
    }

    Assets::SceneInstance duplicate =
        asset_->instances[index];

    duplicate.name =
        duplicate.name.empty()
            ? "SceneObject_Copy"
            : duplicate.name +
                "_Copy";

    SceneObjectTransform transform =
        ExtractTransform(
            duplicate);

    transform.position[0] +=
        0.5f;

    BuildTransformMatrix(
        duplicate.worldMatrix,
        transform);

    asset_->instances.push_back(
        std::move(duplicate));

    objectMaterials_.push_back(
        objectMaterials_[index]);

    if (!RebuildSceneRuntime(
            backend,
            error)) {
        asset_->instances.pop_back();
        objectMaterials_.pop_back();

        RebuildSceneRuntime(
            backend,
            nullptr);

        return false;
    }

    if (duplicatedIndex) {
        *duplicatedIndex =
            asset_->instances.size() -
            1;
    }

    return true;
}

bool ScenePreview::DeleteObject(
    RHI::IBackend& backend,
    std::size_t index,
    std::string* error)
{
    if (!asset_ ||
        index >=
            asset_->instances.size()) {
        return Fail(
            error,
            "Selected scene object cannot be deleted.");
    }

    if (asset_->instances.size() <=
        1) {
        return Fail(
            error,
            "The last scene object cannot be deleted in this editor build.");
    }

    const std::uint32_t depth =
        asset_->instances[index]
            .hierarchyDepth;

    std::size_t end =
        index + 1;

    while (end <
           asset_->instances.size() &&
           asset_->instances[end]
                   .hierarchyDepth >
               depth) {
        ++end;
    }

    auto oldInstances =
        asset_->instances;

    auto oldMaterials =
        objectMaterials_;

    asset_->instances.erase(
        asset_->instances.begin() +
            static_cast<std::ptrdiff_t>(
                index),
        asset_->instances.begin() +
            static_cast<std::ptrdiff_t>(
                end));

    if (index <
        objectMaterials_.size()) {
        const std::size_t materialEnd =
            std::min(
                end,
                objectMaterials_.size());

        objectMaterials_.erase(
            objectMaterials_.begin() +
                static_cast<std::ptrdiff_t>(
                    index),
            objectMaterials_.begin() +
                static_cast<std::ptrdiff_t>(
                    materialEnd));
    }

    if (!RebuildSceneRuntime(
            backend,
            error)) {
        asset_->instances =
            std::move(oldInstances);

        objectMaterials_ =
            std::move(oldMaterials);

        RebuildSceneRuntime(
            backend,
            nullptr);

        return false;
    }

    return true;
}

bool ScenePreview::RebuildSceneRuntime(
    RHI::IBackend& backend,
    std::string* error)
{
    if (!asset_ ||
        asset_->instances.empty()) {
        return Fail(
            error,
            "Scene contains no renderable objects.");
    }

    if (objectMaterials_.size() !=
        asset_->instances.size()) {
        objectMaterials_.clear();
        objectMaterials_.reserve(
            asset_->instances.size());

        for (const Assets::SceneInstance& instance :
             asset_->instances) {
            if (instance.meshIndex >=
                meshes_.size()) {
                return Fail(
                    error,
                    "Scene object references an invalid mesh.");
            }

            const RenderMesh& mesh =
                meshes_[
                    instance.meshIndex];

            SceneMaterialState material;
            material.baseColorFactor =
                mesh.baseColorFactor;
            material.metallic =
                mesh.metallic;
            material.roughness =
                mesh.roughness;

            objectMaterials_.push_back(
                material);
        }
    }

    frameResources_.Reset();
    draws_.clear();
    entityIds_.clear();
    world_.Clear();

    Renderer::FrameResourcesDesc
        frameDesc;

    frameDesc.frameCount = 2;

    frameDesc.constantBufferCount =
        asset_->instances.size();

    frameDesc.constantBufferSize =
        sizeof(SceneConstants);

    if (!frameResources_.Initialize(
            backend,
            frameDesc)) {
        return Fail(
            error,
            "Could not rebuild scene frame resources.");
    }

    draws_.reserve(
        asset_->instances.size());

    entityIds_.reserve(
        asset_->instances.size());

    std::vector<World::EntityId>
        hierarchyParents;

    for (const Assets::SceneInstance& instance :
         asset_->instances) {
        if (instance.meshIndex >=
            meshes_.size()) {
            return Fail(
                error,
                "Scene object references an invalid mesh.");
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

        World::EntityId parentEntity =
            World::InvalidEntity;

        if (instance.hierarchyDepth > 0 &&
            instance.hierarchyDepth - 1 <
                hierarchyParents.size()) {
            parentEntity =
                hierarchyParents[
                    instance.hierarchyDepth -
                    1];
        }

        const World::EntityId entityId =
            world_.CreateEntity(
                instance.name.empty()
                    ? "SceneObject"
                    : instance.name,
                parentEntity);

        if (entityId ==
            World::InvalidEntity) {
            return Fail(
                error,
                "Could not create HamunWorld entity.");
        }

        World::EntityRecord* entity =
            world_.Find(entityId);

        if (!entity) {
            return Fail(
                error,
                "Could not resolve HamunWorld entity.");
        }

        const SceneObjectTransform transform =
            ExtractTransform(
                instance);

        entity->transform.position.local = {
            transform.position[0],
            transform.position[1],
            transform.position[2]
        };

        entity->transform.rotationDegrees =
            transform.rotationDegrees;

        entity->transform.scale =
            transform.scale;

        entity->mesh =
            World::MeshComponent{
                scenePath_,
                instance.meshIndex
            };

        entityIds_.push_back(
            entityId);

        if (hierarchyParents.size() <=
            instance.hierarchyDepth) {
            hierarchyParents.resize(
                instance.hierarchyDepth + 1,
                World::InvalidEntity);
        }

        hierarchyParents[
            instance.hierarchyDepth] =
                entityId;

        hierarchyParents.resize(
            instance.hierarchyDepth + 1);
    }

    return true;
}

void ScenePreview::MoveCamera(
    float forward,
    float right,
    float up) noexcept
{
    camera_.MoveLocal(
        forward,
        right,
        up);
}

void ScenePreview::RotateCamera(
    float yawDelta,
    float pitchDelta) noexcept
{
    camera_.Rotate(
        yawDelta,
        pitchDelta);
}

Renderer::Vec3
ScenePreview::CameraPosition() const noexcept
{
    return camera_.Position();
}

float ScenePreview::CameraYaw() const noexcept
{
    return camera_.Yaw();
}

float ScenePreview::CameraPitch() const noexcept
{
    return camera_.Pitch();
}

void ScenePreview::SetCameraPose(
    const Renderer::Vec3& position,
    float yaw,
    float pitch) noexcept
{
    camera_.SetPosition(
        position);

    camera_.SetOrientation(
        yaw,
        pitch);
}

const std::filesystem::path&
ScenePreview::ScenePath() const noexcept
{
    return scenePath_;
}

} // namespace Hamun::Editor
