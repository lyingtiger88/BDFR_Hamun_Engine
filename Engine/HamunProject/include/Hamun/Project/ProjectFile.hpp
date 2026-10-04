#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace Hamun::Project {

struct ProjectDescriptor {
    std::string name;
    std::string templateId;
    std::string engineName;
    int formatVersion = 0;
    std::filesystem::path projectFile;
    std::filesystem::path rootDirectory;
};

std::optional<ProjectDescriptor> LoadProjectFile(
    const std::filesystem::path& projectFile,
    std::string* error = nullptr);

} // namespace Hamun::Project
