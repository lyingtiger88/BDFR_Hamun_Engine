#include <Hamun/Core/Log.hpp>
#include <Hamun/Graph/VM.hpp>
#include <Hamun/Platform/Window.hpp>
#include <Hamun/RHI/RHI.hpp>
#include <Hamun/World/StreamingScheduler.hpp>

#include <array>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>

namespace {

bool HasArgument(int argc, char** argv, std::string_view argument)
{
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == argument)
            return true;
    }
    return false;
}

void RunFoundationSelfTests()
{
    Hamun::World::StreamingScheduler streaming;
    Hamun::World::WorldPosition observer{};

    Hamun::World::StreamingRequest request;
    request.id = 1;
    request.resource = "Example/CityCell_2_0";
    request.position.cellX = 2;
    request.velocityX = -60.0;
    streaming.Enqueue(request, observer);

    if (const auto next = streaming.TryPop()) {
        std::cout
            << "Next stream request: "
            << next->resource
            << " score=" << next->score
            << '\n';
    }

    Hamun::Graph::Program program;
    program.constants = {6.0, 7.0};
    program.code = {
        {Hamun::Graph::OpCode::PushConstant, 0},
        {Hamun::Graph::OpCode::PushConstant, 1},
        {Hamun::Graph::OpCode::Multiply, 0},
        {Hamun::Graph::OpCode::Return, 0}
    };

    Hamun::Graph::VM vm;
    const auto value = vm.Execute(program);
    if (const auto* result =
            std::get_if<double>(&value)) {
        std::cout
            << "HamunGraph VM test: 6 * 7 = "
            << *result
            << '\n';
    }
}

#if defined(_WIN32)

struct Vertex {
    float position[3];
    float color[4];
};

bool RunRhiTriangle(
    Hamun::Platform::IWindow& window,
    bool smokeTest)
{
    using namespace Hamun::RHI;

    auto backend =
        CreateBackend(BackendType::D3D12);
    if (!backend)
        return false;

    BackendCreateInfo createInfo;
    createInfo.nativeWindowHandle =
        window.NativeHandle();
    createInfo.width = window.Width();
    createInfo.height = window.Height();
    createInfo.enableValidation = true;

    if (!backend->Initialize(createInfo))
        return false;

    const Vertex vertices[] = {
        {{ 0.0f,  0.60f, 0.0f},
         {0.15f, 0.75f, 1.00f, 1.0f}},
        {{ 0.60f, -0.55f, 0.0f},
         {0.95f, 0.35f, 0.20f, 1.0f}},
        {{-0.60f, -0.55f, 0.0f},
         {0.25f, 0.95f, 0.45f, 1.0f}}
    };

    BufferDesc vertexBufferDesc;
    vertexBufferDesc.size = sizeof(vertices);
    vertexBufferDesc.usage = BufferUsage::Vertex;
    vertexBufferDesc.initialData = vertices;

    auto vertexBuffer =
        backend->CreateBuffer(vertexBufferDesc);
    if (!vertexBuffer)
        return false;

    const std::string shaderSource = R"(
struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
)";

    ShaderDesc vsDesc;
    vsDesc.stage = ShaderStage::Vertex;
    vsDesc.source = shaderSource;
    vsDesc.entryPoint = "VSMain";

    ShaderDesc psDesc;
    psDesc.stage = ShaderStage::Pixel;
    psDesc.source = shaderSource;
    psDesc.entryPoint = "PSMain";

    auto vertexShader =
        backend->CreateShader(vsDesc);
    auto pixelShader =
        backend->CreateShader(psDesc);

    if (!vertexShader || !pixelShader)
        return false;

    GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.vertexShader = vertexShader.get();
    pipelineDesc.pixelShader = pixelShader.get();
    pipelineDesc.vertexStride = sizeof(Vertex);
    pipelineDesc.vertexAttributes = {
        {
            VertexSemantic::Position,
            0,
            VertexFormat::Float3,
            static_cast<std::uint32_t>(
                offsetof(Vertex, position))
        },
        {
            VertexSemantic::Color,
            0,
            VertexFormat::Float4,
            static_cast<std::uint32_t>(
                offsetof(Vertex, color))
        }
    };

    auto pipeline =
        backend->CreateGraphicsPipeline(
            pipelineDesc);
    if (!pipeline)
        return false;

    int renderedFrames = 0;
    while (window.PumpEvents()) {
        ICommandList* commands =
            backend->BeginFrame();
        if (!commands)
            return false;

        commands->BeginRenderPass(
            {0.025f, 0.045f, 0.075f, 1.0f});
        commands->SetPipeline(*pipeline);
        commands->SetVertexBuffer(
            *vertexBuffer,
            sizeof(Vertex));
        commands->Draw(3);
        commands->EndRenderPass();

        if (!backend->SubmitFrame())
            return false;

        ++renderedFrames;
        if (smokeTest &&
            renderedFrames >= 3)
            break;
    }

    backend->Shutdown();
    return true;
}

#endif

} // namespace

int main(int argc, char** argv)
{
    using Hamun::Core::Log;
    using Hamun::Core::LogLevel;

    Log(
        LogLevel::Info,
        "BDFR Hamun Engine sandbox booting");

    RunFoundationSelfTests();

    const bool smokeTest =
        HasArgument(
            argc, argv, "--smoke-test");

#if defined(_WIN32)
    Hamun::Platform::WindowDesc windowDesc;
    windowDesc.title =
        "BDFR Hamun Engine - RHI Triangle";
    windowDesc.width = 1280;
    windowDesc.height = 720;

    auto window =
        Hamun::Platform::CreateNativeWindow(
            windowDesc);
    if (!window)
        return 2;

    if (!RunRhiTriangle(
            *window, smokeTest))
        return 3;
#else
    (void)smokeTest;
    Log(
        LogLevel::Info,
        "Non-Windows CI currently validates the cross-platform RHI interfaces; native Vulkan rendering is the next backend milestone.");
#endif

    return 0;
}
