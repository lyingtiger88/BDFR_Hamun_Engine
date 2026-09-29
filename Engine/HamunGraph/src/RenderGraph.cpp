#include <HamunGraph/RenderGraph.hpp>
#include <HamunGraph/RenderPass.hpp>

#include <stdexcept>

namespace Hamun {

void RenderGraph::BeginFrame()
{
}

void RenderGraph::AddPass(
    const std::shared_ptr<RenderPass>& pass)
{
    if (!pass) {
        throw std::invalid_argument(
            "HamunGraph: cannot add a null render pass");
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

} // namespace Hamun
