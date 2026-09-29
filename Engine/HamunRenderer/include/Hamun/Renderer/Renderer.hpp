#pragma once

#include <HamunGraph/RenderGraph.hpp>

#include <cstddef>

namespace Hamun::Renderer {

class Renderer {
public:
    Renderer();

    void BeginFrame();
    void Render();
    void EndFrame();

    [[nodiscard]] std::size_t RenderPassCount() const noexcept
    {
        return renderGraph_.PassCount();
    }

private:
    Hamun::RenderGraph renderGraph_;
};

} // namespace Hamun::Renderer
