#include <windows.h>
#include <windowsx.h>

#include <Hamun/Core/Log.hpp>
#include <Hamun/Platform/Window.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Hamun::Platform {
namespace {

constexpr wchar_t kWindowClassName[] =
    L"HamunEngineWindowClass";

constexpr std::size_t KeyCount = 8;
constexpr std::size_t MouseButtonCount = 3;

std::wstring ToWide(const std::string& text)
{
    if (text.empty())
        return L"";

    const int length = MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);

    if (length <= 0)
        return std::wstring(
            text.begin(),
            text.end());

    std::wstring result(
        static_cast<std::size_t>(length),
        L'\0');

    MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        length);

    return result;
}

std::size_t KeyIndex(Key key)
{
    return static_cast<std::size_t>(key);
}

std::size_t MouseButtonIndex(
    MouseButton button)
{
    return static_cast<std::size_t>(button);
}

std::optional<Key> MapVirtualKey(
    WPARAM key)
{
    switch (key) {
        case 'W': return Key::W;
        case 'A': return Key::A;
        case 'S': return Key::S;
        case 'D': return Key::D;
        case 'Q': return Key::Q;
        case 'E': return Key::E;
        case VK_SHIFT: return Key::LeftShift;
        case VK_ESCAPE: return Key::Escape;
        default: return std::nullopt;
    }
}

class Win32Window final : public IWindow {
public:
    explicit Win32Window(
        const WindowDesc& desc)
        : width_(
            std::max<std::uint32_t>(
                desc.width,
                1u))
        , height_(
            std::max<std::uint32_t>(
                desc.height,
                1u))
    {
        static bool registered = false;

        if (!registered) {
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.style =
                CS_HREDRAW |
                CS_VREDRAW;
            wc.lpfnWndProc =
                &Win32Window::WndProc;
            wc.hInstance =
                GetModuleHandleW(nullptr);
            wc.hCursor =
                LoadCursorW(
                    nullptr,
                    IDC_ARROW);
            wc.lpszClassName =
                kWindowClassName;

            if (!RegisterClassExW(&wc) &&
                GetLastError() !=
                    ERROR_CLASS_ALREADY_EXISTS) {
                Core::Log(
                    Core::LogLevel::Error,
                    "Win32: RegisterClassExW failed.");
                return;
            }

            registered = true;
        }

        RECT rect{
            0,
            0,
            static_cast<LONG>(width_),
            static_cast<LONG>(height_)
        };

        AdjustWindowRect(
            &rect,
            WS_OVERLAPPEDWINDOW,
            FALSE);

        const std::wstring title =
            ToWide(desc.title);

        hwnd_ = CreateWindowExW(
            0,
            kWindowClassName,
            title.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            this);

        if (!hwnd_) {
            Core::Log(
                Core::LogLevel::Error,
                "Win32: CreateWindowExW failed.");
            return;
        }

        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
    }

    ~Win32Window() override
    {
        if (hwnd_ &&
            IsWindow(hwnd_)) {
            DestroyWindow(hwnd_);
        }
    }

    [[nodiscard]] bool IsValid() const noexcept
    {
        return hwnd_ != nullptr;
    }

    void* NativeHandle() const noexcept override
    {
        return hwnd_;
    }

    std::uint32_t Width() const noexcept override
    {
        return width_;
    }

    std::uint32_t Height() const noexcept override
    {
        return height_;
    }

    bool IsKeyDown(
        Key key) const noexcept override
    {
        return keys_[KeyIndex(key)];
    }

    bool IsMouseButtonDown(
        MouseButton button) const noexcept override
    {
        return mouseButtons_[
            MouseButtonIndex(button)];
    }

    MouseDelta ConsumeMouseDelta() noexcept override
    {
        const MouseDelta result =
            mouseDelta_;
        mouseDelta_ = {};
        return result;
    }

