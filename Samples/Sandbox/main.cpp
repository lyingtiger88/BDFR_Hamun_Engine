#include <Hamun/Assets/GltfAsset.hpp>
#include <Hamun/Assets/ImageAsset.hpp>
#include <Hamun/Core/Log.hpp>
#include <Hamun/Graph/VM.hpp>
#include <Hamun/Platform/Window.hpp>
#include <Hamun/Renderer/ComputeDispatch.hpp>
#include <Hamun/Renderer/FrameResources.hpp>
#include <Hamun/Renderer/FreeCamera.hpp>
#include <Hamun/Renderer/Material.hpp>
#include <Hamun/Renderer/Renderer.hpp>
#include <Hamun/RHI/RHI.hpp>
#include <Hamun/World/StreamingScheduler.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool HasArgument(
    int argc,
    char** argv,
    std::string_view argument)
{
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) ==
            argument) {
            return true;
        }
    }

    return false;
}

void RunFoundationSelfTests()
{
    Hamun::World::StreamingScheduler streaming;
    Hamun::World::WorldPosition observer{};

    Hamun::World::StreamingRequest request;
    request.id = 1;
    request.resource =
        "Example/CityCell_2_0";
    request.position.cellX = 2;
    request.velocityX = -60.0;

    streaming.Enqueue(
        request,
        observer);

    if (const auto next =
            streaming.TryPop()) {
        std::cout
            << "Next stream request: "
            << next->resource
            << " score="
            << next->score
            << '\n';
    }

    Hamun::Graph::Program program;
    program.constants = {
        6.0,
        7.0
    };

    program.code = {
        {
            Hamun::Graph::OpCode::PushConstant,
            0
        },
        {
            Hamun::Graph::OpCode::PushConstant,
            1
        },
        {
            Hamun::Graph::OpCode::Multiply,
            0
        },
        {
            Hamun::Graph::OpCode::Return,
            0
        }
    };

    Hamun::Graph::VM vm;
    const auto value =
        vm.Execute(program);

    if (const auto* result =
            std::get_if<double>(&value)) {
        std::cout
            << "HamunGraph VM test: "
            << "6 * 7 = "
            << *result
            << '\n';
    }

    Hamun::Renderer::Renderer renderer;
    renderer.BeginFrame();
    renderer.Render();
    renderer.EndFrame();

    std::cout
        << "RenderGraph bootstrap: passes="
        << renderer.RenderPassCount()
        << '\n';
}

#if defined(_WIN32)

struct alignas(256) SceneConstants {
    Hamun::Renderer::Mat4 model;
    Hamun::Renderer::Mat4 viewProjection;

    float baseColorFactor[4]{
        1.0f,
        1.0f,
        1.0f,
        1.0f
    };

    float lightDirection[4]{
        0.45f,
        -1.0f,
        0.25f,
        0.0f
    };

    float lightColor[4]{
        1.0f,
        0.95f,
        0.86f,
        1.0f
    };

    float lightingParams[4]{
        0.08f,
        1.35f,
        0.0f,
        0.0f
    };

    float materialParams[4]{
        0.0f,
        1.0f,
        0.0f,
        0.0f
    };

    float cameraPosition[4]{
        0.0f,
        0.0f,
        0.0f,
        1.0f
    };
};

struct RenderMesh {
    std::unique_ptr<Hamun::RHI::IBuffer>
        vertexBuffer;

    std::unique_ptr<Hamun::RHI::IBuffer>
        indexBuffer;

    std::unique_ptr<Hamun::RHI::ITexture>
        texture;

    std::shared_ptr<Hamun::Renderer::Material>
        baseMaterial;

    std::shared_ptr<Hamun::Renderer::MaterialInstance>
        material;

    std::uint32_t indexCount = 0;
};

std::filesystem::path DefaultScenePath()
{
    return
        Hamun::Platform::ExecutableDirectory() /
        "Assets" /
        "TestScene.gltf";
}

