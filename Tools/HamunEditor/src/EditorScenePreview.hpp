#pragma once

#include <Hamun/Assets/GltfAsset.hpp>
#include <Hamun/Renderer/FrameResources.hpp>
#include <Hamun/Renderer/FreeCamera.hpp>
#include <Hamun/Renderer/Renderer.hpp>
#include <Hamun/RHI/RHI.hpp>
#include <Hamun/World/World.hpp>

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

    std::array<float, 3> rotationDegrees{
        0.0f, 0.0f, 0.0f
    };

    std::array<float, 3> scale{
        1.0f, 1.0f, 1.0f
    };
};

struct SceneMaterialState {
    std::array<float, 4> baseColorFactor{
        1.0f, 1.0f, 1.0f, 1.0f
    };

    float metallic = 0.0f;
    float roughness = 1.0f;
};

struct SceneObjectState {
    std::string name;
    std::uint32_t meshIndex = 0;
    std::uint32_t hierarchyDepth = 0;
    SceneObjectTransform transform{};
    SceneMaterialState material{};
};

struct SceneObjectInfo {
    std::size_t index = 0;
    World::EntityId entityId =
        World::InvalidEntity;
    World::EntityId parentEntityId =
        World::InvalidEntity;
    std::uint32_t hierarchyDepth = 0;
    std::string name;
    std::string meshName;
    std::uint32_t meshIndex = 0;
    SceneObjectTransform transform{};
    SceneMaterialState material{};
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

    bool SetMaterial(
        std::size_t index,
        const SceneMaterialState& material);

    bool SetName(
        std::size_t index,
        std::string name);

    [[nodiscard]] std::vector<SceneObjectState>
    CaptureObjects() const;

    bool ReplaceObjects(
        RHI::IBackend& backend,
        const std::vector<SceneObjectState>& objects,
        std::string* error = nullptr);

    bool DuplicateObject(
        RHI::IBackend& backend,
        std::size_t index,
        std::size_t* duplicatedIndex = nullptr,
        std::string* error = nullptr);

    bool DeleteObject(
        RHI::IBackend& backend,
        std::size_t index,
        std::string* error = nullptr);

    [[nodiscard]] std::optional<std::size_t>
    PickObject(
        float viewportX,
        float viewportY,
        float viewportWidth,
        float viewportHeight) const;

    void MoveCamera(
        float forward,
        float right,
        float up) noexcept;

    void RotateCamera(
        float yawDelta,
        float pitchDelta) noexcept;

    [[nodiscard]] Renderer::Vec3
    CameraPosition() const noexcept;

    [[nodiscard]] float CameraYaw() const noexcept;
    [[nodiscard]] float CameraPitch() const noexcept;

    void SetCameraPose(
        const Renderer::Vec3& position,
        float yaw,
        float pitch) noexcept;

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

    World::World world_;
    std::vector<World::EntityId>
        entityIds_;

    std::vector<SceneMaterialState>
        objectMaterials_;

    std::unique_ptr<RHI::IShader> vertexShader_;
    std::unique_ptr<RHI::IShader> pixelShader_;
    std::unique_ptr<RHI::IPipeline> pipeline_;
    std::unique_ptr<RHI::ISampler> sampler_;

    Renderer::FrameResources frameResources_;
    Renderer::Renderer renderer_;
    Renderer::FreeCamera camera_;
    std::vector<Renderer::IndexedDraw> draws_;

    bool RebuildSceneRuntime(
        RHI::IBackend& backend,
        std::string* error);

    std::uint32_t width_ = 1;
    std::uint32_t height_ = 1;
};

} // namespace Hamun::Editor
