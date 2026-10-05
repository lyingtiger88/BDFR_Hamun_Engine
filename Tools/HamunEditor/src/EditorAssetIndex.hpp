#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Hamun::Editor {

enum class AssetKind {
    Scene,
    Gltf,
    Texture,
    Hair,
    Source,
    Project,
    Other
};

struct IndexedAsset {
    std::filesystem::path absolutePath;
    std::filesystem::path relativePath;
    AssetKind kind = AssetKind::Other;
};

[[nodiscard]] AssetKind ClassifyAsset(
    const std::filesystem::path& path);

[[nodiscard]] std::wstring AssetKindLabel(
    AssetKind kind);

std::vector<IndexedAsset> IndexProjectAssets(
    const std::filesystem::path& root);

bool ImportAssetWithCompanions(
    const std::filesystem::path& source,
    const std::filesystem::path& projectRoot,
    std::filesystem::path& importedPath,
    std::string* error = nullptr);

} // namespace Hamun::Editor
