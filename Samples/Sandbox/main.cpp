#include <Hamun/Core/Log.hpp>
#include <Hamun/Graph/VM.hpp>
#include <Hamun/Platform/Window.hpp>
#include <Hamun/RHI/RHI.hpp>
#include <Hamun/World/StreamingScheduler.hpp>

#include <array>
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
    if (const auto* result =
            std::get_if<double>(&value)) {
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
    const float yScale =
        1.0f / std::tan(fovY * 0.5f);
    const float xScale =
        yScale / aspect;
    const float zScale =
        farZ / (farZ - nearZ);

    result.m[0] = xScale;
    result.m[5] = yScale;
    result.m[10] = zScale;
    result.m[11] = 1.0f;
    result.m[14] = -nearZ * zScale;
    return result;
}

struct Vertex {
    float position[3];
    float uv[2];
};

struct alignas(256) SceneConstants {
    Mat4 mvp;
};

std::array<std::uint8_t, 16 * 16 * 4>
BuildCheckerboardTexture()
{
    std::array<std::uint8_t, 16 * 16 * 4> pixels{};

    for (std::uint32_t y = 0; y < 16; ++y) {
        for (std::uint32_t x = 0; x < 16; ++x) {
            const bool alternate =
                ((x / 4) + (y / 4)) % 2 != 0;

            const std::size_t index =
                (static_cast<std::size_t>(y) * 16 + x) * 4;

            if (alternate) {
                pixels[index + 0] = 30;
                pixels[index + 1] = 185;
                pixels[index + 2] = 235;
                pixels[index + 3] = 255;
            } else {
                pixels[index + 0] = 240;
                pixels[index + 1] = 105;
                pixels[index + 2] = 45;
                pixels[index + 3] = 255;
            }
        }
    }

    return pixels;
}

