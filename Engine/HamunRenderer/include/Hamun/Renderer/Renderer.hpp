#pragma once

#include <Hamun/Renderer/RenderDraw.hpp>
#include <Hamun/Renderer/RenderGraph.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <span>

namespace Hamun::RHI {
class IBackend;
class IPipeline;
class ISampler;
}

namespace Hamun::Renderer {

class MainRenderPass;

class Renderer {
public:
    Renderer();

    void BeginFrame();
    void Render();
    void EndFrame();

    bool RenderFrame(
        RHI::IBackend& backend,
        RHI::IPipeline& pipeline,
        RHI::ISampler& sampler,
        std::span<const IndexedDraw> draws,
        const std::array<float, 4>& clearColor = {
            0.018f,
            0.035f,
            0.060f,
            1.0f
        });

    [[nodiscard]] std::size_t RenderPassCount() const noexcept
    {
        return renderGraph_.PassCount();
    }

private:
    std::shared_ptr<MainRenderPass> mainPass_;
    RenderGraph renderGraph_;
};

} // namespace Hamun::Renderer
