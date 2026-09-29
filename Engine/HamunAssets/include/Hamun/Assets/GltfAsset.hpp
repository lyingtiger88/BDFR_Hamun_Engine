#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Hamun::Assets {

struct MeshVertex {
    float position[3]{};
    float normal[3]{0.0f, 1.0f, 0.0f};
    float uv[2]{};
};

struct MeshAsset {
    std::string name;
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::filesystem::path baseColorTexture;
    std::array<float, 4> baseColorFactor{
        1.0f, 1.0f, 1.0f, 1.0f
    };

    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
};

struct SceneInstance {
    std::string name;
    std::uint32_t meshIndex = 0;

    // Flat glTF world matrix. Hamun currently uses row-vector HLSL math;
    // copying the glTF column-major flat representation directly gives
    // the transposed mathematical matrix required by that convention.
    std::array<float, 16> worldMatrix{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
};

struct GltfAsset {
    std::vector<MeshAsset> meshes;
    std::vector<SceneInstance> instances;
};

std::optional<GltfAsset> LoadGltf(
    const std::filesystem::path& path,
    std::string* error = nullptr);

} // namespace Hamun::Assets
