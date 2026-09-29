#pragma once

#include <Hamun/Renderer/IPostSceneProcessor.hpp>
#include <Hamun/Renderer/RenderPass.hpp>

namespace Hamun::Renderer {

class PostScenePass final : public RenderPass {
public:
    PostScenePass();

    void Configure(
        RHI::ICommandList& commands,
        IPostSceneProcessor* processor) noexcept;

    void Reset() noexcept;
    void Execute() override;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return succeeded_;
    }

private:
    RHI::ICommandList* commands_ = nullptr;
    IPostSceneProcessor* processor_ = nullptr;
    bool succeeded_ = true;
};

} // namespace Hamun::Renderer
