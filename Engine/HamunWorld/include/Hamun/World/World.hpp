#pragma once

#include <Hamun/World/WorldPosition.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Hamun::World {

using EntityId = std::uint64_t;
inline constexpr EntityId InvalidEntity = 0;

struct TransformComponent {
    WorldPosition position{};

    std::array<float, 3> rotationDegrees{
        0.0f,
        0.0f,
        0.0f
    };

    std::array<float, 3> scale{
        1.0f,
        1.0f,
        1.0f
    };
};

struct MeshComponent {
    std::filesystem::path assetPath;
    std::uint32_t meshIndex = 0;
};

struct MaterialComponent {
    std::array<float, 4> baseColorFactor{
        1.0f,
        1.0f,
        1.0f,
        1.0f
    };

    float metallic = 0.0f;
    float roughness = 1.0f;
};

struct EntityRecord {
    EntityId id = InvalidEntity;
    EntityId parent = InvalidEntity;
    std::string name;
    TransformComponent transform{};
    std::optional<MeshComponent> mesh;
    std::optional<MaterialComponent> material;
};

class World {
public:
    EntityId CreateEntity(
        std::string name,
        EntityId parent = InvalidEntity);

    bool DestroyEntity(
        EntityId entity);

    bool SetParent(
        EntityId entity,
        EntityId parent);

    [[nodiscard]] EntityRecord* Find(
        EntityId entity) noexcept;

    [[nodiscard]] const EntityRecord* Find(
        EntityId entity) const noexcept;

    [[nodiscard]] const std::vector<EntityRecord>&
    Entities() const noexcept
    {
        return entities_;
    }

    [[nodiscard]] std::size_t EntityCount() const noexcept
    {
        return entities_.size();
    }

    void Clear() noexcept;

private:
    [[nodiscard]] bool WouldCreateCycle(
        EntityId entity,
        EntityId parent) const noexcept;

    EntityId nextEntityId_ = 1;
    std::vector<EntityRecord> entities_;
};

} // namespace Hamun::World
