#pragma once

#include <Hamun/Renderer/RenderDraw.hpp>
#include <Hamun/Renderer/RenderPass.hpp>
#include <Hamun/RHI/RHI.hpp>

#include <span>

namespace Hamun::Renderer {

class MotionVectorPass final : public RenderPass {
public:
    MotionVectorPass();

    void Configure(
        RHI::ICommandList& commands,
        RHI::IPipeline* pipeline,
        std::span<const IndexedDraw> draws,
        RHI::ITexture* motionTarget,
        RHI::ITexture* depthTarget) noexcept;

    void Reset() noexcept;

    void Execute() override;

private:
    RHI::ICommandList* commands_ = nullptr;
    RHI::IPipeline* pipeline_ = nullptr;
    RHI::ITexture* motionTarget_ = nullptr;
    RHI::ITexture* depthTarget_ = nullptr;
    std::span<const IndexedDraw> draws_{};
};

} // namespace Hamun::Renderer
