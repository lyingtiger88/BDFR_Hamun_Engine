#include <Hamun/Core/Log.hpp>
#include <Hamun/Graph/VM.hpp>
#include <Hamun/Platform/Window.hpp>
#include <Hamun/RHI/RHI.hpp>
#include <Hamun/World/StreamingScheduler.hpp>

#include <iostream>
#include <string_view>

namespace {

bool HasArgument(int argc, char** argv, std::string_view argument)
{
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == argument)
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
        std::cout << "Next stream request: "
                  << next->resource
                  << " score=" << next->score << '\n';
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
    if (const auto* result = std::get_if<double>(&value))
        std::cout << "HamunGraph VM test: 6 * 7 = " << *result << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    using Hamun::Core::Log;
    using Hamun::Core::LogLevel;

    Log(LogLevel::Info, "BDFR Hamun Engine sandbox booting");
    RunFoundationSelfTests();

    const bool smokeTest = HasArgument(argc, argv, "--smoke-test");

#if defined(_WIN32)
    Hamun::Platform::WindowDesc windowDesc;
    windowDesc.title = "BDFR Hamun Engine - DirectX 12";
    windowDesc.width = 1280;
    windowDesc.height = 720;

    auto window = Hamun::Platform::CreateWindow(windowDesc);
    if (!window)
        return 2;

    auto backend =
        Hamun::RHI::CreateBackend(Hamun::RHI::BackendType::D3D12);

    Hamun::RHI::BackendCreateInfo createInfo;
    createInfo.nativeWindowHandle = window->NativeHandle();
    createInfo.width = window->Width();
    createInfo.height = window->Height();
    createInfo.enableValidation = true;

    if (!backend || !backend->Initialize(createInfo))
        return 3;

    int renderedFrames = 0;
    while (window->PumpEvents()) {
        if (!backend->RenderFrame())
            return 4;

        ++renderedFrames;
        if (smokeTest && renderedFrames >= 3)
            break;
    }

    backend->Shutdown();
#else
    auto backend =
        Hamun::RHI::CreateBackend(Hamun::RHI::BackendType::Vulkan);

    if (backend) {
        Hamun::RHI::BackendCreateInfo createInfo;
        backend->Initialize(createInfo);
        backend->RenderFrame();
        backend->Shutdown();
    }

    (void)smokeTest;
#endif

    return 0;
}
