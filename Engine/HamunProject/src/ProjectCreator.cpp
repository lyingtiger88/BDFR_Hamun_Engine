#include <Hamun/Project/ProjectCreator.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <system_error>

namespace Hamun::Project {
namespace {

bool IsValidProjectName(
    const std::string& name)
{
    if (name.empty())
        return false;

    return std::all_of(
        name.begin(),
        name.end(),
        [](unsigned char c) {
            return
                std::isalnum(c) ||
                c == '_' ||
                c == '-' ||
                c == ' ';
        });
}

bool ShouldSubstitute(
    const std::filesystem::path& path)
{
    const std::string ext =
        path.extension().string();

    return
        ext == ".txt" ||
        ext == ".md" ||
        ext == ".cpp" ||
        ext == ".c" ||
        ext == ".hpp" ||
        ext == ".h" ||
        ext == ".cmake" ||
        ext == ".json" ||
        ext == ".ini" ||
        ext == ".bat" ||
        ext == ".sh" ||
        ext == ".hamunproject" ||
        path.filename() == "CMakeLists.txt";
}

void ReplaceAll(
    std::string& text,
    const std::string& from,
    const std::string& to)
{
    if (from.empty())
        return;

    std::size_t position = 0;

    while ((position =
                text.find(
                    from,
                    position)) !=
           std::string::npos) {
        text.replace(
            position,
            from.size(),
            to);

        position +=
            to.size();
    }
}

bool CopyTemplateFile(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    const std::string& projectName,
    std::string& error)
{
    std::error_code ec;

    std::filesystem::create_directories(
        destination.parent_path(),
        ec);

    if (ec) {
        error =
            "Could not create destination directory: " +
            ec.message();
        return false;
    }

    if (!ShouldSubstitute(source)) {
        std::filesystem::copy_file(
            source,
            destination,
            std::filesystem::copy_options::overwrite_existing,
            ec);

        if (ec) {
            error =
                "Could not copy template file: " +
                ec.message();
            return false;
        }

        return true;
    }

    std::ifstream input(
        source,
        std::ios::binary);

    if (!input) {
        error =
            "Could not read template file: " +
            source.string();
        return false;
    }

    std::ostringstream buffer;
    buffer <<
        input.rdbuf();

    std::string text =
        buffer.str();

    ReplaceAll(
        text,
        "{{PROJECT_NAME}}",
        projectName);

    std::ofstream output(
        destination,
        std::ios::binary |
        std::ios::trunc);

    if (!output) {
        error =
            "Could not create project file: " +
            destination.string();
        return false;
    }

    output <<
        text;

    return true;
}

} // namespace

CreateProjectResult CreateProject(
    const CreateProjectRequest& request)
{
    CreateProjectResult result;

    if (!request.projectTemplate) {
        result.error =
            "No project template is selected.";
        return result;
    }

    if (!IsValidProjectName(
            request.projectName)) {
        result.error =
            "Project name may contain letters, numbers, spaces, '-' and '_' only.";
        return result;
    }

    if (request.destinationRoot.empty()) {
        result.error =
            "Project destination is empty.";
        return result;
    }

    if (!std::filesystem::exists(
            request.projectTemplate
                ->contentRoot)) {
        result.error =
            "Selected template content is missing.";
        return result;
    }

    result.projectDirectory =
        request.destinationRoot /
        request.projectName;

    std::error_code ec;

    if (std::filesystem::exists(
            result.projectDirectory)) {
        if (!request.overwriteExisting) {
            result.error =
                "Project directory already exists.";
            return result;
        }

        std::filesystem::remove_all(
            result.projectDirectory,
            ec);

        if (ec) {
            result.error =
                "Could not replace existing project directory: " +
                ec.message();
            return result;
        }
    }

    std::filesystem::create_directories(
        result.projectDirectory,
        ec);

    if (ec) {
        result.error =
            "Could not create project directory: " +
            ec.message();
        return result;
    }

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(
             request.projectTemplate
                 ->contentRoot,
             ec)) {
        if (ec) {
            result.error =
                "Could not enumerate template content: " +
                ec.message();
            return result;
        }

        const auto relative =
            std::filesystem::relative(
                entry.path(),
                request.projectTemplate
                    ->contentRoot,
                ec);

        if (ec) {
            result.error =
                "Could not resolve template path: " +
                ec.message();
            return result;
        }

        const auto destination =
            result.projectDirectory /
            relative;

        if (entry.is_directory()) {
            std::filesystem::create_directories(
                destination,
                ec);

            if (ec) {
                result.error =
                    "Could not create project directory tree: " +
                    ec.message();
                return result;
            }

            continue;
        }

        if (!entry.is_regular_file())
            continue;

        if (!CopyTemplateFile(
                entry.path(),
                destination,
                request.projectName,
                result.error)) {
            return result;
        }
    }

    result.success = true;
    return result;
}

} // namespace Hamun::Project
