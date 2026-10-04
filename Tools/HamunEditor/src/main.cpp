#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

#include <Hamun/Project/ProjectCreator.hpp>
#include <Hamun/Project/ProjectFile.hpp>
#include <Hamun/Project/TemplateCatalog.hpp>
#include <Hamun/RHI/RHI.hpp>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr int IdOpenProject = 2001;
constexpr int IdExit = 2002;
constexpr UINT_PTR IdViewportTimer = 3001;

HWND g_projectTitle = nullptr;
HWND g_outlinerHeader = nullptr;
HWND g_outliner = nullptr;
HWND g_viewportHeader = nullptr;
HWND g_viewport = nullptr;
HWND g_inspectorHeader = nullptr;
HWND g_inspector = nullptr;
HWND g_assetsHeader = nullptr;
HWND g_assets = nullptr;
HWND g_status = nullptr;

std::optional<Hamun::Project::ProjectDescriptor>
    g_project;

std::unique_ptr<Hamun::RHI::IBackend>
    g_viewportBackend;

bool g_viewportRenderFailed = false;

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
        std::filesystem::path(path)
            .parent_path();
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
        static_cast<std::size_t>(size),
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

    LocalFree(argv);
    return found;
}

std::filesystem::path ProjectArgument()
{
    int argc = 0;

    LPWSTR* argv =
        CommandLineToArgvW(
            GetCommandLineW(),
            &argc);

    if (!argv)
        return {};

    std::filesystem::path result;

    for (int i = 1;
         i < argc;
         ++i) {
        const std::wstring argument =
            argv[i];

        if (argument.rfind(
                L"--",
                0) == 0) {
            continue;
        }

        result =
            std::filesystem::path(
                argument);
        break;
    }

    LocalFree(argv);
    return result;
}

bool RunViewportBackendSmoke(
    Hamun::RHI::BackendType type,
    HWND viewport)
{
    auto backend =
        Hamun::RHI::CreateBackend(
            type);

    if (!backend)
        return false;

    Hamun::RHI::BackendCreateInfo
        createInfo;

    createInfo.nativeWindowHandle =
        viewport;
    createInfo.width =
        320;
    createInfo.height =
        180;
    createInfo.enableValidation =
        false;

    if (!backend->Initialize(
            createInfo)) {
        backend->Shutdown();
        return false;
    }

    bool success = true;

    for (int frame = 0;
         frame < 3;
         ++frame) {
        auto* commands =
            backend->BeginFrame();

        if (!commands) {
            success = false;
            break;
        }

        commands->BeginRenderPass(
            {
                0.02f,
                0.03f,
                0.05f,
                1.0f
            });

        commands->EndRenderPass();

        if (!backend->SubmitFrame()) {
            success = false;
            break;
        }
    }

    backend->Shutdown();
    return success;
}

int RunViewportSmokeMode(
    HINSTANCE instance)
{
    const wchar_t* className =
        L"HamunEditorViewportSmokeHost";

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc =
        DefWindowProcW;
    windowClass.hInstance =
        instance;
    windowClass.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW);
    windowClass.lpszClassName =
        className;

    if (!RegisterClassW(
            &windowClass)) {
        return 40;
    }

    HWND parent =
        CreateWindowExW(
            0,
            className,
            L"Hamun viewport smoke parent",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            640,
            480,
            nullptr,
            nullptr,
            instance,
            nullptr);

    if (!parent) {
        UnregisterClassW(
            className,
            instance);
        return 41;
    }

    HWND viewport =
        CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD |
                WS_VISIBLE,
            0,
            0,
            320,
            180,
            parent,
            nullptr,
            instance,
            nullptr);

    if (!viewport) {
        DestroyWindow(
            parent);

        UnregisterClassW(
            className,
            instance);

        return 42;
    }

    const bool dx12 =
        RunViewportBackendSmoke(
            Hamun::RHI::BackendType::D3D12,
            viewport);

    const bool dx11 =
        RunViewportBackendSmoke(
            Hamun::RHI::BackendType::D3D11,
            viewport);

    DestroyWindow(
        parent);

    UnregisterClassW(
        className,
        instance);

    if (!dx12)
        return 43;

    if (!dx11)
        return 44;

    return 0;
}

