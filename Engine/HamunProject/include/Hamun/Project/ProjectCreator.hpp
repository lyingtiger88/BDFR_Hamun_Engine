#pragma once

#include <Hamun/Project/TemplateCatalog.hpp>

#include <filesystem>
#include <string>

namespace Hamun::Project {

struct CreateProjectRequest {
    const TemplateDescriptor* projectTemplate = nullptr;
    std::string projectName;
    std::filesystem::path destinationRoot;
    bool overwriteExisting = false;
};

struct CreateProjectResult {
    bool success = false;
    std::filesystem::path projectDirectory;
    std::string error;
};

CreateProjectResult CreateProject(
    const CreateProjectRequest& request);

} // namespace Hamun::Project