    bool PumpEvents() override
    {
        MSG message{};

        while (PeekMessageW(
                &message,
                nullptr,
                0,
                0,
                PM_REMOVE)) {
            if (message.message ==
                WM_QUIT) {
                return false;
            }

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        return !quit_;
    }

private:
    void SetMouseButton(
        MouseButton button,
        bool down)
    {
        mouseButtons_[
            MouseButtonIndex(button)] =
            down;
    }

    void ResetInput()
    {
        keys_.fill(false);
        mouseButtons_.fill(false);
        mouseDelta_ = {};
        hasLastMouse_ = false;
    }

    void OnMouseMove(
        int x,
        int y)
    {
        if (hasLastMouse_ &&
            IsMouseButtonDown(
                MouseButton::Right)) {
            mouseDelta_.x +=
                static_cast<float>(
                    x - lastMouseX_);
            mouseDelta_.y +=
                static_cast<float>(
                    y - lastMouseY_);
        }

        lastMouseX_ = x;
        lastMouseY_ = y;
        hasLastMouse_ = true;
    }

    static LRESULT CALLBACK WndProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        Win32Window* self =
            reinterpret_cast<Win32Window*>(
                GetWindowLongPtrW(
                    hwnd,
                    GWLP_USERDATA));

        if (message == WM_NCCREATE) {
            const auto* create =
                reinterpret_cast<
                    const CREATESTRUCTW*>(
                        lParam);

            self =
                static_cast<Win32Window*>(
                    create->lpCreateParams);

            SetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(
                    self));
        }

        switch (message) {
            case WM_CLOSE:
                DestroyWindow(hwnd);
                return 0;

            case WM_DESTROY:
                if (self)
                    self->quit_ = true;

                PostQuitMessage(0);
                return 0;

            case WM_SIZE:
                if (self &&
                    wParam != SIZE_MINIMIZED) {
                    self->width_ =
                        std::max<std::uint32_t>(
                            LOWORD(lParam),
                            1u);

                    self->height_ =
                        std::max<std::uint32_t>(
                            HIWORD(lParam),
                            1u);
                }
                return 0;

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                if (self) {
                    if (const auto key =
                            MapVirtualKey(
                                wParam)) {
                        self->keys_[
                            KeyIndex(*key)] =
                            true;
                    }
                }
                return 0;

            case WM_KEYUP:
            case WM_SYSKEYUP:
                if (self) {
                    if (const auto key =
                            MapVirtualKey(
                                wParam)) {
                        self->keys_[
                            KeyIndex(*key)] =
                            false;
                    }
                }
                return 0;

            case WM_LBUTTONDOWN:
                if (self)
                    self->SetMouseButton(
                        MouseButton::Left,
                        true);
                return 0;

            case WM_LBUTTONUP:
                if (self)
                    self->SetMouseButton(
                        MouseButton::Left,
                        false);
                return 0;

            case WM_RBUTTONDOWN:
                if (self) {
                    self->SetMouseButton(
                        MouseButton::Right,
                        true);
                    self->lastMouseX_ =
                        GET_X_LPARAM(lParam);
                    self->lastMouseY_ =
                        GET_Y_LPARAM(lParam);
                    self->hasLastMouse_ =
                        true;
                    SetCapture(hwnd);
                }
                return 0;

            case WM_RBUTTONUP:
                if (self) {
                    self->SetMouseButton(
                        MouseButton::Right,
                        false);
                    ReleaseCapture();
                }
                return 0;

            case WM_MBUTTONDOWN:
                if (self)
                    self->SetMouseButton(
                        MouseButton::Middle,
                        true);
                return 0;

            case WM_MBUTTONUP:
                if (self)
                    self->SetMouseButton(
                        MouseButton::Middle,
                        false);
                return 0;

            case WM_MOUSEMOVE:
                if (self) {
                    self->OnMouseMove(
                        GET_X_LPARAM(lParam),
                        GET_Y_LPARAM(lParam));
                }
                return 0;

            case WM_KILLFOCUS:
                if (self)
                    self->ResetInput();
                return 0;

            default:
                break;
        }

        return DefWindowProcW(
            hwnd,
            message,
            wParam,
            lParam);
    }

    HWND hwnd_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;

    std::array<bool, KeyCount> keys_{};
    std::array<
        bool,
        MouseButtonCount> mouseButtons_{};

    MouseDelta mouseDelta_{};
    int lastMouseX_ = 0;
    int lastMouseY_ = 0;
    bool hasLastMouse_ = false;
    bool quit_ = false;
};

} // namespace

std::unique_ptr<IWindow> CreateNativeWindow(
    const WindowDesc& desc)
{
    auto window =
        std::make_unique<Win32Window>(
            desc);

    if (!window->IsValid())
        return {};

    return window;
}

std::filesystem::path ExecutableDirectory()
{
    std::vector<wchar_t> buffer(
        32768,
        L'\0');

    const DWORD length =
        GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(
                buffer.size()));

    if (length == 0 ||
        length >= buffer.size()) {
        return
            std::filesystem::current_path();
    }

    return std::filesystem::path(
        std::wstring(
            buffer.data(),
            length))
        .parent_path();
}

} // namespace Hamun::Platform