bool RunTexturedMesh(
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
        {{-0.6f,-0.6f,-0.6f},{0.0f,1.0f}},
        {{-0.6f, 0.6f,-0.6f},{0.0f,0.0f}},
        {{ 0.6f, 0.6f,-0.6f},{1.0f,0.0f}},
        {{ 0.6f,-0.6f,-0.6f},{1.0f,1.0f}},

        {{ 0.6f,-0.6f, 0.6f},{0.0f,1.0f}},
        {{ 0.6f, 0.6f, 0.6f},{0.0f,0.0f}},
        {{-0.6f, 0.6f, 0.6f},{1.0f,0.0f}},
        {{-0.6f,-0.6f, 0.6f},{1.0f,1.0f}},

        {{-0.6f,-0.6f, 0.6f},{0.0f,1.0f}},
        {{-0.6f, 0.6f, 0.6f},{0.0f,0.0f}},
        {{-0.6f, 0.6f,-0.6f},{1.0f,0.0f}},
        {{-0.6f,-0.6f,-0.6f},{1.0f,1.0f}},

        {{ 0.6f,-0.6f,-0.6f},{0.0f,1.0f}},
        {{ 0.6f, 0.6f,-0.6f},{0.0f,0.0f}},
        {{ 0.6f, 0.6f, 0.6f},{1.0f,0.0f}},
        {{ 0.6f,-0.6f, 0.6f},{1.0f,1.0f}},

        {{-0.6f, 0.6f,-0.6f},{0.0f,1.0f}},
        {{-0.6f, 0.6f, 0.6f},{0.0f,0.0f}},
        {{ 0.6f, 0.6f, 0.6f},{1.0f,0.0f}},
        {{ 0.6f, 0.6f,-0.6f},{1.0f,1.0f}},

        {{-0.6f,-0.6f, 0.6f},{0.0f,1.0f}},
        {{-0.6f,-0.6f,-0.6f},{0.0f,0.0f}},
        {{ 0.6f,-0.6f,-0.6f},{1.0f,0.0f}},
        {{ 0.6f,-0.6f, 0.6f},{1.0f,1.0f}}
    };

    const std::uint16_t indices[] = {
         0, 1, 2,  0, 2, 3,
         4, 5, 6,  4, 6, 7,
         8, 9,10,  8,10,11,
        12,13,14, 12,14,15,
        16,17,18, 16,18,19,
        20,21,22, 20,22,23
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

    auto vertexBuffer =
        backend->CreateBuffer(vertexDesc);
    auto indexBuffer =
        backend->CreateBuffer(indexDesc);
    auto constantBuffer =
        backend->CreateBuffer(constantDesc);

    if (!vertexBuffer ||
        !indexBuffer ||
        !constantBuffer)
        return false;

    const auto pixels =
        BuildCheckerboardTexture();

    TextureDesc textureDesc;
    textureDesc.width = 16;
    textureDesc.height = 16;
    textureDesc.format =
        TextureFormat::RGBA8_UNorm;
    textureDesc.initialData = pixels.data();
    textureDesc.rowPitch = 16 * 4;

    auto texture =
        backend->CreateTexture(textureDesc);

    SamplerDesc samplerDesc;
    samplerDesc.filter = SamplerFilter::Linear;
    samplerDesc.addressU =
        SamplerAddressMode::Repeat;
    samplerDesc.addressV =
        SamplerAddressMode::Repeat;

    auto sampler =
        backend->CreateSampler(samplerDesc);

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
    output.position = mul(float4(input.position, 1.0f), mvp);
    output.uv = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return BaseColor.Sample(BaseSampler, input.uv);
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
    pipelineDesc.vertexShader =
        vertexShader.get();
    pipelineDesc.pixelShader =
        pixelShader.get();
    pipelineDesc.vertexStride =
        sizeof(Vertex);
    pipelineDesc.constantBufferCount = 1;
    pipelineDesc.textureCount = 1;
    pipelineDesc.samplerCount = 1;
    pipelineDesc.depthTest = true;
    pipelineDesc.vertexAttributes = {
        {
            VertexSemantic::Position,
            0,
            VertexFormat::Float3,
            static_cast<std::uint32_t>(
                offsetof(Vertex, position))
        },
        {
            VertexSemantic::TexCoord,
            0,
            VertexFormat::Float2,
            static_cast<std::uint32_t>(
                offsetof(Vertex, uv))
        }
    };

    auto pipeline =
        backend->CreateGraphicsPipeline(
            pipelineDesc);

    if (!pipeline)
        return false;

    const float aspect =
        static_cast<float>(window.Width()) /
        static_cast<float>(window.Height());

    constexpr float pi =
        3.14159265358979323846f;

    const Mat4 projection =
        PerspectiveFovLH(
            pi / 3.0f,
            aspect,
            0.1f,
            100.0f);

    const Mat4 view =
        Translation(
            0.0f,
            0.0f,
            3.0f);

    const auto start =
        std::chrono::steady_clock::now();

    int renderedFrames = 0;

    while (window.PumpEvents()) {
        const auto now =
            std::chrono::steady_clock::now();

        const float seconds =
            std::chrono::duration<float>(
                now - start).count();

        const Mat4 model =
            Multiply(
                RotationY(seconds * 0.8f),
                RotationX(seconds * 0.35f));

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

        ICommandList* commands =
            backend->BeginFrame();

        if (!commands)
            return false;

        commands->BeginRenderPass(
            {0.012f, 0.022f, 0.040f, 1.0f});
        commands->SetPipeline(*pipeline);
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
            sizeof(Vertex));
        commands->SetIndexBuffer(
            *indexBuffer,
            IndexType::UInt16);
        commands->DrawIndexed(
            static_cast<std::uint32_t>(
                std::size(indices)));
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
            argc,
            argv,
            "--smoke-test");

#if defined(_WIN32)
    Hamun::Platform::WindowDesc windowDesc;
    windowDesc.title =
        "BDFR Hamun Engine - Textured RHI Mesh";
    windowDesc.width = 1280;
    windowDesc.height = 720;

    auto window =
        Hamun::Platform::CreateNativeWindow(
            windowDesc);

    if (!window)
        return 2;

    if (!RunTexturedMesh(
            *window,
            smokeTest))
        return 3;
#else
    (void)smokeTest;

    Log(
        LogLevel::Info,
        "Non-Windows CI validates the cross-platform RHI interface; native Vulkan rendering follows.");
#endif

    return 0;
}