std::unique_ptr<Hamun::RHI::ITexture>
CreateMeshTexture(
    Hamun::RHI::IBackend& backend,
    const Hamun::Assets::MeshAsset& mesh)
{
    using namespace Hamun::RHI;

    Hamun::Assets::ImageAsset image;
    std::string imageError;

    if (!mesh.baseColorTexture.empty()) {
        const auto loadedImage =
            Hamun::Assets::LoadImageRGBA8(
                mesh.baseColorTexture,
                &imageError);

        if (!loadedImage) {
            std::cerr
                << "Failed to load texture: "
                << mesh.baseColorTexture
                << " - "
                << imageError
                << '\n';

            return {};
        }

        image = *loadedImage;
    } else {
        image.width = 1;
        image.height = 1;
        image.rgba8 = {
            255,
            255,
            255,
            255
        };
    }

    TextureDesc textureDesc;
    textureDesc.width =
        image.width;
    textureDesc.height =
        image.height;
    textureDesc.format =
        TextureFormat::RGBA8_UNorm;
    textureDesc.initialData =
        image.rgba8.data();
    textureDesc.rowPitch =
        image.width * 4u;

    return
        backend.CreateTexture(
            textureDesc);
}

bool BuildRenderMeshes(
    Hamun::RHI::IBackend& backend,
    const Hamun::Assets::GltfAsset& asset,
    std::vector<RenderMesh>& renderMeshes)
{
    using namespace Hamun::RHI;

    renderMeshes.clear();
    renderMeshes.reserve(
        asset.meshes.size());

    for (const Hamun::Assets::MeshAsset& mesh :
         asset.meshes) {
        BufferDesc vertexDesc;
        vertexDesc.size =
            mesh.vertices.size() *
            sizeof(
                Hamun::Assets::MeshVertex);
        vertexDesc.usage =
            BufferUsage::Vertex;
        vertexDesc.initialData =
            mesh.vertices.data();

        BufferDesc indexDesc;
        indexDesc.size =
            mesh.indices.size() *
            sizeof(std::uint32_t);
        indexDesc.usage =
            BufferUsage::Index;
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
                mesh);

        renderMesh.indexCount =
            static_cast<std::uint32_t>(
                mesh.indices.size());

        Hamun::Renderer::MaterialDesc materialDesc;
        materialDesc.name =
            mesh.name.empty()
                ? "GltfMaterial"
                : mesh.name + "_Material";
        materialDesc.parameters.baseColorFactor =
            mesh.baseColorFactor;
        materialDesc.parameters.metallic =
            mesh.metallicFactor;
        materialDesc.parameters.roughness =
            mesh.roughnessFactor;
        materialDesc.depthTest = true;

        renderMesh.baseMaterial =
            std::make_shared<Hamun::Renderer::Material>(
                std::move(materialDesc));

        renderMesh.material =
            std::make_shared<Hamun::Renderer::MaterialInstance>(
                renderMesh.baseMaterial);

        if (!renderMesh.vertexBuffer ||
            !renderMesh.indexBuffer ||
            !renderMesh.texture ||
            !renderMesh.material) {
            return false;
        }

        renderMeshes.push_back(
            std::move(renderMesh));
    }

    return true;
}

