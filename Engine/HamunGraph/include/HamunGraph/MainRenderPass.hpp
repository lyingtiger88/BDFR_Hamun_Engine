#pragma once

#include <HamunGraph/RenderPass.hpp>

namespace Hamun {

class MainRenderPass final : public RenderPass {
public:
    MainRenderPass()
        : RenderPass("MainRenderPass")
    {
    }

    void Setup() override {}
    void Execute() override {}
};

} // namespace Hamun
