#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Hamun::Project {

struct TemplateDescriptor {
    std::string id;
    std::string displayName;
    std::string description;
    std::string category;
    std::filesystem::path templateRoot;
    std::filesystem::path contentRoot;
    bool hidden = false;
};

class TemplateCatalog {
public:
    bool LoadDirectory(
        const std::filesystem::path& root,
        std::string* error = nullptr);

    void Clear() noexcept;

    [[nodiscard]] const std::vector<TemplateDescriptor>& Templates() const noexcept
    {
        return templates_;
    }

    [[nodiscard]] const TemplateDescriptor* Find(
        const std::string& id) const noexcept;

private:
    std::vector<TemplateDescriptor> templates_;
};

std::optional<TemplateDescriptor> LoadTemplateManifest(
    const std::filesystem::path& manifestPath,
    std::string* error = nullptr);

} // namespace Hamun::Project
