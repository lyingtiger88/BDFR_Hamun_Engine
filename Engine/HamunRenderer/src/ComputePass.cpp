#include <Hamun/Renderer/ComputePass.hpp>

namespace Hamun::Renderer {

ComputePass::ComputePass()
    : RenderPass("ComputePass")
{
}

void ComputePass::Configure(
    RHI::ICommandList& commands,
    std::span<const ComputeDispatch> dispatches) noexcept
{
    commands_ = &commands;
    dispatches_ = dispatches;
}

void ComputePass::Reset() noexcept
{
    commands_ = nullptr;
    dispatches_ = {};
}

void ComputePass::Execute()
{
    if (!commands_)
        return;

    for (const ComputeDispatch& dispatch :
         dispatches_) {
        if (!dispatch.pipeline ||
            dispatch.groupCountX == 0 ||
            dispatch.groupCountY == 0 ||
            dispatch.groupCountZ == 0) {
            continue;
        }

        commands_->SetComputePipeline(
            *dispatch.pipeline);

        commands_->Dispatch(
            dispatch.groupCountX,
            dispatch.groupCountY,
            dispatch.groupCountZ);
    }
}

} // namespace Hamun::Renderer
