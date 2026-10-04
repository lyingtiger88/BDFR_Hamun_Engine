#include <Hamun/Project/ProjectFile.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <map>
#include <string>

namespace Hamun::Project {
namespace {

std::string Trim(std::string value)
{
    const auto notSpace =
        [](unsigned char c) {
            return !std::isspace(c);
        };

    value.erase(
        value.begin(),
        std::find_if(
            value.begin(),
            value.end(),
            notSpace));

    value.erase(
        std::find_if(
            value.rbegin(),
            value.rend(),
            notSpace).base(),
        value.end());

    return value;
}

void SetError(
    std::string* error,
    const std::string& message)
{
    if (error)
        *error = message;
}

} // namespace

std::optional<ProjectDescriptor> LoadProjectFile(
    const std::filesystem::path& projectFile,
    std::string* error)
{
    std::ifstream file(
        projectFile,
        std::ios::binary);

    if (!file) {
        SetError(
            error,
            "Could not open project file: " +
                projectFile.string());
        return std::nullopt;
    }

    std::map<std::string, std::string>
        values;

    std::string line;
    bool firstLine = true;

    while (std::getline(
        file,
        line)) {
        if (firstLine) {
            firstLine = false;

            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line.erase(
                    0,
                    3);
            }
        }

        line = Trim(
            line);

        if (line.empty() ||
            line[0] == '#') {
            continue;
        }

        const std::size_t separator =
            line.find('=');

        if (separator ==
            std::string::npos) {
            continue;
        }

        const std::string key =
            Trim(
                line.substr(
                    0,
                    separator));

        const std::string value =
            Trim(
                line.substr(
                    separator + 1));

        if (!key.empty()) {
            values[key] =
                value;
        }
    }

    if (values["name"].empty()) {
        SetError(
            error,
            "Project file is missing the required 'name' field.");
        return std::nullopt;
    }

    if (values["format"].empty()) {
        SetError(
            error,
            "Project file is missing the required 'format' field.");
        return std::nullopt;
    }

    int formatVersion = 0;

    const auto formatText =
        values["format"];

    const auto parseResult =
        std::from_chars(
            formatText.data(),
            formatText.data() +
                formatText.size(),
            formatVersion);

    if (parseResult.ec !=
            std::errc{} ||
        parseResult.ptr !=
            formatText.data() +
                formatText.size() ||
        formatVersion <= 0) {
        SetError(
            error,
            "Project file contains an invalid format version.");
        return std::nullopt;
    }

    ProjectDescriptor descriptor;
    descriptor.name =
        values["name"];
    descriptor.templateId =
        values["template"];
    descriptor.engineName =
        values["engine"].empty()
            ? "BDFR Hamun Engine"
            : values["engine"];
    descriptor.formatVersion =
        formatVersion;
    descriptor.projectFile =
        std::filesystem::absolute(
            projectFile);
    descriptor.rootDirectory =
        descriptor.projectFile
            .parent_path();

    return descriptor;
}

} // namespace Hamun::Project
