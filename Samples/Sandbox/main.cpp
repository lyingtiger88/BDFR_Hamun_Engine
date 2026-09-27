#include <Hamun/Core/Log.hpp>
#include <Hamun/Graph/VM.hpp>
#include <Hamun/Platform/Window.hpp>
#include <Hamun/RHI/RHI.hpp>
#include <Hamun/World/StreamingScheduler.hpp>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
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
    if (const auto* result = std::get_if<double>(&value)) {
        std::cout
            << "HamunGraph VM test: 6 * 7 = "
            << *result
            << '\n';
    }
}

#if defined(_WIN32)

struct Mat4 {
    float m[16]{};
};

Mat4 Identity()
{
    Mat4 result{};
    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;
    return result;
}

Mat4 Multiply(const Mat4& a, const Mat4& b)
{
    Mat4 result{};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum +=
                    a.m[row * 4 + k] *
                    b.m[k * 4 + column];
            }
            result.m[row * 4 + column] = sum;
        }
    }
    return result;
}

Mat4 RotationY(float radians)
{
    Mat4 result = Identity();
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    result.m[0] = c;
    result.m[2] = -s;
    result.m[8] = s;
    result.m[10] = c;
    return result;
}

Mat4 RotationX(float radians)
{
    Mat4 result = Identity();
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    result.m[5] = c;
    result.m[6] = s;
    result.m[9] = -s;
    result.m[10] = c;
    return result;
}

Mat4 Translation(float x, float y, float z)
{
    Mat4 result = Identity();
    result.m[12] = x;
    result.m[13] = y;
    result.m[14] = z;
    return result;
}

Mat4 PerspectiveFovLH(
    float fovY,
    float aspect,
    float nearZ,
    float farZ)
{
    Mat4 result{};
    const float yScale = 1.0f / std::tan(fovY * 0.5f);
    const float xScale = yScale / aspect;
    const float zScale = farZ / (farZ - nearZ);

    result.m[0] = xScale;
    result.m[5] = yScale;
    result.m[10] = zScale;
    result.m[11] = 1.0f;
    result.m[14] = -nearZ * zScale;
    return result;
}

struct Vertex {
    float position[3];
    float color[4];
};

struct alignas(256) SceneConstants {
    Mat4 mvp;
};

