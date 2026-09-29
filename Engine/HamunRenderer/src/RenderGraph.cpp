#include <Hamun/Renderer/RenderGraph.hpp>
#include <Hamun/Renderer/RenderPass.hpp>

#include <stdexcept>

namespace Hamun::Renderer {

void RenderGraph::BeginFrame()
{
}

void RenderGraph::AddPass(
    const std::shared_ptr<RenderPass>& pass)
{
    if (!pass) {
        throw std::invalid_argument(
            "HamunRenderer: cannot add a null render pass");
    }

    passes_.push_back(pass);
}

void RenderGraph::Compile()
{
    for (const auto& pass : passes_) {
        pass->Setup();
    }
}

void RenderGraph::Execute()
{
    for (const auto& pass : passes_) {
        pass->Execute();
    }
}

void RenderGraph::EndFrame()
{
}

} // namespace Hamun::Renderer
