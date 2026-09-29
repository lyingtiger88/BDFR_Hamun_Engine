#include <Hamun/Renderer/ComputePass.hpp>
#include <Hamun/Renderer/MainRenderPass.hpp>
#include <Hamun/Renderer/Renderer.hpp>

#include <Hamun/RHI/RHI.hpp>

#include <memory>

namespace Hamun::Renderer {

Renderer::Renderer()
    : computePass_(
        std::make_shared<ComputePass>())
    , mainPass_(
        std::make_shared<MainRenderPass>())
{
    renderGraph_.AddPass(
        computePass_);

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
    std::span<const ComputeDispatch> computeDispatches,
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

    for (const ComputeDispatch& dispatch :
         computeDispatches) {
        if (!dispatch.pipeline ||
            dispatch.groupCountX == 0 ||
            dispatch.groupCountY == 0 ||
            dispatch.groupCountZ == 0) {
            return false;
        }

        for (RHI::IBuffer* buffer :
             dispatch.storageBuffers) {
            if (!buffer ||
                buffer->Usage() !=
                    RHI::BufferUsage::Storage) {
                return false;
            }
        }
    }

    RHI::ICommandList* commands =
        backend.BeginFrame();

    if (!commands)
        return false;

    computePass_->Configure(
        *commands,
        computeDispatches);

    mainPass_->Configure(
        *commands,
        pipeline,
        sampler,
        draws,
        clearColor);

    BeginFrame();
    Render();
    EndFrame();

    computePass_->Reset();
    mainPass_->Reset();

    return backend.SubmitFrame();
}

} // namespace Hamun::Renderer
