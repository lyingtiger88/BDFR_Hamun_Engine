#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Hamun::Hair {

struct TfxAsset {
    float version = 0.0f;
    std::uint32_t guideStrandCount = 0;
    std::uint32_t verticesPerStrand = 0;

    std::vector<std::array<float, 4>> positions;
    std::vector<std::array<float, 2>> strandUv;

    [[nodiscard]] std::size_t VertexCount() const noexcept
    {
        return positions.size();
    }
};

std::optional<TfxAsset> LoadTfx(
    std::span<const std::byte> bytes,
    std::string* error = nullptr);

std::optional<TfxAsset> LoadTfx(
    const std::filesystem::path& path,
    std::string* error = nullptr);

} // namespace Hamun::Hair
