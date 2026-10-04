#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>

#include <Hamun/Project/ProjectCreator.hpp>
#include <Hamun/Project/TemplateCatalog.hpp>

#include <filesystem>
#include <memory>
#include <string>

namespace {

constexpr int IdTemplateList = 1001;
constexpr int IdProjectName = 1002;
constexpr int IdLocation = 1003;
constexpr int IdBrowse = 1004;
constexpr int IdCreate = 1005;
constexpr int IdDescription = 1006;
constexpr int IdStatus = 1007;

Hamun::Project::TemplateCatalog g_catalog;
HWND g_templateList = nullptr;
HWND g_description = nullptr;
HWND g_projectName = nullptr;
HWND g_location = nullptr;
HWND g_status = nullptr;

std::filesystem::path ExecutableDirectory();

int RunTemplateSmokeMode()
{
    Hamun::Project::TemplateCatalog catalog;
    std::string error;

    const auto templatesRoot =
        ExecutableDirectory() /
        "Templates";

    if (!catalog.LoadDirectory(
            templatesRoot,
            &error)) {
        return 20;
    }

    if (catalog.Templates().empty())
        return 21;

    const auto smokeRoot =
        std::filesystem::temp_directory_path() /
        "HamunTemplateSmoke";

    Hamun::Project::CreateProjectRequest
        request;

    request.projectTemplate =
        &catalog.Templates().front();

    request.projectName =
        "HamunTemplateSmokeProject";

    request.destinationRoot =
        smokeRoot;

    request.overwriteExisting =
        true;

    const auto result =
        Hamun::Project::CreateProject(
            request);

    if (!result.success)
        return 22;

    const bool manifestExists =
        std::filesystem::exists(
            result.projectDirectory /
            "Project.hamunproject");

    std::error_code ec;
    std::filesystem::remove_all(
        smokeRoot,
        ec);

    return manifestExists
        ? 0
        : 23;
}

bool HasCommandLineFlag(
    const wchar_t* flag)
{
    int argc = 0;

    LPWSTR* argv =
        CommandLineToArgvW(
            GetCommandLineW(),
            &argc);

    if (!argv)
        return false;

    bool found = false;

    for (int i = 1;
         i < argc;
         ++i) {
        if (std::wstring(argv[i]) ==
            flag) {
            found = true;
            break;
        }
    }

    LocalFree(
        argv);

    return found;
}

std::wstring Utf8ToWide(
    const std::string& value)
{
    if (value.empty())
        return {};

    const int size =
        MultiByteToWideChar(
            CP_UTF8,
            0,
            value.c_str(),
            -1,
            nullptr,
            0);

    if (size <= 1)
        return {};

    std::wstring result(
        static_cast<std::size_t>(
            size),
        L'\0');

    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.c_str(),
        -1,
        result.data(),
        size);

    if (!result.empty() &&
        result.back() == L'\0') {
        result.pop_back();
    }

    return result;
}

std::string WideToUtf8(
    const std::wstring& value)
{
    if (value.empty())
        return {};

    const int size =
        WideCharToMultiByte(
            CP_UTF8,
            0,
            value.c_str(),
            -1,
            nullptr,
            0,
            nullptr,
            nullptr);

    if (size <= 1)
        return {};

    std::string result(
        static_cast<std::size_t>(
            size),
        '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        -1,
        result.data(),
        size,
        nullptr,
        nullptr);

    if (!result.empty() &&
        result.back() == '\0') {
        result.pop_back();
    }

    return result;
}

std::wstring GetText(
    HWND control)
{
    const int length =
        GetWindowTextLengthW(
            control);

    std::wstring value(
        static_cast<std::size_t>(
            length + 1),
        L'\0');

    GetWindowTextW(
        control,
        value.data(),
        length + 1);

    value.resize(
        static_cast<std::size_t>(
            length));

    return value;
}

std::filesystem::path ExecutableDirectory()
{
    wchar_t path[MAX_PATH]{};

    const DWORD length =
        GetModuleFileNameW(
            nullptr,
            path,
            MAX_PATH);

    if (length == 0)
        return {};

    return
        std::filesystem::path(
            path)
            .parent_path();
}

void SetStatus(
    const std::wstring& text)
{
    if (g_status) {
        SetWindowTextW(
            g_status,
            text.c_str());
    }
}

void RefreshTemplateDetails()
{
    const LRESULT selection =
        SendMessageW(
            g_templateList,
            LB_GETCURSEL,
            0,
            0);

    if (selection ==
        LB_ERR) {
        SetWindowTextW(
            g_description,
            L"Select a template to see its details.");
        return;
    }

    const auto& templates =
        g_catalog.Templates();

    const std::size_t index =
        static_cast<std::size_t>(
            selection);

    if (index >= templates.size())
        return;

    const auto& item =
        templates[index];

    std::wstring details =
        Utf8ToWide(
            item.displayName);

    details +=
        L"\r\nCategory: ";

    details +=
        Utf8ToWide(
            item.category);

    details +=
        L"\r\n\r\n";

    details +=
        Utf8ToWide(
            item.description);

    SetWindowTextW(
        g_description,
        details.c_str());
}

