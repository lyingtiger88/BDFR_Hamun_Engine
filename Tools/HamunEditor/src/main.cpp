#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>

#include <Hamun/Project/ProjectCreator.hpp>
#include <Hamun/Project/ProjectFile.hpp>
#include <Hamun/Project/TemplateCatalog.hpp>
#include <Hamun/RHI/RHI.hpp>

#include "EditorAssetIndex.hpp"
#include "EditorSceneDocument.hpp"
#include "EditorScenePreview.hpp"

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int IdOpenProject = 2001;
constexpr int IdExit = 2002;
constexpr int IdOpenScene = 2003;
constexpr int IdSaveScene = 2004;
constexpr int IdSaveSceneAs = 2005;
constexpr int IdImportAsset = 2006;
constexpr int IdRendererAuto = 2101;
constexpr int IdRendererD3D12 = 2102;
constexpr int IdRendererD3D11 = 2103;
constexpr int IdOutliner = 2201;
constexpr int IdApplyTransform = 2202;
constexpr int IdAssets = 2301;
constexpr UINT_PTR IdViewportTimer = 3001;

HWND g_projectTitle = nullptr;
HWND g_outlinerHeader = nullptr;
HWND g_outliner = nullptr;
HWND g_viewportHeader = nullptr;
HWND g_viewport = nullptr;
HWND g_inspectorHeader = nullptr;
HWND g_inspector = nullptr;
HWND g_positionLabel = nullptr;
HWND g_positionX = nullptr;
HWND g_positionY = nullptr;
HWND g_positionZ = nullptr;
HWND g_scaleLabel = nullptr;
HWND g_scaleX = nullptr;
HWND g_scaleY = nullptr;
HWND g_scaleZ = nullptr;
HWND g_applyTransform = nullptr;
HWND g_assetsHeader = nullptr;
HWND g_assets = nullptr;
HWND g_status = nullptr;

std::optional<Hamun::Project::ProjectDescriptor>
    g_project;

struct ViewportRenderResources {
    std::unique_ptr<Hamun::RHI::IShader>
        vertexShader;

    std::unique_ptr<Hamun::RHI::IShader>
        pixelShader;

    std::unique_ptr<Hamun::RHI::IPipeline>
        pipeline;

    bool Create(
        Hamun::RHI::IBackend& backend)
    {
        static const std::string
            shaderSource = R"(
struct ViewportVertex
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

ViewportVertex VSMain(
    uint vertexId : SV_VertexID)
{
    const float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };

    ViewportVertex output;
    const float2 position =
        positions[vertexId];

    output.position =
        float4(
            position,
            0.0f,
            1.0f);

    output.uv =
        float2(
            position.x * 0.5f + 0.5f,
            0.5f - position.y * 0.5f);

    return output;
}

float GridLine(
    float coordinate,
    float scale,
    float thickness)
{
    const float wrapped =
        frac(
            coordinate *
            scale);

    const float edgeDistance =
        min(
            wrapped,
            1.0f -
                wrapped);

    return
        1.0f -
        step(
            thickness,
            edgeDistance);
}

float4 PSMain(
    ViewportVertex input)
    : SV_TARGET
{
    const float minorGrid =
        max(
            GridLine(
                input.uv.x,
                32.0f,
                0.035f),
            GridLine(
                input.uv.y,
                18.0f,
                0.035f));

    const float majorGrid =
        max(
            GridLine(
                input.uv.x,
                8.0f,
                0.025f),
            GridLine(
                input.uv.y,
                4.5f,
                0.025f));

    float3 color =
        float3(
            0.030f,
            0.038f,
            0.052f);

    color =
        lerp(
            color,
            float3(
                0.075f,
                0.088f,
                0.110f),
            minorGrid *
                0.65f);

    color =
        lerp(
            color,
            float3(
                0.125f,
                0.145f,
                0.175f),
            majorGrid *
                0.85f);

    const float horizontalAxis =
        1.0f -
        step(
            0.0035f,
            abs(
                input.uv.y -
                0.5f));

    const float verticalAxis =
        1.0f -
        step(
            0.0035f,
            abs(
                input.uv.x -
                0.5f));

    color =
        lerp(
            color,
            float3(
                0.48f,
                0.18f,
                0.16f),
            horizontalAxis);

    color =
        lerp(
            color,
            float3(
                0.16f,
                0.46f,
                0.28f),
            verticalAxis);

    return
        float4(
            color,
            1.0f);
}
)";

        Hamun::RHI::ShaderDesc
            vertexDesc;

        vertexDesc.stage =
            Hamun::RHI::ShaderStage::Vertex;

        vertexDesc.source =
            shaderSource;

        vertexDesc.entryPoint =
            "VSMain";

        Hamun::RHI::ShaderDesc
            pixelDesc;

        pixelDesc.stage =
            Hamun::RHI::ShaderStage::Pixel;

        pixelDesc.source =
            shaderSource;

        pixelDesc.entryPoint =
            "PSMain";

        vertexShader =
            backend.CreateShader(
                vertexDesc);

        pixelShader =
            backend.CreateShader(
                pixelDesc);

        if (!vertexShader ||
            !pixelShader) {
            Reset();
            return false;
        }

        Hamun::RHI::GraphicsPipelineDesc
            pipelineDesc;

        pipelineDesc.vertexShader =
            vertexShader.get();

        pipelineDesc.pixelShader =
            pixelShader.get();

        pipelineDesc.renderTargetFormat =
            Hamun::RHI::TextureFormat::
                RGBA8_UNorm;

        pipelineDesc.depthTest =
            false;

        pipeline =
            backend.CreateGraphicsPipeline(
                pipelineDesc);

        if (!pipeline) {
            Reset();
            return false;
        }

        return true;
    }

    void Reset()
    {
        pipeline.reset();
        pixelShader.reset();
        vertexShader.reset();
    }

    [[nodiscard]] bool Ready()
        const noexcept
    {
        return
            pipeline !=
            nullptr;
    }
};