bool RunAssetScene(
    Hamun::Platform::IWindow& window,
    bool smokeTest,
    Hamun::RHI::BackendType backendType)
{
    using namespace Hamun::RHI;

    std::string assetError;

    const std::filesystem::path scenePath =
        DefaultScenePath();

    const auto asset =
        Hamun::Assets::LoadGltf(
            scenePath,
            &assetError);

    if (!asset ||
        asset->meshes.empty() ||
        asset->instances.empty()) {
        std::cerr
            << "Failed to load glTF scene: "
            << scenePath
            << " - "
            << assetError
            << '\n';
        return false;
    }

    std::cout
        << "Loaded glTF scene: "
        << scenePath.filename().string()
        << " | meshes="
        << asset->meshes.size()
        << " instances="
        << asset->instances.size()
        << '\n';

    for (const auto& instance :
         asset->instances) {
        std::cout
            << "  instance: "
            << instance.name
            << " -> mesh "
            << instance.meshIndex
            << '\n';
    }

    auto backend =
        CreateBackend(backendType);

    if (!backend)
        return false;

    BackendCreateInfo createInfo;
    createInfo.nativeWindowHandle =
        window.NativeHandle();
    createInfo.width =
        window.Width();
    createInfo.height =
        window.Height();
    createInfo.enableValidation =
        true;

    if (!backend->Initialize(
            createInfo))
        return false;

    std::cout
        << BuildCapabilityReport(
            *backend);

    std::vector<RenderMesh>
        renderMeshes;

    if (!BuildRenderMeshes(
            *backend,
            *asset,
            renderMeshes)) {
        return false;
    }

    SamplerDesc samplerDesc;
    samplerDesc.filter =
        SamplerFilter::Linear;
    samplerDesc.addressU =
        SamplerAddressMode::Repeat;
    samplerDesc.addressV =
        SamplerAddressMode::Repeat;

    auto sampler =
        backend->CreateSampler(
            samplerDesc);

    if (!sampler)
        return false;

    Hamun::Renderer::FrameResources
        frameResources;

    Hamun::Renderer::FrameResourcesDesc
        frameResourcesDesc;

    frameResourcesDesc.frameCount = 2;
    frameResourcesDesc.constantBufferCount =
        asset->instances.size();
    frameResourcesDesc.constantBufferSize =
        sizeof(SceneConstants);

    if (!frameResources.Initialize(
            *backend,
            frameResourcesDesc)) {
        return false;
    }

    const std::string shaderSource = R"(
cbuffer SceneConstants : register(b0)
{
    row_major float4x4 model;
    row_major float4x4 viewProjection;
    float4 baseColorFactor;
    float4 lightDirection;
    float4 lightColor;
    float4 lightingParams;
    float4 materialParams;
    float4 cameraPosition;
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

static const float PI =
    3.14159265359f;

float DistributionGGX(
    float3 normal,
    float3 halfway,
    float roughness)
{
    const float alpha =
        max(
            roughness * roughness,
            0.0025f);

    const float alpha2 =
        alpha * alpha;

    const float nDotH =
        saturate(
            dot(
                normal,
                halfway));

    const float nDotH2 =
        nDotH * nDotH;

    const float denominatorTerm =
        nDotH2 *
        (alpha2 - 1.0f) +
        1.0f;

    return
        alpha2 /
        max(
            PI *
            denominatorTerm *
            denominatorTerm,
            0.0001f);
}

float GeometrySchlickGGX(
    float nDotV,
    float roughness)
{
    const float r =
        roughness + 1.0f;

    const float k =
        (r * r) /
        8.0f;

    return
        nDotV /
        max(
            nDotV *
            (1.0f - k) +
            k,
            0.0001f);
}

float GeometrySmith(
    float3 normal,
    float3 viewDirection,
    float3 lightDirectionValue,
    float roughness)
{
    const float nDotV =
        saturate(
            dot(
                normal,
                viewDirection));

    const float nDotL =
        saturate(
            dot(
                normal,
                lightDirectionValue));

    return
        GeometrySchlickGGX(
            nDotV,
            roughness) *
        GeometrySchlickGGX(
            nDotL,
            roughness);
}

float3 FresnelSchlick(
    float cosTheta,
    float3 f0)
{
    return
        f0 +
        (1.0f - f0) *
        pow(
            1.0f -
            saturate(cosTheta),
            5.0f);
}

PSInput VSMain(VSInput input)
{
    PSInput output;

    const float4 worldPosition =
        mul(
            float4(
                input.position,
                1.0f),
            model);

    output.position =
        mul(
            worldPosition,
            viewProjection);

    output.worldPosition =
        worldPosition.xyz;

    output.normalWS =
        normalize(
            mul(
                float4(
                    input.normal,
                    0.0f),
                model).xyz);

    output.uv =
        input.uv;

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

    const float metallic =
        saturate(
            materialParams.x);

    const float roughness =
        clamp(
            materialParams.y,
            0.045f,
            1.0f);

    const float3 normal =
        normalize(
            input.normalWS);

    const float3 viewDirection =
        normalize(
            cameraPosition.xyz -
            input.worldPosition);

    const float3 towardLight =
        normalize(
            -lightDirection.xyz);

    const float3 halfway =
        normalize(
            viewDirection +
            towardLight);

    const float nDotL =
        saturate(
            dot(
                normal,
                towardLight));

    const float nDotV =
        saturate(
            dot(
                normal,
                viewDirection));

    const float3 f0 =
        lerp(
            float3(
                0.04f,
                0.04f,
                0.04f),
            albedo,
            metallic);

    const float distribution =
        DistributionGGX(
            normal,
            halfway,
            roughness);

    const float geometry =
        GeometrySmith(
            normal,
            viewDirection,
            towardLight,
            roughness);

    const float3 fresnel =
        FresnelSchlick(
            saturate(
                dot(
                    halfway,
                    viewDirection)),
            f0);

    const float3 numerator =
        distribution *
        geometry *
        fresnel;

    const float denominator =
        max(
            4.0f *
            nDotV *
            nDotL,
            0.001f);

    const float3 specular =
        numerator /
        denominator;

    const float3 kS =
        fresnel;

    const float3 kD =
        (1.0f - kS) *
        (1.0f - metallic);

    const float3 radiance =
        lightColor.rgb *
        lightingParams.y;

    const float3 direct =
        (
            kD *
            albedo /
            PI +
            specular
        ) *
        radiance *
        nDotL;

    const float3 ambient =
        albedo *
        lightingParams.x;

    return float4(
        ambient + direct,
        sampled.a *
        baseColorFactor.a);
}
)";

    ShaderDesc vsDesc;
    vsDesc.stage =
        ShaderStage::Vertex;
    vsDesc.source =
        shaderSource;
    vsDesc.entryPoint =
        "VSMain";

    ShaderDesc psDesc;
    psDesc.stage =
        ShaderStage::Pixel;
    psDesc.source =
        shaderSource;
    psDesc.entryPoint =
        "PSMain";

    auto vertexShader =
        backend->CreateShader(
            vsDesc);

    auto pixelShader =
        backend->CreateShader(
            psDesc);

    if (!vertexShader ||
        !pixelShader)
        return false;

    GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.vertexShader =
        vertexShader.get();
    pipelineDesc.pixelShader =
        pixelShader.get();
    pipelineDesc.vertexStride =
        sizeof(
            Hamun::Assets::MeshVertex);
    pipelineDesc.constantBufferCount =
        1;
    pipelineDesc.textureCount =
        1;
    pipelineDesc.samplerCount =
        1;
    pipelineDesc.depthTest =
        true;

    pipelineDesc.vertexAttributes = {
        {
            VertexSemantic::Position,
            0,
            VertexFormat::Float3,
            static_cast<std::uint32_t>(
                offsetof(
                    Hamun::Assets::MeshVertex,
                    position))
        },
        {
            VertexSemantic::Normal,
            0,
            VertexFormat::Float3,
            static_cast<std::uint32_t>(
                offsetof(
                    Hamun::Assets::MeshVertex,
                    normal))
        },
        {
            VertexSemantic::TexCoord,
            0,
            VertexFormat::Float2,
            static_cast<std::uint32_t>(
                offsetof(
                    Hamun::Assets::MeshVertex,
                    uv))
        }
    };

    auto pipeline =
        backend->CreateGraphicsPipeline(
            pipelineDesc);

    if (!pipeline)
        return false;

    std::unique_ptr<IShader>
        computeShader;

    std::unique_ptr<IPipeline>
        computePipeline;

    std::vector<Hamun::Renderer::ComputeDispatch>
        computeDispatches;

    if (backend->Caps().compute) {
        const std::string computeSource = R"(
[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
}
)";

        ShaderDesc computeShaderDesc;
        computeShaderDesc.stage =
            ShaderStage::Compute;
        computeShaderDesc.source =
            computeSource;
        computeShaderDesc.entryPoint =
            "CSMain";

        computeShader =
            backend->CreateShader(
                computeShaderDesc);

        if (!computeShader)
            return false;

        ComputePipelineDesc
            computePipelineDesc;

        computePipelineDesc.computeShader =
            computeShader.get();

        computePipeline =
            backend->CreateComputePipeline(
                computePipelineDesc);

        if (!computePipeline)
            return false;

        Hamun::Renderer::ComputeDispatch
            dispatch;

        dispatch.pipeline =
            computePipeline.get();

        dispatch.groupCountX = 1;
        dispatch.groupCountY = 1;
        dispatch.groupCountZ = 1;

        computeDispatches.push_back(
            dispatch);

        std::cout
            << "Compute smoke dispatch: enabled\n";
    }

    Hamun::Renderer::Renderer sceneRenderer;

    std::vector<Hamun::Renderer::IndexedDraw>
        sceneDraws;

    sceneDraws.reserve(
        asset->instances.size());

    for (std::size_t instanceIndex = 0;
         instanceIndex < asset->instances.size();
         ++instanceIndex) {
        const auto& instance =
            asset->instances[
                instanceIndex];

        if (instance.meshIndex >=
            renderMeshes.size()) {
            return false;
        }

        const RenderMesh& renderMesh =
            renderMeshes[
                instance.meshIndex];

        Hamun::Renderer::IndexedDraw draw;
        draw.vertexBuffer =
            renderMesh.vertexBuffer.get();
        draw.indexBuffer =
            renderMesh.indexBuffer.get();
        draw.constantBuffer =
            nullptr;
        draw.texture =
            renderMesh.texture.get();
        draw.vertexStride =
            sizeof(
                Hamun::Assets::MeshVertex);
        draw.indexCount =
            renderMesh.indexCount;
        draw.indexType =
            IndexType::UInt32;

        sceneDraws.push_back(
            draw);
    }

    Hamun::Renderer::FreeCamera camera;

    std::cout
        << "Controls: WASD move, Q/E down/up, "
        << "hold RMB + mouse to look, Shift sprint, Esc exit.\n";

    auto previousTime =
        std::chrono::steady_clock::now();

    auto fpsStart =
        previousTime;

    int renderedFrames = 0;
    int fpsFrames = 0;

    while (window.PumpEvents()) {
        if (window.IsKeyDown(
                Hamun::Platform::Key::Escape)) {
            break;
        }

        const auto now =
            std::chrono::steady_clock::now();

        float deltaSeconds =
            std::chrono::duration<float>(
                now - previousTime).count();

        previousTime = now;

        if (deltaSeconds > 0.1f)
            deltaSeconds = 0.1f;

        camera.Update(
            window,
            deltaSeconds);

        const float aspect =
            static_cast<float>(
                window.Width()) /
            static_cast<float>(
                window.Height());

        const Hamun::Renderer::Mat4
            viewProjection =
                camera.ViewProjection(
                    aspect);

        const ISwapChain* swapChain =
            backend->SwapChain();

        if (!swapChain ||
            !frameResources.SelectFrame(
                swapChain->FrameIndex())) {
            return false;
        }

        for (std::size_t instanceIndex = 0;
             instanceIndex <
                asset->instances.size();
             ++instanceIndex) {
            const auto& instance =
                asset->instances[
                    instanceIndex];

            if (instance.meshIndex >=
                renderMeshes.size()) {
                return false;
            }

            const RenderMesh& renderMesh =
                renderMeshes[
                    instance.meshIndex];

            SceneConstants constants;

            for (std::size_t matrixIndex = 0;
                 matrixIndex < 16;
                 ++matrixIndex) {
                constants.model.m[
                    matrixIndex] =
                    instance.worldMatrix[
                        matrixIndex];
            }

            constants.viewProjection =
                viewProjection;

            for (std::size_t colorIndex = 0;
                 colorIndex < 4;
                 ++colorIndex) {
                constants.baseColorFactor[
                    colorIndex] =
                    renderMesh
                        .material
                        ->Parameters()
                        .baseColorFactor[
                            colorIndex];
            }

            constants.materialParams[0] =
                renderMesh
                    .material
                    ->Parameters()
                    .metallic;

            constants.materialParams[1] =
                renderMesh
                    .material
                    ->Parameters()
                    .roughness;

            constants.cameraPosition[0] =
                camera.Position().x;

            constants.cameraPosition[1] =
                camera.Position().y;

            constants.cameraPosition[2] =
                camera.Position().z;

            if (!frameResources.UpdateConstantBuffer(
                    instanceIndex,
                    &constants,
                    sizeof(constants),
                    0)) {
                return false;
            }

            sceneDraws[
                instanceIndex]
                .constantBuffer =
                    frameResources.ConstantBuffer(
                        instanceIndex);

            if (!sceneDraws[
                    instanceIndex]
                    .constantBuffer) {
                return false;
            }
        }

        if (!sceneRenderer.RenderFrame(
                *backend,
                *pipeline,
                *sampler,
                sceneDraws,
                computeDispatches)) {
            return false;
        }

        ++renderedFrames;
        ++fpsFrames;

        const float fpsWindow =
            std::chrono::duration<float>(
                now - fpsStart).count();

        if (!smokeTest &&
            fpsWindow >= 1.0f) {
            std::cout
                << "FPS: "
                << static_cast<int>(
                    fpsFrames /
                    fpsWindow)
                << " | Camera: "
                << camera.Position().x
                << ", "
                << camera.Position().y
                << ", "
                << camera.Position().z
                << " | Draws: "
                << asset->instances.size()
                << '\n';

            fpsFrames = 0;
            fpsStart = now;
        }

        if (smokeTest &&
            renderedFrames >= 3)
            break;
    }

    backend->Shutdown();
    return true;
}

