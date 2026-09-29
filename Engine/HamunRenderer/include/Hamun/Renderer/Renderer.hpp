#pragma once

#include <Hamun/Renderer/RenderFrameSubmission.hpp>
#include <Hamun/Renderer/RenderGraph.hpp>

#include <cstddef>
#include <memory>

namespace Hamun::RHI {
class IBackend;
}

namespace Hamun::Renderer {

class ComputePass;
class MainRenderPass;
class MotionVectorPass;
class PresentPass;

class Renderer {
public:
    Renderer();

    void BeginFrame();
    void Render();
    void EndFrame();

    bool RenderFrame(
        RHI::IBackend& backend,
        const RenderFrameSubmission& submission);

    [[nodiscard]] std::size_t RenderPassCount() const noexcept
    {
        return renderGraph_.PassCount();
    }

private:
    std::shared_ptr<ComputePass> computePass_;
    std::shared_ptr<MainRenderPass> mainPass_;
    std::shared_ptr<MotionVectorPass> motionVectorPass_;
    std::shared_ptr<PresentPass> presentPass_;
    RenderGraph renderGraph_;
};

} // namespace Hamun::Renderer
