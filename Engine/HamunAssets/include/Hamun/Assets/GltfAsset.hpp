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
};

struct GltfAsset {
    std::vector<MeshAsset> meshes;
};

std::optional<GltfAsset> LoadGltf(
    const std::filesystem::path& path,
    std::string* error = nullptr);

} // namespace Hamun::Assets