int RunEditorSmokeMode()
{
    Hamun::Project::TemplateCatalog catalog;
    std::string error;

    const auto templatesRoot =
        ExecutableDirectory() /
        "Templates";

    if (!catalog.LoadDirectory(
            templatesRoot,
            &error)) {
        return 30;
    }

    const auto* blank =
        catalog.Find("blank");

    if (!blank)
        return 31;

    const auto smokeRoot =
        std::filesystem::temp_directory_path() /
        "HamunEditorSmoke";

    Hamun::Project::CreateProjectRequest
        request;

    request.projectTemplate =
        blank;
    request.projectName =
        "HamunEditorSmokeProject";
    request.destinationRoot =
        smokeRoot;
    request.overwriteExisting =
        true;

    const auto created =
        Hamun::Project::CreateProject(
            request);

    if (!created.success)
        return 32;

    const auto manifest =
        created.projectDirectory /
        "Project.hamunproject";

    auto project =
        Hamun::Project::LoadProjectFile(
            manifest,
            &error);

    const bool valid =
        project &&
        project->name ==
            "HamunEditorSmokeProject" &&
        project->templateId ==
            "blank" &&
        project->formatVersion ==
            1 &&
        project->rootDirectory ==
            std::filesystem::absolute(
                created.projectDirectory);

    std::error_code ec;
    std::filesystem::remove_all(
        smokeRoot,
        ec);

    return valid
        ? 0
        : 33;
}

HWND AddControl(
    HWND parent,
    const wchar_t* className,
    const wchar_t* text,
    DWORD style,
    int id = 0)
{
    return CreateWindowExW(
        0,
        className,
        text,
        WS_CHILD |
            WS_VISIBLE |
            style,
        0,
        0,
        10,
        10,
        parent,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(
                id)),
        GetModuleHandleW(nullptr),
        nullptr);
}

void SetStatus(
    const std::wstring& value)
{
    if (g_status) {
        SetWindowTextW(
            g_status,
            value.c_str());
    }
}

void ShutdownViewportBackend()
{
    if (g_viewportBackend) {
        g_viewportBackend->Shutdown();
        g_viewportBackend.reset();
    }
}

bool InitializeViewportBackend()
{
    if (!g_viewport)
        return false;

    RECT rect{};
    GetClientRect(
        g_viewport,
        &rect);

    const auto width =
        static_cast<std::uint32_t>(
            std::max(
                1L,
                rect.right -
                    rect.left));

    const auto height =
        static_cast<std::uint32_t>(
            std::max(
                1L,
                rect.bottom -
                    rect.top));

    constexpr Hamun::RHI::BackendType
        candidates[] = {
            Hamun::RHI::BackendType::D3D12,
            Hamun::RHI::BackendType::D3D11
        };

    ShutdownViewportBackend();

    for (const auto type :
         candidates) {
        auto backend =
            Hamun::RHI::CreateBackend(
                type);

        if (!backend)
            continue;

        Hamun::RHI::BackendCreateInfo
            createInfo;

        createInfo.nativeWindowHandle =
            g_viewport;
        createInfo.width =
            width;
        createInfo.height =
            height;
        createInfo.enableValidation =
            false;

        if (!backend->Initialize(
                createInfo)) {
            backend->Shutdown();
            continue;
        }

        std::wstring label =
            L"Viewport - ";

        label +=
            Utf8ToWide(
                std::string(
                    backend->Name()));

        const auto& adapter =
            backend->Adapter();

        if (!adapter.name.empty()) {
            label +=
                L" - ";

            label +=
                Utf8ToWide(
                    adapter.name);
        }

        SetWindowTextW(
            g_viewportHeader,
            label.c_str());

        g_viewportBackend =
            std::move(backend);

        g_viewportRenderFailed =
            false;

        SetStatus(
            L"Live editor viewport initialized.");

        return true;
    }

    g_viewportRenderFailed =
        true;

    SetWindowTextW(
        g_viewportHeader,
        L"Viewport - unavailable");

    SetStatus(
        L"Could not initialize DX12 or DX11 for the editor viewport.");

    return false;
}