#endif

} // namespace

int main(
    int argc,
    char** argv)
{
    using Hamun::Core::Log;
    using Hamun::Core::LogLevel;

    Log(
        LogLevel::Info,
        "BDFR Hamun Engine sandbox booting");

    RunFoundationSelfTests();

    const bool smokeTest =
        HasArgument(
            argc,
            argv,
            "--smoke-test");

#if defined(_WIN32)
    const bool useD3D11 =
        HasArgument(
            argc,
            argv,
            "--d3d11");

    const Hamun::RHI::BackendType backendType =
        useD3D11
            ? Hamun::RHI::BackendType::D3D11
            : Hamun::RHI::BackendType::D3D12;

    Hamun::Platform::WindowDesc windowDesc;

    windowDesc.title =
        useD3D11
            ? "BDFR Hamun Engine - Test v0.3 (DX11)"
            : "BDFR Hamun Engine - Test v0.3 (DX12)";

    windowDesc.width = 1280;
    windowDesc.height = 720;

    auto window =
        Hamun::Platform::CreateNativeWindow(
            windowDesc);

    if (!window)
        return 2;

    if (!RunAssetScene(
            *window,
            smokeTest,
            backendType)) {
        return 3;
    }
#else
    (void)smokeTest;

    Log(
        LogLevel::Info,
        "Non-Windows CI validates the cross-platform engine modules; native Vulkan rendering follows.");
#endif

    return 0;
}
