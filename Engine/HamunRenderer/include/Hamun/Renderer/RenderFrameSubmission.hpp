#pragma once

#include <Hamun/Renderer/ComputeDispatch.hpp>
#include <Hamun/Renderer/IPostSceneProcessor.hpp>
#include <Hamun/Renderer/RenderDraw.hpp>
#include <Hamun/RHI/RHI.hpp>

#include <array>
#include <span>

namespace Hamun::Renderer {

struct RenderFrameSubmission {
    RHI::IPipeline* scenePipeline = nullptr;
    RHI::ISampler* sceneSampler = nullptr;
    std::span<const IndexedDraw> draws{};
    std::span<const ComputeDispatch> computeDispatches{};

    RHI::ITexture* sceneColorTarget = nullptr;
    RHI::ITexture* sceneDepthTarget = nullptr;

    RHI::IPipeline* motionPipeline = nullptr;
    RHI::ITexture* motionTarget = nullptr;

    IPostSceneProcessor* postSceneProcessor = nullptr;

    RHI::IPipeline* presentPipeline = nullptr;
    RHI::ISampler* presentSampler = nullptr;
    RHI::ITexture* presentSource = nullptr;

    std::array<float, 4> clearColor{
        0.018f,
        0.035f,
        0.060f,
        1.0f
    };
};

} // namespace Hamun::Renderer