std::unique_ptr<Hamun::RHI::IBackend>
    g_viewportBackend;

ViewportRenderResources
    g_viewportResources;

Hamun::Editor::ScenePreview
    g_scenePreview;

std::vector<Hamun::Editor::SceneObjectTransform>
    g_sceneTransforms;

std::filesystem::path
    g_sceneTransformSource;

std::filesystem::path
    g_activeScenePath;

std::filesystem::path
    g_sceneDocumentPath;

std::vector<Hamun::Editor::IndexedAsset>
    g_assetEntries;

bool g_viewportMouseLook = false;
POINT g_viewportLastMouse{};
ULONGLONG g_lastCameraTick = 0;

enum class ViewportBackendPreference {
    Auto,
    D3D12,
    D3D11
};

ViewportBackendPreference
    g_viewportBackendPreference =
        ViewportBackendPreference::Auto;

std::wstring
    g_viewportBackendLabel;

double g_viewportFps = 0.0;
double g_viewportFrameTimeMs = 0.0;
std::uint64_t g_viewportFpsFrameCount = 0;
ULONGLONG g_viewportFpsWindowStart = 0;

bool g_viewportRenderFailed = false;
bool g_viewportResizePending = false;
ULONGLONG g_viewportResizeRequestedAt = 0;

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

std::filesystem::path EditorPreviewScenePath()
{
    return
        ExecutableDirectory() /
        "Assets" /
        "TestScene.gltf";
}

