#include <Hamun/Project/TemplateCatalog.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>

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

bool ParseBool(
    const std::string& value)
{
    std::string normalized =
        value;

    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::tolower(c));
        });

    return
        normalized == "1" ||
        normalized == "true" ||
        normalized == "yes" ||
        normalized == "on";
}

void SetError(
    std::string* error,
    const std::string& message)
{
    if (error)
        *error = message;
}

} // namespace

std::optional<TemplateDescriptor> LoadTemplateManifest(
    const std::filesystem::path& manifestPath,
    std::string* error)
{
    std::ifstream file(
        manifestPath);

    if (!file) {
        SetError(
            error,
            "Could not open template manifest: " +
                manifestPath.string());
        return std::nullopt;
    }

    std::map<std::string, std::string>
        values;

    std::string line;

    while (std::getline(
        file,
        line)) {
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

    if (values["id"].empty() ||
        values["name"].empty()) {
        SetError(
            error,
            "Template manifest requires id and name: " +
                manifestPath.string());
        return std::nullopt;
    }

    TemplateDescriptor descriptor;
    descriptor.id =
        values["id"];
    descriptor.displayName =
        values["name"];
    descriptor.description =
        values["description"];
    descriptor.category =
        values["category"].empty()
            ? "General"
            : values["category"];

    descriptor.templateRoot =
        manifestPath.parent_path();

    const std::string content =
        values["content"].empty()
            ? "Content"
            : values["content"];

    descriptor.contentRoot =
        descriptor.templateRoot /
        content;

    descriptor.hidden =
        ParseBool(
            values["hidden"]);

    if (!std::filesystem::exists(
            descriptor.contentRoot)) {
        SetError(
            error,
            "Template content directory does not exist: " +
                descriptor.contentRoot.string());
        return std::nullopt;
    }

    return descriptor;
}

bool TemplateCatalog::LoadDirectory(
    const std::filesystem::path& root,
    std::string* error)
{
    Clear();

    if (!std::filesystem::exists(root)) {
        SetError(
            error,
            "Template directory does not exist: " +
                root.string());
        return false;
    }

    std::error_code ec;

    for (const auto& entry :
         std::filesystem::directory_iterator(
             root,
             ec)) {
        if (ec)
            break;

        if (!entry.is_directory())
            continue;

        const auto manifest =
            entry.path() /
            "template.hamun";

        if (!std::filesystem::exists(
                manifest)) {
            continue;
        }

        std::string manifestError;

        auto descriptor =
            LoadTemplateManifest(
                manifest,
                &manifestError);

        if (!descriptor) {
            SetError(
                error,
                manifestError);
            return false;
        }

        if (!descriptor->hidden) {
            templates_.push_back(
                std::move(*descriptor));
        }
    }

    std::sort(
        templates_.begin(),
        templates_.end(),
        [](const TemplateDescriptor& a,
           const TemplateDescriptor& b) {
            if (a.category != b.category)
                return a.category < b.category;

            return a.displayName <
                b.displayName;
        });

    return true;
}

void TemplateCatalog::Clear() noexcept
{
    templates_.clear();
}

const TemplateDescriptor* TemplateCatalog::Find(
    const std::string& id) const noexcept
{
    const auto it =
        std::find_if(
            templates_.begin(),
            templates_.end(),
            [&id](
                const TemplateDescriptor& item) {
                return item.id == id;
            });

    return
        it == templates_.end()
            ? nullptr
            : &*it;
}

} // namespace Hamun::Project
