#include <Hamun/RHI/RHI.hpp>

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

} // namespace Hamun::RHI
