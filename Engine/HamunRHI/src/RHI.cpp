#include <Hamun/RHI/RHI.hpp>

#include <sstream>

namespace Hamun::RHI {

std::unique_ptr<IBackend> CreateD3D12Backend();
std::unique_ptr<IBackend> CreateD3D11Backend();
std::unique_ptr<IBackend> CreateVulkanBackend();
std::unique_ptr<IBackend> CreateGLESBackend();

std::unique_ptr<IBackend> CreateBackend(BackendType type)
{
    switch (type) {
        case BackendType::D3D12:
            return CreateD3D12Backend();

        case BackendType::D3D11:
            return CreateD3D11Backend();

        case BackendType::Vulkan:
            return CreateVulkanBackend();

        case BackendType::OpenGLES:
            return CreateGLESBackend();
    }

    return {};
}

std::string BuildCapabilityReport(
    const IBackend& backend)
{
    const AdapterInfo& adapter =
        backend.Adapter();

    const Capabilities& caps =
        backend.Caps();

    constexpr std::uint64_t bytesPerMiB =
        1024ull * 1024ull;

    std::ostringstream output;

    output
        << "Hamun Hardware Report\n"
        << "  Backend: " << backend.Name() << '\n';

    if (!adapter.name.empty()) {
        output
            << "  GPU: " << adapter.name << '\n'
            << "  Dedicated VRAM: "
            << adapter.dedicatedVideoMemory / bytesPerMiB
            << " MiB\n"
            << "  Shared Memory: "
            << adapter.sharedSystemMemory / bytesPerMiB
            << " MiB\n"
            << "  Vendor ID: "
            << adapter.vendorId
            << "\n  Device ID: "
            << adapter.deviceId
            << '\n';
    } else {
        output
            << "  GPU: unavailable from this backend\n";
    }

    const auto support =
        [](bool value) {
            return value ? "YES" : "NO";
        };

    output
        << "  Compute: " << support(caps.compute) << '\n'
        << "  Async Compute: " << support(caps.asyncCompute) << '\n'
        << "  Indirect Draw: " << support(caps.indirectDraw) << '\n'
        << "  Bindless/Resource Indexing: " << support(caps.bindless) << '\n'
        << "  Mesh Shaders: " << support(caps.meshShaders) << '\n'
        << "  Ray Tracing: " << support(caps.rayTracing) << '\n';

    return output.str();
}

} // namespace Hamun::RHI
