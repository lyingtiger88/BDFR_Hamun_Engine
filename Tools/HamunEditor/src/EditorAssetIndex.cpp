#include "EditorAssetIndex.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <set>
#include <system_error>
#include <utility>

namespace Hamun::Editor {
namespace {

std::string LowerExtension(
    const std::filesystem::path& path)
{
    std::string extension =
        path.extension().string();

    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char value) {
            return static_cast<char>(
                std::tolower(value));
        });

    return extension;
}

bool Fail(
    std::string* error,
    std::string message)
{
    if (error)
        *error = std::move(message);

    return false;
}

bool CopyFileReplace(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    std::string* error)
{
    std::error_code ec;

    std::filesystem::copy_file(
        source,
        destination,
        std::filesystem::copy_options::overwrite_existing,
        ec);

    if (ec) {
        return Fail(
            error,
            "Could not copy asset '" +
                source.string() +
                "'.");
    }

    return true;
}


std::vector<std::filesystem::path> ReferencedGltfCompanions(
    const std::filesystem::path& gltfPath)
{
    std::ifstream input(gltfPath, std::ios::binary);
    if (!input)
        return {};

    const std::string json(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    static const std::regex uriPattern(
        R"hamun("uri"\s*:\s*"([^"]+)")hamun",
        std::regex::ECMAScript);

    std::set<std::filesystem::path> unique;

    for (std::sregex_iterator it(
             json.begin(),
             json.end(),
             uriPattern),
         end;
         it != end;
         ++it) {
        std::string uri = (*it)[1].str();

        if (uri.empty() ||
            uri.rfind("data:", 0) == 0 ||
            uri.find("://") != std::string::npos) {
            continue;
        }

        std::filesystem::path relative =
            std::filesystem::path(uri)
                .lexically_normal();

        if (relative.is_absolute())
            continue;

        bool traversal = false;
        for (const auto& component : relative) {
            if (component == "..") {
                traversal = true;
                break;
            }
        }

        if (traversal)
            continue;

        unique.insert(
            std::move(relative));
    }

    return {
        unique.begin(),
        unique.end()
    };
}

} // namespace

AssetKind ClassifyAsset(
    const std::filesystem::path& path)
{
    const std::string extension =
        LowerExtension(path);

    if (extension == ".hamunscene")
        return AssetKind::Scene;

    if (extension == ".gltf" ||
        extension == ".glb")
        return AssetKind::Gltf;

    if (extension == ".png" ||
        extension == ".jpg" ||
        extension == ".jpeg" ||
        extension == ".tga" ||
        extension == ".bmp")
        return AssetKind::Texture;

    if (extension == ".tfx")
        return AssetKind::Hair;

    if (extension == ".cpp" ||
        extension == ".c" ||
        extension == ".h" ||
        extension == ".hpp")
        return AssetKind::Source;

    if (extension == ".hamunproject")
        return AssetKind::Project;

    return AssetKind::Other;
}

std::wstring AssetKindLabel(
    AssetKind kind)
{
    switch (kind) {
        case AssetKind::Scene:
            return L"Scene";

        case AssetKind::Gltf:
            return L"3D";

        case AssetKind::Texture:
            return L"Texture";

        case AssetKind::Hair:
            return L"Hair";

        case AssetKind::Source:
            return L"Source";

        case AssetKind::Project:
            return L"Project";

        case AssetKind::Other:
        default:
            return L"File";
    }
}

std::vector<IndexedAsset> IndexProjectAssets(
    const std::filesystem::path& root)
{
    std::vector<IndexedAsset> assets;

    std::error_code ec;

    if (!std::filesystem::exists(
            root,
            ec)) {
        return assets;
    }

    const auto options =
        std::filesystem::directory_options::
            skip_permission_denied;

    for (std::filesystem::recursive_directory_iterator
             iterator(
                 root,
                 options,
                 ec),
         end;
         iterator != end;
         iterator.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }

        if (iterator->is_directory(ec)) {
            ec.clear();

            const std::string name =
                iterator->path()
                    .filename()
                    .string();

            if (name == ".git" ||
                name == ".vs" ||
                name == "build" ||
                name == "Build" ||
                name == "out" ||
                name == "dist") {
                iterator.disable_recursion_pending();
            }

            continue;
        }

        if (!iterator->is_regular_file(ec)) {
            ec.clear();
            continue;
        }

        IndexedAsset asset;
        asset.absolutePath =
            iterator->path();

        asset.relativePath =
            std::filesystem::relative(
                iterator->path(),
                root,
                ec);

        if (ec) {
            ec.clear();

            asset.relativePath =
                iterator->path()
                    .filename();
        }

        asset.kind =
            ClassifyAsset(
                iterator->path());

        assets.push_back(
            std::move(asset));
    }

    std::sort(
        assets.begin(),
        assets.end(),
        [](const IndexedAsset& a,
           const IndexedAsset& b) {
            return
                a.relativePath
                    .generic_string() <
                b.relativePath
                    .generic_string();
        });

    return assets;
}

bool ImportAssetWithCompanions(
    const std::filesystem::path& source,
    const std::filesystem::path& projectRoot,
    std::filesystem::path& importedPath,
    std::string* error)
{
    if (source.empty() ||
        !std::filesystem::is_regular_file(
            source)) {
        return Fail(
            error,
            "Import source file is invalid.");
    }

    const auto destinationRoot =
        projectRoot /
        "Assets" /
        "Imported" /
        source.stem();

    std::error_code ec;

    std::filesystem::create_directories(
        destinationRoot,
        ec);

    if (ec) {
        return Fail(
            error,
            "Could not create project import directory.");
    }

    importedPath =
        destinationRoot /
        source.filename();

    std::error_code equivalentError;

    const bool sameFile =
        std::filesystem::exists(
            importedPath,
            equivalentError) &&
        !equivalentError &&
        std::filesystem::equivalent(
            source,
            importedPath,
            equivalentError) &&
        !equivalentError;

    if (!sameFile &&
        !CopyFileReplace(
            source,
            importedPath,
            error)) {
        return false;
    }

    const std::string extension =
        LowerExtension(source);

    if (extension == ".gltf") {
        const auto sourceFolder =
            source.parent_path();

        for (const auto& relative :
             ReferencedGltfCompanions(source)) {
            const auto companionSource =
                sourceFolder /
                relative;

            if (!std::filesystem::is_regular_file(
                    companionSource,
                    ec)) {
                ec.clear();
                return Fail(
                    error,
                    "Referenced glTF companion is missing: " +
                        relative.generic_string());
            }

            const auto companionDestination =
                destinationRoot /
                relative;

            std::filesystem::create_directories(
                companionDestination.parent_path(),
                ec);

            if (ec) {
                return Fail(
                    error,
                    "Could not create glTF companion directory.");
            }

            if (!CopyFileReplace(
                    companionSource,
                    companionDestination,
                    error)) {
                return false;
            }
        }
    }

    return true;
}

} // namespace Hamun::Editor
