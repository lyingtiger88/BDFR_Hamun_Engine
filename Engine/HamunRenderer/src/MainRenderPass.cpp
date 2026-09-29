#include <Hamun/Renderer/MainRenderPass.hpp>

namespace Hamun::Renderer {

MainRenderPass::MainRenderPass()
    : RenderPass("MainRenderPass")
{
}

void MainRenderPass::Configure(
    RHI::ICommandList& commands,
    RHI::IPipeline& pipeline,
    RHI::ISampler& sampler,
    std::span<const IndexedDraw> draws,
    RHI::ITexture* colorTarget,
    RHI::ITexture* depthTarget,
    const std::array<float, 4>& clearColor) noexcept
{
    commands_ = &commands;
    pipeline_ = &pipeline;
    sampler_ = &sampler;
    colorTarget_ = colorTarget;
    depthTarget_ = depthTarget;
    draws_ = draws;
    clearColor_ = clearColor;
}

void MainRenderPass::Reset() noexcept
{
    commands_ = nullptr;
    pipeline_ = nullptr;
    sampler_ = nullptr;
    colorTarget_ = nullptr;
    depthTarget_ = nullptr;
    draws_ = {};
}

void MainRenderPass::Execute()
{
    if (!commands_ ||
        !pipeline_ ||
        !sampler_) {
        return;
    }

    if (colorTarget_) {
        commands_->BeginRenderPassToTexture(
            *colorTarget_,
            depthTarget_,
            clearColor_);
    } else {
        commands_->BeginRenderPass(
            clearColor_);
    }

    commands_->SetPipeline(
        *pipeline_);

    commands_->SetSampler(
        0,
        *sampler_);

    for (const IndexedDraw& draw : draws_) {
        commands_->SetConstantBuffer(
            0,
            *draw.constantBuffer);

        commands_->SetTexture(
            0,
            *draw.texture);

        commands_->SetVertexBuffer(
            *draw.vertexBuffer,
            draw.vertexStride);

        commands_->SetIndexBuffer(
            *draw.indexBuffer,
            draw.indexType);

        commands_->DrawIndexed(
            draw.indexCount);
    }

    commands_->EndRenderPass();
}

} // namespace Hamun::Renderer