bool RunRhiCube(
    Hamun::Platform::IWindow& window,
    bool smokeTest)
{
    using namespace Hamun::RHI;

    auto backend = CreateBackend(BackendType::D3D12);
    if (!backend)
        return false;

    BackendCreateInfo createInfo;
    createInfo.nativeWindowHandle = window.NativeHandle();
    createInfo.width = window.Width();
    createInfo.height = window.Height();
    createInfo.enableValidation = true;

    if (!backend->Initialize(createInfo))
        return false;

    const Vertex vertices[] = {
        {{-0.6f, -0.6f, -0.6f}, {1.0f, 0.2f, 0.2f, 1.0f}},
        {{-0.6f,  0.6f, -0.6f}, {0.2f, 1.0f, 0.2f, 1.0f}},
        {{ 0.6f,  0.6f, -0.6f}, {0.2f, 0.4f, 1.0f, 1.0f}},
        {{ 0.6f, -0.6f, -0.6f}, {1.0f, 0.8f, 0.2f, 1.0f}},
        {{-0.6f, -0.6f,  0.6f}, {0.9f, 0.2f, 1.0f, 1.0f}},
        {{-0.6f,  0.6f,  0.6f}, {0.2f, 1.0f, 1.0f, 1.0f}},
        {{ 0.6f,  0.6f,  0.6f}, {1.0f, 0.5f, 0.2f, 1.0f}},
        {{ 0.6f, -0.6f,  0.6f}, {0.8f, 0.8f, 0.9f, 1.0f}}
    };

    const std::uint16_t indices[] = {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        4, 5, 1, 4, 1, 0,
        3, 2, 6, 3, 6, 7,
        1, 5, 6, 1, 6, 2,
        4, 0, 3, 4, 3, 7
    };

    BufferDesc vertexDesc;
    vertexDesc.size = sizeof(vertices);
    vertexDesc.usage = BufferUsage::Vertex;
    vertexDesc.initialData = vertices;

    BufferDesc indexDesc;
    indexDesc.size = sizeof(indices);
    indexDesc.usage = BufferUsage::Index;
    indexDesc.initialData = indices;

    BufferDesc constantDesc;
    constantDesc.size = sizeof(SceneConstants);
    constantDesc.usage = BufferUsage::Constant;

    auto vertexBuffer = backend->CreateBuffer(vertexDesc);
    auto indexBuffer = backend->CreateBuffer(indexDesc);
    auto constantBuffer = backend->CreateBuffer(constantDesc);

    if (!vertexBuffer || !indexBuffer || !constantBuffer)
        return false;

    const std::string shaderSource = R"(
cbuffer SceneConstants : register(b0)
{
    row_major float4x4 mvp;
};

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
    output.position = mul(float4(input.position, 1.0f), mvp);
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

    auto vertexShader = backend->CreateShader(vsDesc);
    auto pixelShader = backend->CreateShader(psDesc);

    if (!vertexShader || !pixelShader)
        return false;

    GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.vertexShader = vertexShader.get();
    pipelineDesc.pixelShader = pixelShader.get();
    pipelineDesc.vertexStride = sizeof(Vertex);
    pipelineDesc.constantBufferCount = 1;
    pipelineDesc.depthTest = true;
    pipelineDesc.vertexAttributes = {
        {
            VertexSemantic::Position,
            0,
            VertexFormat::Float3,
            static_cast<std::uint32_t>(offsetof(Vertex, position))
        },
        {
            VertexSemantic::Color,
            0,
            VertexFormat::Float4,
            static_cast<std::uint32_t>(offsetof(Vertex, color))
        }
    };

    auto pipeline =
        backend->CreateGraphicsPipeline(pipelineDesc);
    if (!pipeline)
        return false;

    const float aspect =
        static_cast<float>(window.Width()) /
        static_cast<float>(window.Height());

    constexpr float pi = 3.14159265358979323846f;
    const Mat4 projection =
        PerspectiveFovLH(pi / 3.0f, aspect, 0.1f, 100.0f);
    const Mat4 view = Translation(0.0f, 0.0f, 3.0f);

    const auto start =
        std::chrono::steady_clock::now();

    int renderedFrames = 0;
    while (window.PumpEvents()) {
        const auto now =
            std::chrono::steady_clock::now();
        const float seconds =
            std::chrono::duration<float>(now - start).count();

        const Mat4 model =
            Multiply(
                RotationY(seconds * 0.9f),
                RotationX(seconds * 0.45f));

        SceneConstants constants{};
        constants.mvp =
            Multiply(
                Multiply(model, view),
                projection);

        if (!constantBuffer->Update(
                &constants,
                sizeof(constants),
                0))
            return false;

        ICommandList* commands = backend->BeginFrame();
        if (!commands)
            return false;

        commands->BeginRenderPass(
            {0.018f, 0.032f, 0.055f, 1.0f});
        commands->SetPipeline(*pipeline);
        commands->SetConstantBuffer(0, *constantBuffer);
        commands->SetVertexBuffer(
            *vertexBuffer,
            sizeof(Vertex));
        commands->SetIndexBuffer(
            *indexBuffer,
            IndexType::UInt16);
        commands->DrawIndexed(
            static_cast<std::uint32_t>(
                sizeof(indices) / sizeof(indices[0])));
        commands->EndRenderPass();

        if (!backend->SubmitFrame())
            return false;

        ++renderedFrames;
        if (smokeTest && renderedFrames >= 3)
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
        HasArgument(argc, argv, "--smoke-test");

#if defined(_WIN32)
    Hamun::Platform::WindowDesc windowDesc;
    windowDesc.title =
        "BDFR Hamun Engine - 3D RHI Cube";
    windowDesc.width = 1280;
    windowDesc.height = 720;

    auto window =
        Hamun::Platform::CreateNativeWindow(windowDesc);
    if (!window)
        return 2;

    if (!RunRhiCube(*window, smokeTest))
        return 3;
#else
    (void)smokeTest;
    Log(
        LogLevel::Info,
        "Non-Windows CI validates the cross-platform RHI interface; native Vulkan rendering follows.");
#endif

    return 0;
}