bool BrowseFolder(
    HWND owner)
{
    IFileDialog* dialog =
        nullptr;

    if (FAILED(
            CoCreateInstance(
                CLSID_FileOpenDialog,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(
                    &dialog)))) {
        return false;
    }

    DWORD options = 0;
    dialog->GetOptions(
        &options);

    dialog->SetOptions(
        options |
        FOS_PICKFOLDERS |
        FOS_FORCEFILESYSTEM);

    const HRESULT shown =
        dialog->Show(
            owner);

    if (FAILED(shown)) {
        dialog->Release();
        return false;
    }

    IShellItem* item =
        nullptr;

    if (FAILED(
            dialog->GetResult(
                &item))) {
        dialog->Release();
        return false;
    }

    PWSTR path =
        nullptr;

    const HRESULT pathResult =
        item->GetDisplayName(
            SIGDN_FILESYSPATH,
            &path);

    if (SUCCEEDED(pathResult) &&
        path) {
        SetWindowTextW(
            g_location,
            path);

        CoTaskMemFree(
            path);
    }

    item->Release();
    dialog->Release();

    return SUCCEEDED(pathResult);
}

void CreateSelectedProject()
{
    const LRESULT selection =
        SendMessageW(
            g_templateList,
            LB_GETCURSEL,
            0,
            0);

    if (selection ==
        LB_ERR) {
        SetStatus(
            L"Select a template first.");
        return;
    }

    const auto& templates =
        g_catalog.Templates();

    const std::size_t index =
        static_cast<std::size_t>(
            selection);

    if (index >= templates.size())
        return;

    Hamun::Project::CreateProjectRequest
        request;

    request.projectTemplate =
        &templates[index];

    request.projectName =
        WideToUtf8(
            GetText(
                g_projectName));

    request.destinationRoot =
        GetText(
            g_location);

    const auto result =
        Hamun::Project::CreateProject(
            request);

    if (!result.success) {
        SetStatus(
            Utf8ToWide(
                result.error));
        return;
    }

    const auto manifest =
        result.projectDirectory /
        "Project.hamunproject";

    const auto editor =
        ExecutableDirectory() /
        "HamunEditor.exe";

    if (std::filesystem::exists(editor) &&
        std::filesystem::exists(manifest)) {
        std::wstring parameters =
            L"\"" +
            manifest.wstring() +
            L"\"";

        const auto launchResult =
            reinterpret_cast<INT_PTR>(
                ShellExecuteW(
                    nullptr,
                    L"open",
                    editor.wstring().c_str(),
                    parameters.c_str(),
                    nullptr,
                    SW_SHOWNORMAL));

        if (launchResult > 32) {
            SetStatus(
                L"Project created and opened in HamunEditor.");
            return;
        }
    }

    SetStatus(
        L"Project created successfully. HamunEditor was not available, so the project folder was opened.");

    ShellExecuteW(
        nullptr,
        L"open",
        result.projectDirectory
            .wstring()
            .c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL);
}

void PopulateTemplates()
{
    SendMessageW(
        g_templateList,
        LB_RESETCONTENT,
        0,
        0);

    const auto templatesRoot =
        ExecutableDirectory() /
        "Templates";

    std::string error;

    if (!g_catalog.LoadDirectory(
            templatesRoot,
            &error)) {
        SetStatus(
            Utf8ToWide(
                error));
        return;
    }

    for (const auto& item :
         g_catalog.Templates()) {
        const std::wstring label =
            Utf8ToWide(
                item.displayName);

        SendMessageW(
            g_templateList,
            LB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(
                label.c_str()));
    }

    if (!g_catalog.Templates().empty()) {
        SendMessageW(
            g_templateList,
            LB_SETCURSEL,
            0,
            0);

        RefreshTemplateDetails();
    } else {
        SetStatus(
            L"No templates are installed yet.");
    }
}

HWND AddControl(
    HWND parent,
    const wchar_t* className,
    const wchar_t* text,
    DWORD style,
    int x,
    int y,
    int width,
    int height,
    int id = 0)
{
    return CreateWindowExW(
        0,
        className,
        text,
        WS_CHILD |
            WS_VISIBLE |
            style,
        x,
        y,
        width,
        height,
        parent,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(
                id)),
        GetModuleHandleW(
            nullptr),
        nullptr);
}

