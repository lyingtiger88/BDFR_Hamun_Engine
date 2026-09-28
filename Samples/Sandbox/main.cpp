#include <Hamun/Assets/GltfAsset.hpp>
#include <Hamun/Assets/ImageAsset.hpp>
#include <Hamun/Core/Log.hpp>
#include <Hamun/Graph/VM.hpp>
#include <Hamun/Platform/Window.hpp>
#include <Hamun/Renderer/FreeCamera.hpp>
#include <Hamun/RHI/RHI.hpp>
#include <Hamun/World/StreamingScheduler.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
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
}

#if defined(_WIN32)

struct alignas(256) SceneConstants {
    Hamun::Renderer::Mat4 mvp;
};

std::filesystem::path DefaultScenePath()
{
    return
        Hamun::Platform::ExecutableDirectory() /
        "Assets" /
        "TestScene.gltf";
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
        asset->meshes.empty()) {
        std::cerr
            << "Failed to load glTF: "
            << scenePath
            << " - "
            << assetError
            << '\n';
        return false;
    }

    const Hamun::Assets::MeshAsset& mesh =
        asset->meshes.front();

    if (mesh.vertices.empty() ||
        mesh.indices.empty()) {
        std::cerr
            << "Loaded glTF mesh is empty.\n";
        return false;
    }

    Hamun::Assets::ImageAsset image;

    if (!mesh.baseColorTexture.empty()) {
        const auto loadedImage =
            Hamun::Assets::LoadImageRGBA8(
                mesh.baseColorTexture,
                &assetError);

        if (!loadedImage) {
            std::cerr
                << "Failed to load texture: "
                << mesh.baseColorTexture
                << " - "
                << assetError
                << '\n';
            return false;
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

    std::cout
        << "Loaded glTF mesh: "
        << mesh.name
        << " | vertices="
        << mesh.vertices.size()
        << " indices="
        << mesh.indices.size()
        << " texture="
        << image.width
        << "x"
        << image.height
        << '\n';

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

    BufferDesc constantDesc;
    constantDesc.size =
        sizeof(SceneConstants);
    constantDesc.usage =
        BufferUsage::Constant;

    auto vertexBuffer =
        backend->CreateBuffer(
            vertexDesc);
    auto indexBuffer =
        backend->CreateBuffer(
            indexDesc);
    auto constantBuffer =
        backend->CreateBuffer(
            constantDesc);

    if (!vertexBuffer ||
        !indexBuffer ||
        !constantBuffer)
        return false;

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

    auto texture =
        backend->CreateTexture(
            textureDesc);

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

    if (!texture || !sampler)
        return false;

    const std::string shaderSource = R"(
cbuffer SceneConstants : register(b0)
{
    row_major float4x4 mvp;
};

Texture2D BaseColor : register(t0);
SamplerState BaseSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position =
        mul(
            float4(
                input.position,
                1.0f),
            mvp);
    output.uv = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return BaseColor.Sample(
        BaseSampler,
        input.uv);
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
                Hamun::Platform::Key::Escape))
            break;

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

        SceneConstants constants{};
        constants.mvp =
            camera.ViewProjection(
                aspect);

        if (!constantBuffer->Update(
                &constants,
                sizeof(constants),
                0))
            return false;

        ICommandList* commands =
            backend->BeginFrame();

        if (!commands)
            return false;

        commands->BeginRenderPass(
            {
                0.012f,
                0.022f,
                0.040f,
                1.0f
            });

        commands->SetPipeline(
            *pipeline);

        commands->SetConstantBuffer(
            0,
            *constantBuffer);

        commands->SetTexture(
            0,
            *texture);

        commands->SetSampler(
            0,
            *sampler);

        commands->SetVertexBuffer(
            *vertexBuffer,
            sizeof(
                Hamun::Assets::MeshVertex));

        commands->SetIndexBuffer(
            *indexBuffer,
            IndexType::UInt32);

        commands->DrawIndexed(
            static_cast<std::uint32_t>(
                mesh.indices.size()));

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
            ? "BDFR Hamun Engine - glTF Scene (DX11)"
            : "BDFR Hamun Engine - glTF Scene (DX12)";

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
            backendType))
        return 3;
#else
    (void)smokeTest;

    Log(
        LogLevel::Info,
        "Non-Windows CI validates the cross-platform engine modules; native Vulkan rendering follows.");
#endif

    return 0;
}
