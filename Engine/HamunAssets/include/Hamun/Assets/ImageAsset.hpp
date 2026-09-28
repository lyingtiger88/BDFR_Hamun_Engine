#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Hamun::Assets {

struct ImageAsset {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba8;
};

std::optional<ImageAsset> LoadImageRGBA8(
    const std::filesystem::path& path,
    std::string* error = nullptr);

} // namespace Hamun::Assets
