#pragma once

#include <Hamun/Renderer/RenderPass.hpp>

namespace Hamun::Renderer {

class MainRenderPass final : public RenderPass {
public:
    MainRenderPass()
        : RenderPass("MainRenderPass")
    {
    }

    void Setup() override {}
    void Execute() override {}
};

} // namespace Hamun::Renderer
