#include <Hamun/Renderer/MotionVectorPass.hpp>

namespace Hamun::Renderer {

MotionVectorPass::MotionVectorPass()
    : RenderPass("MotionVectorPass")
{
}

void MotionVectorPass::Configure(
    RHI::ICommandList& commands,
    RHI::IPipeline* pipeline,
    std::span<const IndexedDraw> draws,
    RHI::ITexture* motionTarget,
    RHI::ITexture* depthTarget) noexcept
{
    commands_ = &commands;
    pipeline_ = pipeline;
    draws_ = draws;
    motionTarget_ = motionTarget;
    depthTarget_ = depthTarget;
}

void MotionVectorPass::Reset() noexcept
{
    commands_ = nullptr;
    pipeline_ = nullptr;
    motionTarget_ = nullptr;
    depthTarget_ = nullptr;
    draws_ = {};
}

void MotionVectorPass::Execute()
{
    if (!commands_ ||
        !pipeline_ ||
        !motionTarget_) {
        return;
    }

    commands_->BeginRenderPassToTexture(
        *motionTarget_,
        depthTarget_,
        {
            0.0f,
            0.0f,
            0.0f,
            0.0f
        });

    commands_->SetPipeline(
        *pipeline_);

    for (const IndexedDraw& draw : draws_) {
        commands_->SetConstantBuffer(
            0,
            *draw.constantBuffer);

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
