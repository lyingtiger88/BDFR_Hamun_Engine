#include <Hamun/Renderer/MainRenderPass.hpp>
#include <Hamun/Renderer/Renderer.hpp>

#include <Hamun/RHI/RHI.hpp>

#include <memory>

namespace Hamun::Renderer {

Renderer::Renderer()
    : mainPass_(
        std::make_shared<MainRenderPass>())
{
    renderGraph_.AddPass(
        mainPass_);
}

void Renderer::BeginFrame()
{
    renderGraph_.BeginFrame();
}

void Renderer::Render()
{
    renderGraph_.Compile();
    renderGraph_.Execute();
}

void Renderer::EndFrame()
{
    renderGraph_.EndFrame();
}

bool Renderer::RenderFrame(
    RHI::IBackend& backend,
    RHI::IPipeline& pipeline,
    RHI::ISampler& sampler,
    std::span<const IndexedDraw> draws,
    const std::array<float, 4>& clearColor)
{
    for (const IndexedDraw& draw : draws) {
        if (!draw.vertexBuffer ||
            !draw.indexBuffer ||
            !draw.constantBuffer ||
            !draw.texture ||
            draw.vertexStride == 0 ||
            draw.indexCount == 0) {
            return false;
        }
    }

    RHI::ICommandList* commands =
        backend.BeginFrame();

    if (!commands)
        return false;

    mainPass_->Configure(
        *commands,
        pipeline,
        sampler,
        draws,
        clearColor);

    BeginFrame();
    Render();
    EndFrame();

    mainPass_->Reset();

    return backend.SubmitFrame();
}

} // namespace Hamun::Renderer
