#pragma once

#include <Hamun/Hair/HairGpuResources.hpp>
#include <Hamun/Renderer/ComputeDispatch.hpp>
#include <Hamun/RHI/RHI.hpp>

#include <memory>

namespace Hamun::Hair {

class HairSimulation {
public:
    bool Initialize(
        RHI::IBackend& backend,
        HairGpuResources& resources);

    void Reset() noexcept;

    [[nodiscard]] bool IsReady() const noexcept
    {
        return
            pipeline_ &&
            resources_ &&
            resources_->IsValid();
    }

    [[nodiscard]] Renderer::ComputeDispatch BuildDispatch() const;

private:
    HairGpuResources* resources_ = nullptr;

    std::unique_ptr<RHI::IShader>
        shader_;

    std::unique_ptr<RHI::IPipeline>
        pipeline_;
};

} // namespace Hamun::Hair