std::filesystem::path CurrentSceneAssetPath()
{
    return
        g_activeScenePath.empty()
            ? EditorPreviewScenePath()
            : g_activeScenePath;
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

    Hamun::Editor::ScenePreview
        preview;

    std::string previewError;

    if (!preview.Initialize(
            *backend,
            EditorPreviewScenePath(),
            320,
            180,
            &previewError)) {
        backend->Shutdown();
        return false;
    }

    if (preview.InstanceCount() != 5) {
        preview.Reset();
        backend->Shutdown();
        return false;
    }

    const auto object =
        preview.ObjectInfo(1);

    if (!object) {
        preview.Reset();
        backend->Shutdown();
        return false;
    }

    auto editedTransform =
        object->transform;

    editedTransform.position[0] +=
        0.25f;

    if (!preview.SetTransform(
            1,
            editedTransform)) {
        preview.Reset();
        backend->Shutdown();
        return false;
    }

    const auto editedObject =
        preview.ObjectInfo(1);

    if (!editedObject ||
        editedObject
            ->transform
            .position[0] !=
            editedTransform
                .position[0]) {
        preview.Reset();
        backend->Shutdown();
        return false;
    }

    bool success = true;

    for (int frame = 0;
         frame < 3;
         ++frame) {
        if (!preview.RenderFrame(
                *backend)) {
            success = false;
            break;
        }
    }

    preview.Reset();
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

    const bool projectValid =
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

    if (!projectValid) {
        std::error_code ec;
        std::filesystem::remove_all(
            smokeRoot,
            ec);
        return 33;
    }

    const auto starterScene =
        created.projectDirectory /
        "Scenes" /
        "Main.hamunscene";

    Hamun::Editor::SceneDocument
        sceneDocument;

    if (!Hamun::Editor::LoadSceneDocument(
            starterScene,
            sceneDocument,
            &error)) {
        std::error_code ec;
        std::filesystem::remove_all(
            smokeRoot,
            ec);
        return 34;
    }

    if (!std::filesystem::exists(
            sceneDocument.sourceAsset)) {
        std::error_code ec;
        std::filesystem::remove_all(
            smokeRoot,
            ec);
        return 35;
    }

    Hamun::Editor::SceneObjectTransform
        smokeTransform;

    smokeTransform.position = {
        1.0f,
        2.0f,
        3.0f
    };

    smokeTransform.scale = {
        1.25f,
        0.75f,
        2.0f
    };

    sceneDocument.transforms = {
        smokeTransform
    };

    const auto savedScene =
        created.projectDirectory /
        "Scenes" /
        "SmokeSaved.hamunscene";

    if (!Hamun::Editor::SaveSceneDocument(
            savedScene,
            sceneDocument,
            &error)) {
        std::error_code ec;
        std::filesystem::remove_all(
            smokeRoot,
            ec);
        return 36;
    }

    Hamun::Editor::SceneDocument
        reloadedScene;

    if (!Hamun::Editor::LoadSceneDocument(
            savedScene,
            reloadedScene,
            &error) ||
        reloadedScene.transforms.size() !=
            1 ||
        reloadedScene.transforms[0]
                .position[1] !=
            2.0f) {
        std::error_code ec;
        std::filesystem::remove_all(
            smokeRoot,
            ec);
        return 37;
    }

    const auto assetsBefore =
        Hamun::Editor::IndexProjectAssets(
            created.projectDirectory);

    if (assetsBefore.empty()) {
        std::error_code ec;
        std::filesystem::remove_all(
            smokeRoot,
            ec);
        return 38;
    }

    std::filesystem::path
        importedAsset;

    if (!Hamun::Editor::ImportAssetWithCompanions(
            sceneDocument.sourceAsset,
            created.projectDirectory,
            importedAsset,
            &error) ||
        !std::filesystem::exists(
            importedAsset)) {
        std::error_code ec;
        std::filesystem::remove_all(
            smokeRoot,
            ec);
        return 39;
    }

    const auto assetsAfter =
        Hamun::Editor::IndexProjectAssets(
            created.projectDirectory);

    const bool toolsValid =
        assetsAfter.size() >
            assetsBefore.size();

    std::error_code ec;
    std::filesystem::remove_all(
        smokeRoot,
        ec);

    return toolsValid
        ? 0
        : 45;
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

void PopulateOutliner();
void UpdateInspectorFromSelection();
void PopulateAssets(
    const std::filesystem::path& root);
bool InitializeViewportBackend();

void SetFloatEdit(
    HWND edit,
    float value)
{
    if (!edit)
        return;

    std::wostringstream text;

    text
        << std::fixed
        << std::setprecision(3)
        << value;

    SetWindowTextW(
        edit,
        text.str().c_str());
}

bool ReadFloatEdit(
    HWND edit,
    float& value)
{
    if (!edit)
        return false;

    wchar_t buffer[64]{};

    GetWindowTextW(
        edit,
        buffer,
        64);

    wchar_t* end = nullptr;

    const float parsed =
        std::wcstof(
            buffer,
            &end);

    if (end == buffer)
        return false;

    value = parsed;
    return true;
}

void SetTransformEditorEnabled(
    bool enabled)
{
    for (HWND control :
         {
             g_positionX,
             g_positionY,
             g_positionZ,
             g_scaleX,
             g_scaleY,
             g_scaleZ,
             g_applyTransform
         }) {
        if (control) {
            EnableWindow(
                control,
                enabled ? TRUE : FALSE);
        }
    }
}

void SyncSceneTransformState()
{
    const auto scenePath =
        g_scenePreview.ScenePath();

    if (g_sceneTransformSource !=
            scenePath ||
        g_sceneTransforms.size() !=
            g_scenePreview.InstanceCount()) {
        g_sceneTransformSource =
            scenePath;

        g_sceneTransforms.clear();
        g_sceneTransforms.reserve(
            g_scenePreview.InstanceCount());

        for (std::size_t i = 0;
             i <
                g_scenePreview.InstanceCount();
             ++i) {
            const auto info =
                g_scenePreview.ObjectInfo(i);

            if (info) {
                g_sceneTransforms.push_back(
                    info->transform);
            }
        }

        return;
    }

    for (std::size_t i = 0;
         i < g_sceneTransforms.size();
         ++i) {
        g_scenePreview.SetTransform(
            i,
            g_sceneTransforms[i]);
    }
}

void UpdateViewportHeader()
{
    if (!g_viewportHeader)
        return;

    std::wostringstream text;

    text
        << g_viewportBackendLabel;

    if (g_viewportFps > 0.0) {
        text
            << L" | FPS: "
            << std::fixed
            << std::setprecision(1)
            << g_viewportFps
            << L" | Frame: "
            << std::setprecision(2)
            << g_viewportFrameTimeMs
            << L" ms";
    } else {
        text
            << L" | FPS: -- | Frame: -- ms";
    }

    SetWindowTextW(
        g_viewportHeader,
        text.str().c_str());
}

void ResetViewportPerformanceStats()
{
    g_viewportFps =
        0.0;

    g_viewportFrameTimeMs =
        0.0;

    g_viewportFpsFrameCount =
        0;

    g_viewportFpsWindowStart =
        GetTickCount64();

    UpdateViewportHeader();
}

void RecordViewportFrame()
{
    ++g_viewportFpsFrameCount;

    const ULONGLONG now =
        GetTickCount64();

    const ULONGLONG elapsed =
        now -
        g_viewportFpsWindowStart;

    constexpr ULONGLONG
        FpsSampleMilliseconds =
            500;

    if (elapsed <
        FpsSampleMilliseconds) {
        return;
    }

    if (elapsed > 0) {
        g_viewportFps =
            static_cast<double>(
                g_viewportFpsFrameCount) *
            1000.0 /
            static_cast<double>(
                elapsed);

        g_viewportFrameTimeMs =
            g_viewportFps > 0.0
                ? 1000.0 /
                    g_viewportFps
                : 0.0;
    }

    g_viewportFpsFrameCount =
        0;

    g_viewportFpsWindowStart =
        now;

    UpdateViewportHeader();
}

void ShutdownViewportBackend()
{
    g_scenePreview.Reset();
    g_viewportResources.Reset();

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

    std::vector<Hamun::RHI::BackendType>
        candidates;

    switch (g_viewportBackendPreference) {
        case ViewportBackendPreference::D3D12:
            candidates.push_back(
                Hamun::RHI::BackendType::D3D12);
            break;

        case ViewportBackendPreference::D3D11:
            candidates.push_back(
                Hamun::RHI::BackendType::D3D11);
            break;

        case ViewportBackendPreference::Auto:
        default:
            candidates.push_back(
                Hamun::RHI::BackendType::D3D12);

            candidates.push_back(
                Hamun::RHI::BackendType::D3D11);
            break;
    }

    ShutdownViewportBackend();

    g_viewportResizePending =
        false;

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

        std::string previewError;

        if (!g_scenePreview.Initialize(
                *backend,
                CurrentSceneAssetPath(),
                width,
                height,
                &previewError)) {
            g_scenePreview.Reset();
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

        label +=
            L" (" +
            std::to_wstring(
                width) +
            L"x" +
            std::to_wstring(
                height) +
            L")";

        label +=
            L" | Scene: " +
            g_scenePreview
                .ScenePath()
                .filename()
                .wstring() +
            L" | Objects: " +
            std::to_wstring(
                g_scenePreview
                    .InstanceCount());

        g_viewportBackendLabel =
            std::move(label);

        g_viewportBackend =
            std::move(backend);

        SyncSceneTransformState();

        g_viewportRenderFailed =
            false;

        ResetViewportPerformanceStats();

        PopulateOutliner();
        UpdateInspectorFromSelection();

        SetStatus(
            L"HamunRenderer scene viewport initialized.");

        return true;
    }

    g_viewportRenderFailed =
        true;

    g_viewportBackendLabel =
        L"Viewport - unavailable";

    ResetViewportPerformanceStats();

    SetStatus(
        L"Could not initialize DX12 or DX11 for the editor viewport.");

    return false;
}

void RequestViewportResize()
{
    if (!g_viewportBackend ||
        !g_viewport)
        return;

    auto* swapChain =
        g_viewportBackend
            ->SwapChain();

    if (!swapChain)
        return;

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

    if (swapChain->Width() ==
            width &&
        swapChain->Height() ==
            height) {
        return;
    }

    g_viewportResizePending =
        true;

    g_viewportResizeRequestedAt =
        GetTickCount64();
}

void ProcessViewportResize()
{
    if (!g_viewportResizePending)
        return;

    const ULONGLONG now =
        GetTickCount64();

    constexpr ULONGLONG
        ResizeDebounceMilliseconds =
            150;

    if (now <
        g_viewportResizeRequestedAt +
            ResizeDebounceMilliseconds) {
        return;
    }

    g_viewportResizePending =
        false;

    InitializeViewportBackend();
}

void RenderViewportFrame()
{
    if (!g_viewportBackend ||
        g_viewportRenderFailed) {
        return;
    }

    if (!g_scenePreview.Ready() ||
        !g_scenePreview.RenderFrame(
            *g_viewportBackend)) {
        g_viewportRenderFailed =
            true;

        SetStatus(
            L"HamunRenderer scene viewport frame failed.");

        return;
    }

    RecordViewportFrame();
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

        case WM_LBUTTONDOWN:
            SetFocus(window);
            return 0;

        case WM_RBUTTONDOWN:
            SetFocus(window);

            g_viewportMouseLook =
                true;

            g_viewportLastMouse.x =
                GET_X_LPARAM(lParam);

            g_viewportLastMouse.y =
                GET_Y_LPARAM(lParam);

            SetCapture(window);
            return 0;

        case WM_RBUTTONUP:
            g_viewportMouseLook =
                false;

            if (GetCapture() ==
                window) {
                ReleaseCapture();
            }

            return 0;

        case WM_CAPTURECHANGED:
            g_viewportMouseLook =
                false;
            return 0;

        case WM_MOUSEMOVE:
            if (g_viewportMouseLook &&
                GetCapture() ==
                    window) {
                POINT current{
                    GET_X_LPARAM(lParam),
                    GET_Y_LPARAM(lParam)
                };

                const float deltaX =
                    static_cast<float>(
                        current.x -
                        g_viewportLastMouse.x);

                const float deltaY =
                    static_cast<float>(
                        current.y -
                        g_viewportLastMouse.y);

                g_viewportLastMouse =
                    current;

                g_scenePreview.RotateCamera(
                    deltaX * 0.0025f,
                    -deltaY * 0.0025f);
            }

            return 0;

        case WM_MOUSEWHEEL: {
            const short wheel =
                GET_WHEEL_DELTA_WPARAM(
                    wParam);

            g_scenePreview.MoveCamera(
                wheel > 0
                    ? 0.75f
                    : -0.75f,
                0.0f,
                0.0f);

            return 0;
        }

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
    if (!g_outliner)
        return;

    const LRESULT previousSelection =
        SendMessageW(
            g_outliner,
            LB_GETCURSEL,
            0,
            0);

    SendMessageW(
        g_outliner,
        LB_RESETCONTENT,
        0,
        0);

    if (!g_scenePreview.Ready()) {
        AddListItem(
            g_outliner,
            L"(scene unavailable)");

        SetTransformEditorEnabled(false);
        return;
    }

    for (std::size_t i = 0;
         i <
            g_scenePreview.InstanceCount();
         ++i) {
        const auto info =
            g_scenePreview.ObjectInfo(i);

        if (!info)
            continue;

        std::wstring label(
            static_cast<std::size_t>(
                info->hierarchyDepth) *
                2,
            L' ');

        label +=
            Utf8ToWide(
                info->name);

        if (!info->meshName.empty()) {
            label +=
                L"  [" +
                Utf8ToWide(
                    info->meshName) +
                L"]";
        }

        AddListItem(
            g_outliner,
            label);
    }

    if (g_scenePreview.InstanceCount() > 0) {
        const std::size_t selected =
            previousSelection != LB_ERR &&
            static_cast<std::size_t>(
                previousSelection) <
                g_scenePreview.InstanceCount()
                ? static_cast<std::size_t>(
                    previousSelection)
                : 0;

        SendMessageW(
            g_outliner,
            LB_SETCURSEL,
            static_cast<WPARAM>(
                selected),
            0);
    }
}

void UpdateInspectorFromSelection()
{
    if (!g_inspector ||
        !g_outliner ||
        !g_scenePreview.Ready()) {
        if (g_inspector) {
            SetWindowTextW(
                g_inspector,
                L"No scene object selected.");
        }

        SetTransformEditorEnabled(false);
        return;
    }

    const LRESULT selected =
        SendMessageW(
            g_outliner,
            LB_GETCURSEL,
            0,
            0);

    if (selected == LB_ERR) {
        SetWindowTextW(
            g_inspector,
            L"No scene object selected.");

        SetTransformEditorEnabled(false);
        return;
    }

    const auto info =
        g_scenePreview.ObjectInfo(
            static_cast<std::size_t>(
                selected));

    if (!info) {
        SetWindowTextW(
            g_inspector,
            L"Selected scene object is unavailable.");

        SetTransformEditorEnabled(false);
        return;
    }

    std::wostringstream text;

    text
        << L"Scene Object\r\n\r\n"
        << L"Name: "
        << Utf8ToWide(
            info->name)
        << L"\r\nMesh: "
        << Utf8ToWide(
            info->meshName)
        << L"\r\nIndex: "
        << info->index
        << L"\r\n\r\nMaterial\r\nMetallic: "
        << std::fixed
        << std::setprecision(3)
        << info->metallic
        << L"\r\nRoughness: "
        << info->roughness
        << L"\r\nBase Color: "
        << info->baseColorFactor[0]
        << L", "
        << info->baseColorFactor[1]
        << L", "
        << info->baseColorFactor[2];

    SetWindowTextW(
        g_inspector,
        text.str().c_str());

    SetFloatEdit(
        g_positionX,
        info->transform.position[0]);

    SetFloatEdit(
        g_positionY,
        info->transform.position[1]);

    SetFloatEdit(
        g_positionZ,
        info->transform.position[2]);

    SetFloatEdit(
        g_scaleX,
        info->transform.scale[0]);

    SetFloatEdit(
        g_scaleY,
        info->transform.scale[1]);

    SetFloatEdit(
        g_scaleZ,
        info->transform.scale[2]);

    SetTransformEditorEnabled(true);
}

void ApplyInspectorTransform()
{
    const LRESULT selected =
        SendMessageW(
            g_outliner,
            LB_GETCURSEL,
            0,
            0);

    if (selected == LB_ERR)
        return;

    Hamun::Editor::SceneObjectTransform
        transform;

    if (!ReadFloatEdit(
            g_positionX,
            transform.position[0]) ||
        !ReadFloatEdit(
            g_positionY,
            transform.position[1]) ||
        !ReadFloatEdit(
            g_positionZ,
            transform.position[2]) ||
        !ReadFloatEdit(
            g_scaleX,
            transform.scale[0]) ||
        !ReadFloatEdit(
            g_scaleY,
            transform.scale[1]) ||
        !ReadFloatEdit(
            g_scaleZ,
            transform.scale[2])) {
        SetStatus(
            L"Transform contains an invalid number.");
        return;
    }

    if (transform.scale[0] <= 0.0f ||
        transform.scale[1] <= 0.0f ||
        transform.scale[2] <= 0.0f) {
        SetStatus(
            L"Scale values must be greater than zero.");
        return;
    }

    const std::size_t index =
        static_cast<std::size_t>(
            selected);

    if (!g_scenePreview.SetTransform(
            index,
            transform)) {
        SetStatus(
            L"Could not update scene object transform.");
        return;
    }

    if (g_sceneTransforms.size() ==
        g_scenePreview.InstanceCount()) {
        g_sceneTransforms[index] =
            transform;
    }

    UpdateInspectorFromSelection();

    const auto info =
        g_scenePreview.ObjectInfo(
            index);

    SetStatus(
        info
            ? L"Transform updated: " +
                Utf8ToWide(
                    info->name)
            : L"Transform updated.");
}

void PopulateAssets(
    const std::filesystem::path& root)
{
    if (!g_assets)
        return;

    SendMessageW(
        g_assets,
        LB_RESETCONTENT,
        0,
        0);

    g_assetEntries =
        Hamun::Editor::IndexProjectAssets(
            root);

    for (const auto& asset :
         g_assetEntries) {
        const std::wstring label =
            L"[" +
            Hamun::Editor::AssetKindLabel(
                asset.kind) +
            L"] " +
            asset.relativePath
                .wstring();

        AddListItem(
            g_assets,
            label);
    }

    if (g_assetEntries.empty()) {
        AddListItem(
            g_assets,
            L"(no project assets indexed)");
    }

    if (g_assetsHeader) {
        SetWindowTextW(
            g_assetsHeader,
            (L"Asset Browser - " +
             std::to_wstring(
                 g_assetEntries.size()) +
             L" files")
                .c_str());
    }
}

bool LoadSceneAsset(
    const std::filesystem::path& scenePath,
    bool preserveTransforms = false)
{
    std::error_code ec;

    const auto absolute =
        std::filesystem::weakly_canonical(
            scenePath,
            ec);

    const auto resolved =
        ec
            ? std::filesystem::absolute(
                scenePath)
            : absolute;

    if (!std::filesystem::exists(
            resolved)) {
        SetStatus(
            L"Scene asset does not exist: " +
            resolved.wstring());

        return false;
    }

    g_activeScenePath =
        resolved;

    if (!preserveTransforms) {
        g_sceneTransforms.clear();
        g_sceneTransformSource.clear();
    }

    if (!InitializeViewportBackend()) {
        return false;
    }

    SetStatus(
        L"Scene asset loaded: " +
        resolved.wstring());

    return true;
}

bool OpenSceneDocumentFile(
    const std::filesystem::path& path)
{
    Hamun::Editor::SceneDocument
        document;

    std::string error;

    if (!Hamun::Editor::LoadSceneDocument(
            path,
            document,
            &error)) {
        SetStatus(
            Utf8ToWide(error));

        return false;
    }

    g_activeScenePath =
        document.sourceAsset;

    g_sceneTransforms =
        document.transforms;

    g_sceneTransformSource =
        document.sourceAsset;

    g_sceneDocumentPath =
        path;

    if (!InitializeViewportBackend()) {
        return false;
    }

    SetStatus(
        L"Scene opened: " +
        path.wstring());

    return true;
}

Hamun::Editor::SceneDocument
BuildCurrentSceneDocument()
{
    Hamun::Editor::SceneDocument
        document;

    document.sourceAsset =
        CurrentSceneAssetPath();

    if (g_sceneTransforms.size() ==
        g_scenePreview.InstanceCount()) {
        document.transforms =
            g_sceneTransforms;
    } else {
        document.transforms.reserve(
            g_scenePreview.InstanceCount());

        for (std::size_t i = 0;
             i <
                g_scenePreview.InstanceCount();
             ++i) {
            const auto info =
                g_scenePreview.ObjectInfo(i);

            if (info) {
                document.transforms.push_back(
                    info->transform);
            }
        }
    }

    return document;
}

bool SaveCurrentSceneTo(
    const std::filesystem::path& path)
{
    std::string error;

    if (!Hamun::Editor::SaveSceneDocument(
            path,
            BuildCurrentSceneDocument(),
            &error)) {
        SetStatus(
            Utf8ToWide(error));

        return false;
    }

    g_sceneDocumentPath =
        path;

    if (g_project) {
        PopulateAssets(
            g_project->rootDirectory);
    }

    SetStatus(
        L"Scene saved: " +
        path.wstring());

    return true;
}

bool SaveSceneAsDialog(
    HWND owner)
{
    wchar_t path[MAX_PATH]{};

    const wchar_t filter[] =
        L"Hamun Scene (*.hamunscene)\0"
        L"*.hamunscene\0"
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
        OFN_PATHMUSTEXIST |
        OFN_OVERWRITEPROMPT |
        OFN_EXPLORER;
    dialog.lpstrDefExt =
        L"hamunscene";

    if (!g_sceneDocumentPath.empty()) {
        const std::wstring current =
            g_sceneDocumentPath.wstring();

        wcsncpy_s(
            path,
            current.c_str(),
            _TRUNCATE);
    }

    if (!GetSaveFileNameW(
            &dialog)) {
        return false;
    }

    return SaveCurrentSceneTo(
        path);
}

bool SaveCurrentScene(
    HWND owner)
{
    if (!g_sceneDocumentPath.empty()) {
        return SaveCurrentSceneTo(
            g_sceneDocumentPath);
    }

    if (g_project) {
        return SaveCurrentSceneTo(
            g_project->rootDirectory /
            "Scenes" /
            "Main.hamunscene");
    }

    return SaveSceneAsDialog(
        owner);
}

void OpenSceneDialog(
    HWND owner)
{
    wchar_t path[MAX_PATH]{};

    const wchar_t filter[] =
        L"Hamun Scene or glTF\0"
        L"*.hamunscene;*.gltf;*.glb\0"
        L"Hamun Scene (*.hamunscene)\0"
        L"*.hamunscene\0"
        L"glTF (*.gltf;*.glb)\0"
        L"*.gltf;*.glb\0"
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

    if (!GetOpenFileNameW(
            &dialog)) {
        return;
    }

    const std::filesystem::path selected =
        path;

    if (selected.extension() ==
        L".hamunscene") {
        OpenSceneDocumentFile(
            selected);
    } else {
        g_sceneDocumentPath.clear();

        LoadSceneAsset(
            selected);
    }
}

void ImportAssetDialog(
    HWND owner)
{
    if (!g_project) {
        SetStatus(
            L"Open a project before importing assets.");
        return;
    }

    wchar_t path[MAX_PATH]{};

    const wchar_t filter[] =
        L"Supported Assets\0"
        L"*.gltf;*.glb;*.png;*.jpg;*.jpeg;*.tfx\0"
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

    if (!GetOpenFileNameW(
            &dialog)) {
        return;
    }

    std::filesystem::path importedPath;
    std::string error;

    if (!Hamun::Editor::ImportAssetWithCompanions(
            path,
            g_project->rootDirectory,
            importedPath,
            &error)) {
        SetStatus(
            Utf8ToWide(error));
        return;
    }

    PopulateAssets(
        g_project->rootDirectory);

    if (Hamun::Editor::ClassifyAsset(
            importedPath) ==
        Hamun::Editor::AssetKind::Gltf) {
        g_sceneDocumentPath.clear();

        LoadSceneAsset(
            importedPath);
    } else {
        SetStatus(
            L"Asset imported: " +
            importedPath.wstring());
    }
}

void ActivateSelectedAsset()
{
    const LRESULT selected =
        SendMessageW(
            g_assets,
            LB_GETCURSEL,
            0,
            0);

    if (selected == LB_ERR ||
        static_cast<std::size_t>(
            selected) >=
            g_assetEntries.size()) {
        return;
    }

    const auto& asset =
        g_assetEntries[
            static_cast<std::size_t>(
                selected)];

    switch (asset.kind) {
        case Hamun::Editor::AssetKind::Scene:
            OpenSceneDocumentFile(
                asset.absolutePath);
            break;

        case Hamun::Editor::AssetKind::Gltf:
            g_sceneDocumentPath.clear();

            LoadSceneAsset(
                asset.absolutePath);
            break;

        default:
            SetStatus(
                L"Asset selected: " +
                asset.relativePath
                    .wstring());
            break;
    }
}

void UpdateEditorCameraNavigation()
{
    const ULONGLONG now =
        GetTickCount64();

    if (g_lastCameraTick == 0) {
        g_lastCameraTick = now;
        return;
    }

    const float deltaSeconds =
        std::min(
            static_cast<float>(
                now -
                g_lastCameraTick) /
                1000.0f,
            0.1f);

    g_lastCameraTick = now;

    if (!g_scenePreview.Ready() ||
        (GetFocus() != g_viewport &&
         GetCapture() != g_viewport)) {
        return;
    }

    float speed =
        2.5f *
        deltaSeconds;

    if ((GetAsyncKeyState(
             VK_SHIFT) &
         0x8000) != 0) {
        speed *= 4.0f;
    }

    float forward = 0.0f;
    float right = 0.0f;
    float up = 0.0f;

    if ((GetAsyncKeyState('W') &
         0x8000) != 0) {
        forward += speed;
    }

    if ((GetAsyncKeyState('S') &
         0x8000) != 0) {
        forward -= speed;
    }

    if ((GetAsyncKeyState('D') &
         0x8000) != 0) {
        right += speed;
    }

    if ((GetAsyncKeyState('A') &
         0x8000) != 0) {
        right -= speed;
    }

    if ((GetAsyncKeyState('E') &
         0x8000) != 0) {
        up += speed;
    }

    if ((GetAsyncKeyState('Q') &
         0x8000) != 0) {
        up -= speed;
    }

    if (forward != 0.0f ||
        right != 0.0f ||
        up != 0.0f) {
        g_scenePreview.MoveCamera(
            forward,
            right,
            up);
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

    PopulateAssets(
        g_project->rootDirectory);

    const auto defaultSceneDocument =
        g_project->rootDirectory /
        "Scenes" /
        "Main.hamunscene";

    const auto starterScene =
        g_project->rootDirectory /
        "Assets" /
        "Starter" /
        "TestScene.gltf";

    if (std::filesystem::exists(
            defaultSceneDocument)) {
        OpenSceneDocumentFile(
            defaultSceneDocument);
    } else if (
        std::filesystem::exists(
            starterScene)) {
        g_sceneDocumentPath.clear();
        LoadSceneAsset(
            starterScene);
    } else {
        PopulateOutliner();
        UpdateInspectorFromSelection();
    }

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

    const int inspectorTop =
        contentTop +
        headerHeight;

    const int inspectorBodyHeight =
        mainHeight -
        headerHeight;

    const int inspectorInfoHeight =
        std::max(
            70,
            inspectorBodyHeight -
                150);

    MoveWindow(
        g_inspector,
        inspectorX,
        inspectorTop,
        sideWidth,
        inspectorInfoHeight,
        TRUE);

    const int transformTop =
        inspectorTop +
        inspectorInfoHeight +
        6;

    MoveWindow(
        g_positionLabel,
        inspectorX,
        transformTop,
        sideWidth,
        18,
        TRUE);

    const int fieldGap = 4;

    const int fieldWidth =
        std::max(
            40,
            (sideWidth -
             fieldGap * 2) /
                3);

    const int positionFieldsTop =
        transformTop +
        20;

    MoveWindow(
        g_positionX,
        inspectorX,
        positionFieldsTop,
        fieldWidth,
        22,
        TRUE);

    MoveWindow(
        g_positionY,
        inspectorX +
            fieldWidth +
            fieldGap,
        positionFieldsTop,
        fieldWidth,
        22,
        TRUE);

    MoveWindow(
        g_positionZ,
        inspectorX +
            (fieldWidth +
             fieldGap) *
                2,
        positionFieldsTop,
        fieldWidth,
        22,
        TRUE);

    const int scaleLabelTop =
        positionFieldsTop +
        26;

    MoveWindow(
        g_scaleLabel,
        inspectorX,
        scaleLabelTop,
        sideWidth,
        18,
        TRUE);

    const int scaleFieldsTop =
        scaleLabelTop +
        20;

    MoveWindow(
        g_scaleX,
        inspectorX,
        scaleFieldsTop,
        fieldWidth,
        22,
        TRUE);

    MoveWindow(
        g_scaleY,
        inspectorX +
            fieldWidth +
            fieldGap,
        scaleFieldsTop,
        fieldWidth,
        22,
        TRUE);

    MoveWindow(
        g_scaleZ,
        inspectorX +
            (fieldWidth +
             fieldGap) *
                2,
        scaleFieldsTop,
        fieldWidth,
        22,
        TRUE);

    MoveWindow(
        g_applyTransform,
        inspectorX,
        scaleFieldsTop +
            28,
        sideWidth,
        24,
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
        MF_STRING,
        IdOpenScene,
        L"Open &Scene...");

    AppendMenuW(
        fileMenu,
        MF_STRING,
        IdSaveScene,
        L"&Save Scene\tCtrl+S");

    AppendMenuW(
        fileMenu,
        MF_STRING,
        IdSaveSceneAs,
        L"Save Scene &As...");

    AppendMenuW(
        fileMenu,
        MF_STRING,
        IdImportAsset,
        L"&Import Asset...");

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

    HMENU rendererMenu =
        CreatePopupMenu();

    AppendMenuW(
        rendererMenu,
        MF_STRING |
            MF_CHECKED,
        IdRendererAuto,
        L"&Auto (DX12 -> DX11)");

    AppendMenuW(
        rendererMenu,
        MF_STRING,
        IdRendererD3D12,
        L"DirectX &12");

    AppendMenuW(
        rendererMenu,
        MF_STRING,
        IdRendererD3D11,
        L"DirectX &11");

    AppendMenuW(
        menu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(
            rendererMenu),
        L"&Renderer");

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
                        LBS_NOTIFY,
                    IdOutliner);

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
                    L"EDIT",
                    L"No selection.",
                    WS_BORDER |
                        WS_VSCROLL |
                        ES_MULTILINE |
                        ES_AUTOVSCROLL |
                        ES_READONLY);

            g_positionLabel =
                AddControl(
                    window,
                    L"STATIC",
                    L"Position  X / Y / Z",
                    SS_LEFT);

            g_positionX =
                AddControl(
                    window,
                    L"EDIT",
                    L"0.000",
                    WS_BORDER |
                        ES_AUTOHSCROLL);

            g_positionY =
                AddControl(
                    window,
                    L"EDIT",
                    L"0.000",
                    WS_BORDER |
                        ES_AUTOHSCROLL);

            g_positionZ =
                AddControl(
                    window,
                    L"EDIT",
                    L"0.000",
                    WS_BORDER |
                        ES_AUTOHSCROLL);

            g_scaleLabel =
                AddControl(
                    window,
                    L"STATIC",
                    L"Scale  X / Y / Z",
                    SS_LEFT);

            g_scaleX =
                AddControl(
                    window,
                    L"EDIT",
                    L"1.000",
                    WS_BORDER |
                        ES_AUTOHSCROLL);

            g_scaleY =
                AddControl(
                    window,
                    L"EDIT",
                    L"1.000",
                    WS_BORDER |
                        ES_AUTOHSCROLL);

            g_scaleZ =
                AddControl(
                    window,
                    L"EDIT",
                    L"1.000",
                    WS_BORDER |
                        ES_AUTOHSCROLL);

            g_applyTransform =
                AddControl(
                    window,
                    L"BUTTON",
                    L"Apply Transform",
                    BS_PUSHBUTTON,
                    IdApplyTransform);

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
                        WS_VSCROLL |
                        LBS_NOTIFY,
                    IdAssets);

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
                     g_positionLabel,
                     g_positionX,
                     g_positionY,
                     g_positionZ,
                     g_scaleLabel,
                     g_scaleX,
                     g_scaleY,
                     g_scaleZ,
                     g_applyTransform,
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

            SetTransformEditorEnabled(
                false);

            LayoutControls(
                window);

            return 0;
        }

        case WM_SIZE:
            LayoutControls(
                window);

            RequestViewportResize();
            return 0;

        case WM_TIMER:
            if (wParam ==
                IdViewportTimer) {
                ProcessViewportResize();
                UpdateEditorCameraNavigation();
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
                    IdOutliner &&
                HIWORD(wParam) ==
                    LBN_SELCHANGE) {
                UpdateInspectorFromSelection();
                return 0;
            }

            if (id ==
                    IdAssets &&
                HIWORD(wParam) ==
                    LBN_DBLCLK) {
                ActivateSelectedAsset();
                return 0;
            }

            if (id ==
                    IdApplyTransform &&
                HIWORD(wParam) ==
                    BN_CLICKED) {
                ApplyInspectorTransform();
                return 0;
            }

            if (id ==
                IdOpenProject) {
                OpenProjectDialog(
                    window);
                return 0;
            }

            if (id ==
                IdOpenScene) {
                OpenSceneDialog(
                    window);
                return 0;
            }

            if (id ==
                IdSaveScene) {
                SaveCurrentScene(
                    window);
                return 0;
            }

            if (id ==
                IdSaveSceneAs) {
                SaveSceneAsDialog(
                    window);
                return 0;
            }

            if (id ==
                IdImportAsset) {
                ImportAssetDialog(
                    window);
                return 0;
            }

            if (id ==
                IdRendererAuto ||
                id ==
                    IdRendererD3D12 ||
                id ==
                    IdRendererD3D11) {
                if (id ==
                    IdRendererD3D12) {
                    g_viewportBackendPreference =
                        ViewportBackendPreference::D3D12;
                } else if (id ==
                           IdRendererD3D11) {
                    g_viewportBackendPreference =
                        ViewportBackendPreference::D3D11;
                } else {
                    g_viewportBackendPreference =
                        ViewportBackendPreference::Auto;
                }

                CheckMenuRadioItem(
                    GetMenu(window),
                    IdRendererAuto,
                    IdRendererD3D11,
                    id,
                    MF_BYCOMMAND);

                InitializeViewportBackend();
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