void RenderViewportFrame()
{
    if (!g_viewportBackend ||
        g_viewportRenderFailed) {
        return;
    }

    auto* commands =
        g_viewportBackend
            ->BeginFrame();

    if (!commands) {
        g_viewportRenderFailed =
            true;
        SetStatus(
            L"Editor viewport BeginFrame failed.");
        return;
    }

    commands->BeginRenderPass(
        {
            0.035f,
            0.050f,
            0.075f,
            1.0f
        });

    commands->EndRenderPass();

    if (!g_viewportBackend
            ->SubmitFrame()) {
        g_viewportRenderFailed =
            true;

        SetStatus(
            L"Editor viewport frame submission failed.");
    }
}

LRESULT CALLBACK ViewportProc(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc =
                BeginPaint(
                    window,
                    &paint);

            if (!g_viewportBackend &&
                dc) {
                RECT rect{};
                GetClientRect(
                    window,
                    &rect);

                FillRect(
                    dc,
                    &rect,
                    static_cast<HBRUSH>(
                        GetStockObject(
                            BLACK_BRUSH)));
            }

            EndPaint(
                window,
                &paint);

            return 0;
        }
    }

    return DefWindowProcW(
        window,
        message,
        wParam,
        lParam);
}

void AddListItem(
    HWND list,
    const std::wstring& text)
{
    SendMessageW(
        list,
        LB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            text.c_str()));
}

void PopulateOutliner()
{
    SendMessageW(
        g_outliner,
        LB_RESETCONTENT,
        0,
        0);

    AddListItem(
        g_outliner,
        L"World");

    AddListItem(
        g_outliner,
        L"  Camera (editor placeholder)");

    AddListItem(
        g_outliner,
        L"  Directional Light (editor placeholder)");
}

void PopulateAssets(
    const std::filesystem::path& root)
{
    SendMessageW(
        g_assets,
        LB_RESETCONTENT,
        0,
        0);

    std::vector<std::wstring>
        entries;

    std::error_code ec;

    for (const auto& entry :
         std::filesystem::directory_iterator(
             root,
             ec)) {
        if (ec)
            break;

        std::wstring label =
            entry.path()
                .filename()
                .wstring();

        if (entry.is_directory())
            label += L"\\";

        entries.push_back(
            std::move(label));
    }

    std::sort(
        entries.begin(),
        entries.end());

    for (const auto& entry :
         entries) {
        AddListItem(
            g_assets,
            entry);
    }

    if (entries.empty()) {
        AddListItem(
            g_assets,
            L"(project folder is empty)");
    }
}

bool LoadProjectIntoEditor(
    HWND owner,
    const std::filesystem::path& projectFile)
{
    std::string error;

    auto project =
        Hamun::Project::LoadProjectFile(
            projectFile,
            &error);

    if (!project) {
        const std::wstring message =
            Utf8ToWide(error);

        SetStatus(message);

        MessageBoxW(
            owner,
            message.c_str(),
            L"HamunEditor - Could not open project",
            MB_OK |
                MB_ICONERROR);

        return false;
    }

    g_project =
        std::move(*project);

    const std::wstring name =
        Utf8ToWide(
            g_project->name);

    SetWindowTextW(
        g_projectTitle,
        (L"Project: " + name)
            .c_str());

    const std::wstring inspectorText =
        L"Project\r\n\r\nName: " +
        name +
        L"\r\nTemplate: " +
        Utf8ToWide(
            g_project->templateId) +
        L"\r\nEngine: " +
        Utf8ToWide(
            g_project->engineName) +
        L"\r\nFormat: " +
        std::to_wstring(
            g_project->formatVersion) +
        L"\r\n\r\nRoot:\r\n" +
        g_project->rootDirectory
            .wstring();

    SetWindowTextW(
        g_inspector,
        inspectorText.c_str());

    PopulateOutliner();
    PopulateAssets(
        g_project->rootDirectory);

    const std::wstring windowTitle =
        L"BDFR Hamun Engine - HamunEditor - " +
        name;

    SetWindowTextW(
        owner,
        windowTitle.c_str());

    SetStatus(
        L"Project loaded: " +
        g_project->projectFile
            .wstring());

    return true;
}

void OpenProjectDialog(
    HWND owner)
{
    wchar_t path[MAX_PATH]{};

    const wchar_t filter[] =
        L"Hamun Project (*.hamunproject)\0"
        L"*.hamunproject\0"
        L"All Files (*.*)\0"
        L"*.*\0\0";

    OPENFILENAMEW dialog{};
    dialog.lStructSize =
        sizeof(dialog);
    dialog.hwndOwner =
        owner;
    dialog.lpstrFilter =
        filter;
    dialog.lpstrFile =
        path;
    dialog.nMaxFile =
        MAX_PATH;
    dialog.Flags =
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST |
        OFN_EXPLORER;
    dialog.lpstrDefExt =
        L"hamunproject";

    if (GetOpenFileNameW(
            &dialog)) {
        LoadProjectIntoEditor(
            owner,
            path);
    }
}

