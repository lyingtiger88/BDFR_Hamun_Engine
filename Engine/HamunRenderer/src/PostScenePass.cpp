#include <Hamun/Renderer/PostScenePass.hpp>

namespace Hamun::Renderer {

PostScenePass::PostScenePass()
    : RenderPass("PostScenePass")
{
}

void PostScenePass::Configure(
    RHI::ICommandList& commands,
    IPostSceneProcessor* processor) noexcept
{
    commands_ = &commands;
    processor_ = processor;
    succeeded_ = true;
}

void PostScenePass::Reset() noexcept
{
    commands_ = nullptr;
    processor_ = nullptr;
    succeeded_ = true;
}

void PostScenePass::Execute()
{
    if (!commands_ ||
        !processor_) {
        return;
    }

    succeeded_ =
        processor_->Execute(
            *commands_);
}

} // namespace Hamun::Renderer
