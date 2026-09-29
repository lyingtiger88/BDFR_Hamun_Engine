#include <Hamun/Renderer/Renderer.hpp>
#include <HamunGraph/MainRenderPass.hpp>

#include <memory>

namespace Hamun::Renderer {

Renderer::Renderer()
{
    renderGraph_.AddPass(
        std::make_shared<Hamun::MainRenderPass>());
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

} // namespace Hamun::Renderer