void LayoutControls(
    HWND window)
{
    RECT rect{};
    GetClientRect(
        window,
        &rect);

    const int width =
        rect.right -
        rect.left;

    const int height =
        rect.bottom -
        rect.top;

    const int margin = 12;
    const int titleHeight = 30;
    const int headerHeight = 22;
    const int statusHeight = 26;
    const int assetHeight =
        std::max(
            130,
            height / 4);
    const int sideWidth =
        std::max(
            200,
            width / 5);

    const int contentTop =
        margin +
        titleHeight +
        margin;

    const int assetsTop =
        height -
        statusHeight -
        margin -
        assetHeight;

    const int mainHeight =
        std::max(
            100,
            assetsTop -
            contentTop -
            margin);

    const int viewportX =
        margin +
        sideWidth +
        margin;

    const int viewportWidth =
        std::max(
            100,
            width -
            (margin * 4) -
            (sideWidth * 2));

    const int inspectorX =
        viewportX +
        viewportWidth +
        margin;

    MoveWindow(
        g_projectTitle,
        margin,
        margin,
        width -
            (margin * 2),
        titleHeight,
        TRUE);

    MoveWindow(
        g_outlinerHeader,
        margin,
        contentTop,
        sideWidth,
        headerHeight,
        TRUE);

    MoveWindow(
        g_outliner,
        margin,
        contentTop +
            headerHeight,
        sideWidth,
        mainHeight -
            headerHeight,
        TRUE);

    MoveWindow(
        g_viewportHeader,
        viewportX,
        contentTop,
        viewportWidth,
        headerHeight,
        TRUE);

    MoveWindow(
        g_viewport,
        viewportX,
        contentTop +
            headerHeight,
        viewportWidth,
        mainHeight -
            headerHeight,
        TRUE);

    MoveWindow(
        g_inspectorHeader,
        inspectorX,
        contentTop,
        sideWidth,
        headerHeight,
        TRUE);

    MoveWindow(
        g_inspector,
        inspectorX,
        contentTop +
            headerHeight,
        sideWidth,
        mainHeight -
            headerHeight,
        TRUE);

    MoveWindow(
        g_assetsHeader,
        margin,
        assetsTop,
        width -
            (margin * 2),
        headerHeight,
        TRUE);

    MoveWindow(
        g_assets,
        margin,
        assetsTop +
            headerHeight,
        width -
            (margin * 2),
        assetHeight -
            headerHeight,
        TRUE);

    MoveWindow(
        g_status,
        0,
        height -
            statusHeight,
        width,
        statusHeight,
        TRUE);
}

void CreateMainMenu(
    HWND window)
{
    HMENU menu =
        CreateMenu();

    HMENU fileMenu =
        CreatePopupMenu();

    AppendMenuW(
        fileMenu,
        MF_STRING,
        IdOpenProject,
        L"&Open Project...\tCtrl+O");

    AppendMenuW(
        fileMenu,
        MF_SEPARATOR,
        0,
        nullptr);

    AppendMenuW(
        fileMenu,
        MF_STRING,
        IdExit,
        L"E&xit");

    AppendMenuW(
        menu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(
            fileMenu),
        L"&File");

    SetMenu(
        window,
        menu);
}

