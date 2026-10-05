#pragma once

#include <Hamun/Assets/GltfAsset.hpp>
#include <Hamun/Renderer/FrameResources.hpp>
#include <Hamun/Renderer/FreeCamera.hpp>
#include <Hamun/Renderer/Renderer.hpp>
#include <Hamun/RHI/RHI.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Hamun::Editor {

struct SceneObjectTransform {
    std::array<float, 3> position{
        0.0f, 0.0f, 0.0f
    };

    std::array<float, 3> scale{
        1.0f, 1.0f, 1.0f
    };
};

struct SceneObjectInfo {
    std::size_t index = 0;
    std::uint32_t hierarchyDepth = 0;
    std::string name;
    std::string meshName;
    SceneObjectTransform transform{};

    std::array<float, 4> baseColorFactor{
        1.0f, 1.0f, 1.0f, 1.0f
    };

    float metallic = 0.0f;
    float roughness = 1.0f;
};

class ScenePreview {
public:
    bool Initialize(
        RHI::IBackend& backend,
        const std::filesystem::path& scenePath,
        std::uint32_t width,
        std::uint32_t height,
        std::string* error = nullptr);

    void Reset() noexcept;

    bool RenderFrame(
        RHI::IBackend& backend);

    [[nodiscard]] bool Ready() const noexcept;
    [[nodiscard]] std::size_t InstanceCount() const noexcept;

    [[nodiscard]] std::optional<SceneObjectInfo>
    ObjectInfo(
        std::size_t index) const;

    bool SetTransform(
        std::size_t index,
        const SceneObjectTransform& transform);

    void MoveCamera(
        float forward,
        float right,
        float up) noexcept;

    void RotateCamera(
        float yawDelta,
        float pitchDelta) noexcept;

    [[nodiscard]] Renderer::Vec3
    CameraPosition() const noexcept;

    [[nodiscard]] const std::filesystem::path& ScenePath() const noexcept;

private:
    struct RenderMesh {
        std::unique_ptr<RHI::IBuffer> vertexBuffer;
        std::unique_ptr<RHI::IBuffer> indexBuffer;
        std::unique_ptr<RHI::ITexture> texture;

        std::array<float, 4> baseColorFactor{
            1.0f, 1.0f, 1.0f, 1.0f
        };

        float metallic = 0.0f;
        float roughness = 1.0f;
        std::uint32_t indexCount = 0;
    };

    std::filesystem::path scenePath_;
    std::optional<Assets::GltfAsset> asset_;
    std::vector<RenderMesh> meshes_;

    std::unique_ptr<RHI::IShader> vertexShader_;
    std::unique_ptr<RHI::IShader> pixelShader_;
    std::unique_ptr<RHI::IPipeline> pipeline_;
    std::unique_ptr<RHI::ISampler> sampler_;

    Renderer::FrameResources frameResources_;
    Renderer::Renderer renderer_;
    Renderer::FreeCamera camera_;
    std::vector<Renderer::IndexedDraw> draws_;

    std::uint32_t width_ = 1;
    std::uint32_t height_ = 1;
};

} // namespace Hamun::Editor
