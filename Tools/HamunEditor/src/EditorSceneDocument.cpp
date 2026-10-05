#include "EditorSceneDocument.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace Hamun::Editor {
namespace {

bool Fail(
    std::string* error,
    std::string message)
{
    if (error)
        *error = std::move(message);

    return false;
}

std::filesystem::path PortableSourcePath(
    const std::filesystem::path& scenePath,
    const std::filesystem::path& sourcePath)
{
    std::error_code ec;

    const auto relative =
        std::filesystem::relative(
            sourcePath,
            scenePath.parent_path(),
            ec);

    if (!ec &&
        !relative.empty()) {
        return relative;
    }

    return sourcePath;
}

} // namespace

bool SaveSceneDocument(
    const std::filesystem::path& path,
    const SceneDocument& document,
    std::string* error)
{
    if (document.sourceAsset.empty()) {
        return Fail(
            error,
            "Scene source asset is empty.");
    }

    std::error_code ec;

    std::filesystem::create_directories(
        path.parent_path(),
        ec);

    if (ec) {
        return Fail(
            error,
            "Could not create scene directory.");
    }

    std::ofstream output(
        path,
        std::ios::trunc);

    if (!output) {
        return Fail(
            error,
            "Could not open scene file for writing.");
    }

    output
        << "format=1\n"
        << "source="
        << std::quoted(
            PortableSourcePath(
                path,
                document.sourceAsset)
                .generic_string())
        << "\n";

    output
        << std::setprecision(9);

    for (std::size_t i = 0;
         i < document.transforms.size();
         ++i) {
        const SceneObjectTransform& transform =
            document.transforms[i];

        output
            << "object "
            << i << ' '
            << transform.position[0] << ' '
            << transform.position[1] << ' '
            << transform.position[2] << ' '
            << transform.scale[0] << ' '
            << transform.scale[1] << ' '
            << transform.scale[2]
            << "\n";
    }

    if (!output.good()) {
        return Fail(
            error,
            "Failed while writing scene file.");
    }

    return true;
}

bool LoadSceneDocument(
    const std::filesystem::path& path,
    SceneDocument& document,
    std::string* error)
{
    std::ifstream input(path);

    if (!input) {
        return Fail(
            error,
            "Could not open scene file.");
    }

    SceneDocument loaded;
    std::string line;
    bool formatFound = false;

    while (std::getline(
               input,
               line)) {
        if (line.empty())
            continue;

        if (line.rfind(
                "format=",
                0) == 0) {
            if (line != "format=1") {
                return Fail(
                    error,
                    "Unsupported scene format.");
            }

            formatFound = true;
            continue;
        }

        if (line.rfind(
                "source=",
                0) == 0) {
            std::istringstream stream(
                line.substr(7));

            std::string value;

            if (!(stream >>
                  std::quoted(value))) {
                return Fail(
                    error,
                    "Invalid scene source path.");
            }

            std::filesystem::path source =
                std::filesystem::path(value);

            if (source.is_relative()) {
                source =
                    path.parent_path() /
                    source;
            }

            loaded.sourceAsset =
                std::filesystem::weakly_canonical(
                    source);

            continue;
        }

        if (line.rfind(
                "object ",
                0) == 0) {
            std::istringstream stream(line);

            std::string keyword;
            std::size_t index = 0;
            SceneObjectTransform transform;

            if (!(stream >>
                  keyword >>
                  index >>
                  transform.position[0] >>
                  transform.position[1] >>
                  transform.position[2] >>
                  transform.scale[0] >>
                  transform.scale[1] >>
                  transform.scale[2])) {
                return Fail(
                    error,
                    "Invalid scene transform record.");
            }

            if (index >=
                loaded.transforms.size()) {
                loaded.transforms.resize(
                    index + 1);
            }

            loaded.transforms[index] =
                transform;
        }
    }

    if (!formatFound) {
        return Fail(
            error,
            "Scene format header is missing.");
    }

    if (loaded.sourceAsset.empty()) {
        return Fail(
            error,
            "Scene source asset is missing.");
    }

    if (!std::filesystem::exists(
            loaded.sourceAsset)) {
        return Fail(
            error,
            "Scene source asset does not exist: " +
                loaded.sourceAsset.string());
    }

    document =
        std::move(loaded);

    return true;
}

} // namespace Hamun::Editor
