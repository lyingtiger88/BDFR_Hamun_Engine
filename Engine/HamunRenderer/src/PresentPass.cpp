#include <Hamun/Renderer/PresentPass.hpp>

namespace Hamun::Renderer {

PresentPass::PresentPass()
    : RenderPass("PresentPass")
{
}

void PresentPass::Configure(
    RHI::ICommandList& commands,
    RHI::IPipeline* pipeline,
    RHI::ISampler* sampler,
    RHI::ITexture* source) noexcept
{
    commands_ = &commands;
    pipeline_ = pipeline;
    sampler_ = sampler;
    source_ = source;
}

void PresentPass::Reset() noexcept
{
    commands_ = nullptr;
    pipeline_ = nullptr;
    sampler_ = nullptr;
    source_ = nullptr;
}

void PresentPass::Execute()
{
    if (!commands_ ||
        !pipeline_ ||
        !sampler_ ||
        !source_) {
        return;
    }

    commands_->BeginRenderPass(
        {
            0.0f,
            0.0f,
            0.0f,
            1.0f
        });

    commands_->SetPipeline(
        *pipeline_);

    commands_->SetTexture(
        0,
        *source_);

    commands_->SetSampler(
        0,
        *sampler_);

    commands_->Draw(
        3,
        0);

    commands_->EndRenderPass();
}

} // namespace Hamun::Renderer
