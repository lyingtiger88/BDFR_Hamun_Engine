#include <Hamun/Assets/GltfAsset.hpp>
#include <Hamun/Assets/ImageAsset.hpp>
#include <Hamun/Core/Log.hpp>
#include <Hamun/Graph/VM.hpp>
#include <Hamun/Platform/Window.hpp>
#include <Hamun/Renderer/FreeCamera.hpp>
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
        0.24f,
        0.95f,
        0.0f,
        0.0f
    };
};

struct RenderMesh {
    std::unique_ptr<Hamun::RHI::IBuffer>
        vertexBuffer;

    std::unique_ptr<Hamun::RHI::IBuffer>
        indexBuffer;

    std::unique_ptr<Hamun::RHI::ITexture>
        texture;

    std::uint32_t indexCount = 0;

    std::array<float, 4> baseColorFactor{
        1.0f,
        1.0f,
        1.0f,
        1.0f
    };
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

        renderMesh.baseColorFactor =
            mesh.baseColorFactor;

        if (!renderMesh.vertexBuffer ||
            !renderMesh.indexBuffer ||
            !renderMesh.texture) {
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

    std::vector<
        std::unique_ptr<IBuffer>>
        constantBuffers;

    constantBuffers.reserve(
        asset->instances.size());

    for (std::size_t i = 0;
         i < asset->instances.size();
         ++i) {
        BufferDesc constantDesc;
        constantDesc.size =
            sizeof(SceneConstants);
        constantDesc.usage =
            BufferUsage::Constant;

        auto constantBuffer =
            backend->CreateBuffer(
                constantDesc);

        if (!constantBuffer)
            return false;

        constantBuffers.push_back(
            std::move(
                constantBuffer));
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
    float2 uv : TEXCOORD0;
};

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
    const float3 normal =
        normalize(
            input.normalWS);

    const float3 towardLight =
        normalize(
            -lightDirection.xyz);

    const float diffuse =
        saturate(
            dot(
                normal,
                towardLight));

    const float illumination =
        lightingParams.x +
        diffuse *
        lightingParams.y;

    const float3 lighting =
        lightColor.rgb *
        illumination;

    const float4 sampled =
        BaseColor.Sample(
            BaseSampler,
            input.uv);

    return
        sampled *
        baseColorFactor *
        float4(
            lighting,
            1.0f);
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
                        .baseColorFactor[
                            colorIndex];
            }

            if (!constantBuffers[
                    instanceIndex]
                    ->Update(
                        &constants,
                        sizeof(constants),
                        0)) {
                return false;
            }
        }

        ICommandList* commands =
            backend->BeginFrame();

        if (!commands)
            return false;

        commands->BeginRenderPass(
            {
                0.018f,
                0.035f,
                0.060f,
                1.0f
            });

        commands->SetPipeline(
            *pipeline);

        commands->SetSampler(
            0,
            *sampler);

        for (std::size_t instanceIndex = 0;
             instanceIndex <
                asset->instances.size();
             ++instanceIndex) {
            const auto& instance =
                asset->instances[
                    instanceIndex];

            const RenderMesh& renderMesh =
                renderMeshes[
                    instance.meshIndex];

            commands->SetConstantBuffer(
                0,
                *constantBuffers[
                    instanceIndex]);

            commands->SetTexture(
                0,
                *renderMesh.texture);

            commands->SetVertexBuffer(
                *renderMesh.vertexBuffer,
                sizeof(
                    Hamun::Assets::MeshVertex));

            commands->SetIndexBuffer(
                *renderMesh.indexBuffer,
                IndexType::UInt32);

            commands->DrawIndexed(
                renderMesh.indexCount);
        }

        commands->EndRenderPass();

        if (!backend->SubmitFrame())
            return false;

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
            ? "BDFR Hamun Engine - Scene v0.2 (DX11)"
            : "BDFR Hamun Engine - Scene v0.2 (DX12)";

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
