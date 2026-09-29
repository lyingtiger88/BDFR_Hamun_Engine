#pragma once

#include <Hamun/Renderer/RenderDraw.hpp>
#include <Hamun/Renderer/RenderPass.hpp>

#include <Hamun/RHI/RHI.hpp>

#include <array>
#include <span>

namespace Hamun::Renderer {

class MainRenderPass final : public RenderPass {
public:
    MainRenderPass();

    void Configure(
        RHI::ICommandList& commands,
        RHI::IPipeline& pipeline,
        RHI::ISampler& sampler,
        std::span<const IndexedDraw> draws,
        RHI::ITexture* colorTarget,
        RHI::ITexture* depthTarget,
        const std::array<float, 4>& clearColor) noexcept;

    void Reset() noexcept;

    void Execute() override;

private:
    RHI::ICommandList* commands_ = nullptr;
    RHI::IPipeline* pipeline_ = nullptr;
    RHI::ISampler* sampler_ = nullptr;
    RHI::ITexture* colorTarget_ = nullptr;
    RHI::ITexture* depthTarget_ = nullptr;
    std::span<const IndexedDraw> draws_{};

    std::array<float, 4> clearColor_{
        0.018f,
        0.035f,
        0.060f,
        1.0f
    };
};

} // namespace Hamun::Renderer
