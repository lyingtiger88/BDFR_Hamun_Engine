#include <Hamun/Renderer/ComputePass.hpp>
#include <Hamun/Renderer/MainRenderPass.hpp>
#include <Hamun/Renderer/MotionVectorPass.hpp>
#include <Hamun/Renderer/PresentPass.hpp>
#include <Hamun/Renderer/Renderer.hpp>

#include <Hamun/RHI/RHI.hpp>

#include <memory>

namespace Hamun::Renderer {

Renderer::Renderer()
    : computePass_(
        std::make_shared<ComputePass>())
    , mainPass_(
        std::make_shared<MainRenderPass>())
    , motionVectorPass_(
        std::make_shared<MotionVectorPass>())
    , presentPass_(
        std::make_shared<PresentPass>())
{
    renderGraph_.AddPass(
        computePass_);

    renderGraph_.AddPass(
        mainPass_);

    renderGraph_.AddPass(
        motionVectorPass_);

    renderGraph_.AddPass(
        presentPass_);
}

void Renderer::BeginFrame()
{
    renderGraph_.BeginFrame();
}

void Renderer::Render()
{
    renderGraph_.Compile();
    renderGraph_.Execute();
}

void Renderer::EndFrame()
{
    renderGraph_.EndFrame();
}

bool Renderer::RenderFrame(
    RHI::IBackend& backend,
    const RenderFrameSubmission& submission)
{
    if (!submission.scenePipeline ||
        !submission.sceneSampler) {
        return false;
    }

    for (const IndexedDraw& draw :
         submission.draws) {
        if (!draw.vertexBuffer ||
            !draw.indexBuffer ||
            !draw.constantBuffer ||
            !draw.texture ||
            draw.vertexStride == 0 ||
            draw.indexCount == 0) {
            return false;
        }
    }

    for (const ComputeDispatch& dispatch :
         submission.computeDispatches) {
        if (!dispatch.pipeline ||
            dispatch.groupCountX == 0 ||
            dispatch.groupCountY == 0 ||
            dispatch.groupCountZ == 0) {
            return false;
        }

        for (RHI::IBuffer* buffer :
             dispatch.storageBuffers) {
            if (!buffer ||
                buffer->Usage() !=
                    RHI::BufferUsage::Storage) {
                return false;
            }
        }

        for (RHI::ITexture* texture :
             dispatch.storageTextures) {
            if (!texture ||
                !RHI::HasTextureUsage(
                    texture->Usage(),
                    RHI::TextureUsage::Storage)) {
                return false;
            }
        }
    }

    if (submission.sceneColorTarget &&
        !RHI::HasTextureUsage(
            submission.sceneColorTarget->Usage(),
            RHI::TextureUsage::RenderTarget)) {
        return false;
    }

    if (submission.sceneDepthTarget &&
        !RHI::HasTextureUsage(
            submission.sceneDepthTarget->Usage(),
            RHI::TextureUsage::DepthStencil)) {
        return false;
    }

    const bool motionRequested =
        submission.motionPipeline ||
        submission.motionTarget;

    if (motionRequested &&
        (!submission.motionPipeline ||
         !submission.motionTarget ||
         !RHI::HasTextureUsage(
             submission.motionTarget->Usage(),
             RHI::TextureUsage::RenderTarget))) {
        return false;
    }

    const bool presentRequested =
        submission.presentPipeline ||
        submission.presentSampler ||
        submission.presentSource;

    if (presentRequested &&
        (!submission.presentPipeline ||
         !submission.presentSampler ||
         !submission.presentSource ||
         !RHI::HasTextureUsage(
             submission.presentSource->Usage(),
             RHI::TextureUsage::ShaderResource))) {
        return false;
    }

    RHI::ICommandList* commands =
        backend.BeginFrame();

    if (!commands)
        return false;

    computePass_->Configure(
        *commands,
        submission.computeDispatches);

    mainPass_->Configure(
        *commands,
        *submission.scenePipeline,
        *submission.sceneSampler,
        submission.draws,
        submission.sceneColorTarget,
        submission.sceneDepthTarget,
        submission.clearColor);

    motionVectorPass_->Configure(
        *commands,
        submission.motionPipeline,
        submission.draws,
        submission.motionTarget,
        submission.sceneDepthTarget);

    presentPass_->Configure(
        *commands,
        submission.presentPipeline,
        submission.presentSampler,
        submission.presentSource);

    BeginFrame();
    Render();
    EndFrame();

    computePass_->Reset();
    mainPass_->Reset();
    motionVectorPass_->Reset();
    presentPass_->Reset();

    return backend.SubmitFrame();
}

} // namespace Hamun::Renderer
