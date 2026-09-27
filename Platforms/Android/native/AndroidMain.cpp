#include <android/log.h>
#include <android_native_app_glue.h>

#include <Hamun/Core/Log.hpp>
#include <Hamun/Graph/VM.hpp>
#include <Hamun/World/StreamingScheduler.hpp>

namespace {

constexpr const char* kTag = "HamunEngine";

void HandleCommand(android_app* app, int32_t command)
{
    switch (command) {
        case APP_CMD_INIT_WINDOW:
            __android_log_print(
                ANDROID_LOG_INFO,
                kTag,
                "Android native window is ready.");
            break;

        case APP_CMD_TERM_WINDOW:
            __android_log_print(
                ANDROID_LOG_INFO,
                kTag,
                "Android native window was released.");
            break;

        case APP_CMD_GAINED_FOCUS:
            __android_log_print(
                ANDROID_LOG_INFO,
                kTag,
                "Hamun Engine gained focus.");
            break;

        case APP_CMD_LOST_FOCUS:
            __android_log_print(
                ANDROID_LOG_INFO,
                kTag,
                "Hamun Engine lost focus.");
            break;

        default:
            break;
    }

    (void)app;
}

void RunFoundationChecks()
{
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
    const auto* result = std::get_if<double>(&value);

    __android_log_print(
        ANDROID_LOG_INFO,
        kTag,
        "HamunGraph bootstrap result: %.1f",
        result ? *result : -1.0);

    Hamun::World::StreamingScheduler streaming;
    Hamun::World::WorldPosition observer{};
    Hamun::World::StreamingRequest request;
    request.id = 1;
    request.resource = "Android/BootstrapCell";
    request.position.cellX = 1;
    streaming.Enqueue(request, observer);

    __android_log_print(
        ANDROID_LOG_INFO,
        kTag,
        "HamunWorld bootstrap pending requests: %zu",
        streaming.PendingCount());
}

} // namespace

void android_main(android_app* app)
{
    app_dummy();

    app->onAppCmd = HandleCommand;

    __android_log_print(
        ANDROID_LOG_INFO,
        kTag,
        "BDFR Hamun Engine Android bootstrap started.");

    Hamun::Core::Log(
        Hamun::Core::LogLevel::Info,
        "Hamun Android native runtime started.");

    RunFoundationChecks();

    while (!app->destroyRequested) {
        int events = 0;
        android_poll_source* source = nullptr;

        while (ALooper_pollOnce(
                   -1,
                   nullptr,
                   &events,
                   reinterpret_cast<void**>(&source)) >= 0) {
            if (source)
                source->process(app, source);

            if (app->destroyRequested)
                break;
        }
    }

    __android_log_print(
        ANDROID_LOG_INFO,
        kTag,
        "BDFR Hamun Engine Android bootstrap stopped.");
}