LRESULT CALLBACK WindowProc(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message) {
        case WM_CREATE: {
            CreateMainMenu(
                window);

            HFONT font =
                static_cast<HFONT>(
                    GetStockObject(
                        DEFAULT_GUI_FONT));

            g_projectTitle =
                AddControl(
                    window,
                    L"STATIC",
                    L"No project loaded",
                    SS_LEFT);

            g_outlinerHeader =
                AddControl(
                    window,
                    L"STATIC",
                    L"Outliner",
                    SS_LEFT);

            g_outliner =
                AddControl(
                    window,
                    L"LISTBOX",
                    L"",
                    WS_BORDER |
                        WS_VSCROLL |
                        LBS_NOTIFY);

            g_viewportHeader =
                AddControl(
                    window,
                    L"STATIC",
                    L"Viewport",
                    SS_LEFT);

            g_viewport =
                AddControl(
                    window,
                    L"HamunEditorViewportHost",
                    L"",
                    WS_BORDER |
                        WS_CLIPSIBLINGS);

            g_inspectorHeader =
                AddControl(
                    window,
                    L"STATIC",
                    L"Inspector",
                    SS_LEFT);

            g_inspector =
                AddControl(
                    window,
                    L"STATIC",
                    L"No selection.",
                    WS_BORDER |
                        SS_LEFT);

            g_assetsHeader =
                AddControl(
                    window,
                    L"STATIC",
                    L"Asset Browser",
                    SS_LEFT);

            g_assets =
                AddControl(
                    window,
                    L"LISTBOX",
                    L"",
                    WS_BORDER |
                        WS_HSCROLL |
                        WS_VSCROLL);

            g_status =
                AddControl(
                    window,
                    L"STATIC",
                    L"Ready. Use File > Open Project to load a .hamunproject file.",
                    SS_SUNKEN |
                        SS_LEFT);

            for (HWND control :
                 {
                     g_projectTitle,
                     g_outlinerHeader,
                     g_outliner,
                     g_viewportHeader,
                     g_viewport,
                     g_inspectorHeader,
                     g_inspector,
                     g_assetsHeader,
                     g_assets,
                     g_status
                 }) {
                SendMessageW(
                    control,
                    WM_SETFONT,
                    reinterpret_cast<WPARAM>(
                        font),
                    TRUE);
            }

            LayoutControls(
                window);

            return 0;
        }

        case WM_SIZE:
            LayoutControls(
                window);
            return 0;

        case WM_TIMER:
            if (wParam ==
                IdViewportTimer) {
                RenderViewportFrame();
                return 0;
            }
            break;

        case WM_GETMINMAXINFO: {
            auto* info =
                reinterpret_cast<MINMAXINFO*>(
                    lParam);

            info->ptMinTrackSize.x =
                900;
            info->ptMinTrackSize.y =
                600;

            return 0;
        }

        case WM_COMMAND: {
            const int id =
                LOWORD(wParam);

            if (id ==
                IdOpenProject) {
                OpenProjectDialog(
                    window);
                return 0;
            }

            if (id ==
                IdExit) {
                DestroyWindow(
                    window);
                return 0;
            }

            break;
        }

        case WM_DESTROY:
            KillTimer(
                window,
                IdViewportTimer);

            ShutdownViewportBackend();

            PostQuitMessage(0);
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
            L"--viewport-smoke-test")) {
        return
            RunViewportSmokeMode(
                instance);
    }

    if (HasCommandLineFlag(
            L"--editor-smoke-test")) {
        return
            RunEditorSmokeMode();
    }

    const auto startupProject =
        ProjectArgument();

    const wchar_t* viewportClassName =
        L"HamunEditorViewportHost";

    WNDCLASSW viewportClass{};
    viewportClass.lpfnWndProc =
        ViewportProc;
    viewportClass.hInstance =
        instance;
    viewportClass.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW);
    viewportClass.hbrBackground =
        static_cast<HBRUSH>(
            GetStockObject(
                BLACK_BRUSH));
    viewportClass.lpszClassName =
        viewportClassName;

    if (!RegisterClassW(
            &viewportClass)) {
        return 2;
    }

    const wchar_t* className =
        L"HamunEditorWindow";

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc =
        WindowProc;
    windowClass.hInstance =
        instance;
    windowClass.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW);
    windowClass.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_WINDOW + 1);
    windowClass.lpszClassName =
        className;

    if (!RegisterClassW(
            &windowClass)) {
        return 3;
    }

    HWND window =
        CreateWindowExW(
            0,
            className,
            L"BDFR Hamun Engine - HamunEditor",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1440,
            900,
            nullptr,
            nullptr,
            instance,
            nullptr);

    if (!window)
        return 4;

    ShowWindow(
        window,
        showCommand);

    UpdateWindow(
        window);

    InitializeViewportBackend();

    SetTimer(
        window,
        IdViewportTimer,
        16,
        nullptr);

    if (!startupProject.empty()) {
        LoadProjectIntoEditor(
            window,
            startupProject);
    }

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

    return
        static_cast<int>(
            message.wParam);
}
