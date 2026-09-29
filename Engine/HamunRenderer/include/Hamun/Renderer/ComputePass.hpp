#pragma once

#include <Hamun/Renderer/ComputeDispatch.hpp>
#include <Hamun/Renderer/RenderPass.hpp>

#include <Hamun/RHI/RHI.hpp>

#include <span>

namespace Hamun::Renderer {

class ComputePass final : public RenderPass {
public:
    ComputePass();

    void Configure(
        RHI::ICommandList& commands,
        std::span<const ComputeDispatch> dispatches) noexcept;

    void Reset() noexcept;

    void Execute() override;

private:
    RHI::ICommandList* commands_ = nullptr;
    std::span<const ComputeDispatch> dispatches_{};
};

} // namespace Hamun::Renderer
