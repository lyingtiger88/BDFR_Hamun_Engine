#include <Hamun/Hair/HairSimulation.hpp>

#include <cstdint>
#include <string>

namespace Hamun::Hair {

bool HairSimulation::Initialize(
    RHI::IBackend& backend,
    HairGpuResources& resources)
{
    Reset();

    if (!backend.Caps().compute ||
        !resources.IsValid() ||
        resources.VertexCount() == 0) {
        return false;
    }

    static const std::string shaderSource = R"(
RWStructuredBuffer<float4> GuidePositions : register(u0);
RWStructuredBuffer<float4> SimulatedPositions : register(u1);

[numthreads(1, 1, 1)]
void HairSimulationCS(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint vertexIndex =
        dispatchThreadId.x;

    const float4 restPosition =
        GuidePositions[
            vertexIndex];

    // Bootstrap simulation pass:
    // preserve guide positions in the simulated stream.
    // TressFX constraints, wind and collision are layered on this path next.
    SimulatedPositions[
        vertexIndex] =
        restPosition;
}
)";

    RHI::ShaderDesc shaderDesc;
    shaderDesc.stage =
        RHI::ShaderStage::Compute;
    shaderDesc.source =
        shaderSource;
    shaderDesc.entryPoint =
        "HairSimulationCS";

    shader_ =
        backend.CreateShader(
            shaderDesc);

    if (!shader_) {
        Reset();
        return false;
    }

    RHI::ComputePipelineDesc
        pipelineDesc;

    pipelineDesc.computeShader =
        shader_.get();

    pipelineDesc.storageBufferCount =
        2;

    pipeline_ =
        backend.CreateComputePipeline(
            pipelineDesc);

    if (!pipeline_) {
        Reset();
        return false;
    }

    resources_ =
        &resources;

    return true;
}

void HairSimulation::Reset() noexcept
{
    pipeline_.reset();
    shader_.reset();
    resources_ = nullptr;
}

Renderer::ComputeDispatch
HairSimulation::BuildDispatch() const
{
    Renderer::ComputeDispatch
        dispatch;

    if (!IsReady())
        return dispatch;

    dispatch.pipeline =
        pipeline_.get();

    dispatch.storageBuffers = {
        resources_->GuidePositions(),
        resources_->SimulatedPositions()
    };

    dispatch.groupCountX =
        static_cast<std::uint32_t>(
            resources_->VertexCount());

    dispatch.groupCountY = 1;
    dispatch.groupCountZ = 1;

    return dispatch;
}

} // namespace Hamun::Hair
