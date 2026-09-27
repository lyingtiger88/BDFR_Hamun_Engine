#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <Hamun/Core/Log.hpp>
#include <Hamun/Platform/Window.hpp>

#include <algorithm>
#include <memory>
#include <string>

namespace Hamun::Platform {
namespace {

constexpr wchar_t kWindowClassName[] = L"HamunEngineWindowClass";

std::wstring ToWide(const std::string& text)
{
    if (text.empty())
        return L"";

    const int length = MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        nullptr, 0);

    if (length <= 0)
        return std::wstring(text.begin(), text.end());

    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        result.data(), length);
    return result;
}

class Win32Window final : public IWindow {
public:
    explicit Win32Window(const WindowDesc& desc)
        : width_(std::max<std::uint32_t>(desc.width, 1u))
        , height_(std::max<std::uint32_t>(desc.height, 1u))
    {
        static bool registered = false;
        if (!registered) {
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = &Win32Window::WndProc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.lpszClassName = kWindowClassName;

            if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
                Core::Log(Core::LogLevel::Error,
                    "Win32: RegisterClassExW failed.");
                return;
            }
            registered = true;
        }

        RECT rect{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        const std::wstring title = ToWide(desc.title);
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
            Core::Log(Core::LogLevel::Error,
                "Win32: CreateWindowExW failed.");
            return;
        }

        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
    }

    ~Win32Window() override
    {
        if (hwnd_ && IsWindow(hwnd_))
            DestroyWindow(hwnd_);
    }

    [[nodiscard]] bool IsValid() const noexcept { return hwnd_ != nullptr; }

    void* NativeHandle() const noexcept override
    {
        return hwnd_;
    }

    std::uint32_t Width() const noexcept override { return width_; }
    std::uint32_t Height() const noexcept override { return height_; }

    bool PumpEvents() override
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT)
                return false;

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return !quit_;
    }

private:
    static LRESULT CALLBACK WndProc(
        HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        Win32Window* self =
            reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        if (message == WM_NCCREATE) {
            const auto* create =
                reinterpret_cast<const CREATESTRUCTW*>(lParam);
            self = static_cast<Win32Window*>(create->lpCreateParams);
            SetWindowLongPtrW(
                hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
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
                if (self && wParam != SIZE_MINIMIZED) {
                    self->width_ =
                        std::max<std::uint32_t>(LOWORD(lParam), 1u);
                    self->height_ =
                        std::max<std::uint32_t>(HIWORD(lParam), 1u);
                }
                return 0;

            default:
                break;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    HWND hwnd_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    bool quit_ = false;
};

} // namespace

std::unique_ptr<IWindow> CreateWindow(const WindowDesc& desc)
{
    auto window = std::make_unique<Win32Window>(desc);
    if (!window->IsValid())
        return {};
    return window;
}

} // namespace Hamun::Platform
