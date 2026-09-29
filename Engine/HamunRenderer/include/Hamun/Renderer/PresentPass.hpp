#pragma once

#include <Hamun/Renderer/RenderPass.hpp>
#include <Hamun/RHI/RHI.hpp>

namespace Hamun::Renderer {

class PresentPass final : public RenderPass {
public:
    PresentPass();

    void Configure(
        RHI::ICommandList& commands,
        RHI::IPipeline* pipeline,
        RHI::ISampler* sampler,
        RHI::ITexture* source) noexcept;

    void Reset() noexcept;

    void Execute() override;

private:
    RHI::ICommandList* commands_ = nullptr;
    RHI::IPipeline* pipeline_ = nullptr;
    RHI::ISampler* sampler_ = nullptr;
    RHI::ITexture* source_ = nullptr;
};

} // namespace Hamun::Renderer
