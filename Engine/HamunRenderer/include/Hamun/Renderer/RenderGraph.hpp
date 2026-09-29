#pragma once

#include <cstddef>
#include <memory>
#include <vector>

namespace Hamun::Renderer {

class RenderPass;

class RenderGraph {
public:
    void BeginFrame();

    void AddPass(
        const std::shared_ptr<RenderPass>& pass);

    void Compile();
    void Execute();
    void EndFrame();

    [[nodiscard]] std::size_t PassCount() const noexcept
    {
        return passes_.size();
    }

private:
    std::vector<std::shared_ptr<RenderPass>> passes_;
};

} // namespace Hamun::Renderer