LRESULT CALLBACK WindowProc(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message) {
        case WM_CREATE: {
            HFONT font =
                static_cast<HFONT>(
                    GetStockObject(
                        DEFAULT_GUI_FONT));

            AddControl(
                window,
                L"STATIC",
                L"BDFR Hamun Engine",
                0,
                28,
                20,
                420,
                32);

            AddControl(
                window,
                L"STATIC",
                L"New Project",
                0,
                28,
                58,
                420,
                24);

            g_templateList =
                AddControl(
                    window,
                    L"LISTBOX",
                    L"",
                    WS_BORDER |
                        LBS_NOTIFY |
                        WS_VSCROLL,
                    28,
                    100,
                    300,
                    420,
                    IdTemplateList);

            g_description =
                AddControl(
                    window,
                    L"STATIC",
                    L"Select a template.",
                    WS_BORDER |
                        SS_LEFT,
                    350,
                    100,
                    580,
                    190,
                    IdDescription);

            AddControl(
                window,
                L"STATIC",
                L"Project name",
                0,
                350,
                320,
                130,
                22);

            g_projectName =
                AddControl(
                    window,
                    L"EDIT",
                    L"MyHamunProject",
                    WS_BORDER |
                        ES_AUTOHSCROLL,
                    350,
                    345,
                    580,
                    30,
                    IdProjectName);

            AddControl(
                window,
                L"STATIC",
                L"Location",
                0,
                350,
                395,
                130,
                22);

            g_location =
                AddControl(
                    window,
                    L"EDIT",
                    L"",
                    WS_BORDER |
                        ES_AUTOHSCROLL,
                    350,
                    420,
                    470,
                    30,
                    IdLocation);

            AddControl(
                window,
                L"BUTTON",
                L"Browse...",
                BS_PUSHBUTTON,
                830,
                420,
                100,
                30,
                IdBrowse);

            AddControl(
                window,
                L"BUTTON",
                L"Create Project",
                BS_DEFPUSHBUTTON,
                350,
                480,
                180,
                38,
                IdCreate);

            g_status =
                AddControl(
                    window,
                    L"STATIC",
                    L"Ready.",
                    0,
                    350,
                    540,
                    580,
                    50,
                    IdStatus);

            for (HWND control :
                 {
                     g_templateList,
                     g_description,
                     g_projectName,
                     g_location,
                     g_status
                 }) {
                SendMessageW(
                    control,
                    WM_SETFONT,
                    reinterpret_cast<WPARAM>(
                        font),
                    TRUE);
            }

            const auto defaultLocation =
                std::filesystem::current_path();

            SetWindowTextW(
                g_location,
                defaultLocation
                    .wstring()
                    .c_str());

            PopulateTemplates();
            return 0;
        }

        case WM_COMMAND: {
            const int id =
                LOWORD(wParam);

            const int notification =
                HIWORD(wParam);

            if (id == IdTemplateList &&
                notification ==
                    LBN_SELCHANGE) {
                RefreshTemplateDetails();
                return 0;
            }

            if (id == IdBrowse) {
                BrowseFolder(
                    window);
                return 0;
            }

            if (id == IdCreate) {
                CreateSelectedProject();
                return 0;
            }

            break;
        }

        case WM_DESTROY:
            PostQuitMessage(
                0);
            return 0;
    }

    return DefWindowProcW(
        window,
        message,
        wParam,
        lParam);
}

} // namespace

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int showCommand)
{
    if (HasCommandLineFlag(
            L"--template-smoke-test")) {
        return
            RunTemplateSmokeMode();
    }

    if (FAILED(
            CoInitializeEx(
                nullptr,
                COINIT_APARTMENTTHREADED))) {
        return 1;
    }

    const wchar_t* className =
        L"HamunLauncherWindow";

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc =
        WindowProc;
    windowClass.hInstance =
        instance;
    windowClass.hCursor =
        LoadCursor(
            nullptr,
            IDC_ARROW);
    windowClass.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_WINDOW + 1);
    windowClass.lpszClassName =
        className;

    if (!RegisterClassW(
            &windowClass)) {
        CoUninitialize();
        return 2;
    }

    HWND window =
        CreateWindowExW(
            0,
            className,
            L"BDFR Hamun Engine - Project Launcher",
            WS_OVERLAPPED |
                WS_CAPTION |
                WS_SYSMENU |
                WS_MINIMIZEBOX,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            980,
            660,
            nullptr,
            nullptr,
            instance,
            nullptr);

    if (!window) {
        CoUninitialize();
        return 3;
    }

    ShowWindow(
        window,
        showCommand);

    UpdateWindow(
        window);

    MSG message{};

    while (GetMessageW(
               &message,
               nullptr,
               0,
               0) > 0) {
        TranslateMessage(
            &message);

        DispatchMessageW(
            &message);
    }

    CoUninitialize();

    return
        static_cast<int>(
            message.wParam);
}
