#include <Hamun/Renderer/ComputePass.hpp>
#include <Hamun/Renderer/MainRenderPass.hpp>
#include <Hamun/Renderer/MotionVectorPass.hpp>
#include <Hamun/Renderer/PostScenePass.hpp>
#include <Hamun/Renderer/PresentPass.hpp>
#include <Hamun/Renderer/Renderer.hpp>

#include <Hamun/Core/Log.hpp>
#include <Hamun/RHI/RHI.hpp>

#include <memory>
#include <string>

namespace Hamun::Renderer {
namespace {

bool RenderFailure(
    const char* reason)
{
    Core::Log(
        Core::LogLevel::Error,
        std::string("Renderer::RenderFrame: ") +
            reason);

    return false;
}

} // namespace

Renderer::Renderer()
    : computePass_(
        std::make_shared<ComputePass>())
    , mainPass_(
        std::make_shared<MainRenderPass>())
    , motionVectorPass_(
        std::make_shared<MotionVectorPass>())
    , postScenePass_(
        std::make_shared<PostScenePass>())
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
        postScenePass_);

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
        return RenderFailure(
            "scene pipeline or sampler is missing");
    }

    for (const IndexedDraw& draw :
         submission.draws) {
        if (!draw.vertexBuffer ||
            !draw.indexBuffer ||
            !draw.constantBuffer ||
            !draw.texture ||
            draw.vertexStride == 0 ||
            draw.indexCount == 0) {
            return RenderFailure(
                "indexed draw contains an invalid resource or zero-sized draw");
        }
    }

    for (const ComputeDispatch& dispatch :
         submission.computeDispatches) {
        if (!dispatch.pipeline ||
            dispatch.groupCountX == 0 ||
            dispatch.groupCountY == 0 ||
            dispatch.groupCountZ == 0) {
            return RenderFailure(
                "compute dispatch is invalid");
        }

        for (RHI::IBuffer* buffer :
             dispatch.storageBuffers) {
            if (!buffer ||
                buffer->Usage() !=
                    RHI::BufferUsage::Storage) {
                return RenderFailure(
                    "compute storage buffer is null or not a Storage buffer");
            }
        }

        for (RHI::ITexture* texture :
             dispatch.storageTextures) {
            if (!texture ||
                !RHI::HasTextureUsage(
                    texture->Usage(),
                    RHI::TextureUsage::Storage)) {
                return RenderFailure(
                    "compute storage texture is null or lacks Storage usage");
            }
        }
    }

    if (submission.sceneColorTarget &&
        !RHI::HasTextureUsage(
            submission.sceneColorTarget->Usage(),
            RHI::TextureUsage::RenderTarget)) {
        return RenderFailure(
            "scene color target lacks RenderTarget usage");
    }

    if (submission.sceneDepthTarget &&
        !RHI::HasTextureUsage(
            submission.sceneDepthTarget->Usage(),
            RHI::TextureUsage::DepthStencil)) {
        return RenderFailure(
            "scene depth target lacks DepthStencil usage");
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
        return RenderFailure(
            "motion-vector pass resources are incomplete or invalid");
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
        return RenderFailure(
            "present resources are incomplete or source lacks ShaderResource usage");
    }

    RHI::ICommandList* commands =
        backend.BeginFrame();

    if (!commands)
        return RenderFailure(
            "backend BeginFrame returned no command list");

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

    postScenePass_->Configure(
        *commands,
        submission.postSceneProcessor);

    presentPass_->Configure(
        *commands,
        submission.presentPipeline,
        submission.presentSampler,
        submission.presentSource);

    BeginFrame();
    Render();
    EndFrame();

    const bool postSceneSucceeded =
        postScenePass_->Succeeded();

    computePass_->Reset();
    mainPass_->Reset();
    motionVectorPass_->Reset();
    postScenePass_->Reset();
    presentPass_->Reset();

    const bool submitted =
        backend.SubmitFrame();

    if (!postSceneSucceeded) {
        return RenderFailure(
            "post-scene processor reported failure");
    }

    if (!submitted) {
        return RenderFailure(
            "backend SubmitFrame failed");
    }

    return true;
}

} // namespace Hamun::Renderer
