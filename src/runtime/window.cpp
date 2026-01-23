/// @file window.cpp
/// @brief Window management implementation for void_runtime

#include "window.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#endif

namespace void_runtime {

std::uint32_t Window::next_id_ = 1;

// =============================================================================
// Window Platform Implementation
// =============================================================================

#ifdef _WIN32

struct Window::Impl {
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC hglrc = nullptr;
    WindowConfig config;
    bool cursor_hidden = false;
    HCURSOR custom_cursor = nullptr;
    bool should_close = false;
    Window* owner = nullptr;

    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
};

static const wchar_t* WINDOW_CLASS_NAME = L"VoidEngineWindow";
static bool window_class_registered = false;

LRESULT CALLBACK Window::Impl::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    Window::Impl* impl = reinterpret_cast<Window::Impl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (!impl || !impl->owner) {
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }

    Window* window = impl->owner;
    WindowEvent event;
    event.window_id = window->id();

    switch (msg) {
        case WM_CLOSE:
            impl->should_close = true;
            event.type = WindowEventType::Close;
            if (window->event_callback_) {
                window->event_callback_(window->id(), event);
            }
            return 0;

        case WM_SIZE: {
            int width = LOWORD(lparam);
            int height = HIWORD(lparam);

            if (wparam == SIZE_MINIMIZED) {
                window->state_ = WindowState::Minimized;
                event.type = WindowEventType::Minimize;
            } else if (wparam == SIZE_MAXIMIZED) {
                window->state_ = WindowState::Maximized;
                event.type = WindowEventType::Maximize;
            } else if (wparam == SIZE_RESTORED) {
                window->state_ = WindowState::Normal;
                event.type = WindowEventType::Restore;
            } else {
                event.type = WindowEventType::Resize;
            }

            event.data.resize.width = width;
            event.data.resize.height = height;

            if (window->event_callback_) {
                window->event_callback_(window->id(), event);
            }
            break;
        }

        case WM_MOVE: {
            int x = GET_X_LPARAM(lparam);
            int y = GET_Y_LPARAM(lparam);

            event.type = WindowEventType::Move;
            event.data.move.x = x;
            event.data.move.y = y;

            if (window->event_callback_) {
                window->event_callback_(window->id(), event);
            }
            break;
        }

        case WM_SETFOCUS:
            event.type = WindowEventType::Focus;
            if (window->event_callback_) {
                window->event_callback_(window->id(), event);
            }
            break;

        case WM_KILLFOCUS:
            event.type = WindowEventType::Blur;
            if (window->event_callback_) {
                window->event_callback_(window->id(), event);
            }
            break;

        case WM_DPICHANGED: {
            RECT* suggested = reinterpret_cast<RECT*>(lparam);
            SetWindowPos(hwnd, nullptr,
                suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);

            event.type = WindowEventType::ContentScale;
            event.data.content_scale.x_scale = HIWORD(wparam) / 96.0f;
            event.data.content_scale.y_scale = LOWORD(wparam) / 96.0f;

            if (window->event_callback_) {
                window->event_callback_(window->id(), event);
            }
            break;
        }

        case WM_DROPFILES: {
            HDROP hdrop = reinterpret_cast<HDROP>(wparam);
            UINT count = DragQueryFileW(hdrop, 0xFFFFFFFF, nullptr, 0);

            event.type = WindowEventType::Drop;
            event.dropped_files.clear();

            for (UINT i = 0; i < count; ++i) {
                UINT size = DragQueryFileW(hdrop, i, nullptr, 0) + 1;
                std::wstring path(size, 0);
                DragQueryFileW(hdrop, i, path.data(), size);

                // Convert to UTF-8
                int utf8_size = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
                std::string utf8_path(utf8_size - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, utf8_path.data(), utf8_size, nullptr, nullptr);

                event.dropped_files.push_back(utf8_path);
            }

            DragFinish(hdrop);

            if (window->event_callback_) {
                window->event_callback_(window->id(), event);
            }
            break;
        }

        case WM_PAINT: {
            event.type = WindowEventType::Refresh;
            if (window->event_callback_) {
                window->event_callback_(window->id(), event);
            }
            break;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_SETCURSOR:
            if (LOWORD(lparam) == HTCLIENT && impl->cursor_hidden) {
                SetCursor(nullptr);
                return TRUE;
            }
            break;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

#else

struct Window::Impl {
    Display* display = nullptr;
    ::Window window = 0;
    Atom wm_delete_window;
    WindowConfig config;
    bool should_close = false;
    Window* owner = nullptr;
};

#endif

// =============================================================================
// Window Implementation
// =============================================================================

Window::Window() : impl_(std::make_unique<Impl>()) {
    impl_->owner = this;
}

Window::~Window() {
    destroy();
}

Window::Window(Window&& other) noexcept
    : impl_(std::move(other.impl_)),
      id_(other.id_),
      state_(other.state_),
      event_callback_(std::move(other.event_callback_)) {
    if (impl_) {
        impl_->owner = this;
    }
    other.id_ = 0;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        destroy();
        impl_ = std::move(other.impl_);
        id_ = other.id_;
        state_ = other.state_;
        event_callback_ = std::move(other.event_callback_);
        if (impl_) {
            impl_->owner = this;
        }
        other.id_ = 0;
    }
    return *this;
}

bool Window::create(const WindowConfig& config) {
    impl_->config = config;

#ifdef _WIN32
    // Register window class if needed
    if (!window_class_registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = Impl::window_proc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = WINDOW_CLASS_NAME;

        if (!RegisterClassExW(&wc)) {
            return false;
        }
        window_class_registered = true;
    }

    // Calculate window style
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD ex_style = WS_EX_APPWINDOW;

    if (!config.resizable) {
        style &= ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
    }
    if (!config.decorated) {
        style = WS_POPUP;
    }
    if (config.floating) {
        ex_style |= WS_EX_TOPMOST;
    }

    // Calculate window size for client area
    RECT rect = {0, 0, config.width, config.height};
    AdjustWindowRectEx(&rect, style, FALSE, ex_style);

    int window_width = rect.right - rect.left;
    int window_height = rect.bottom - rect.top;

    // Position
    int x = config.x;
    int y = config.y;
    if (x < 0 || y < 0) {
        x = (GetSystemMetrics(SM_CXSCREEN) - window_width) / 2;
        y = (GetSystemMetrics(SM_CYSCREEN) - window_height) / 2;
    }

    // Convert title to wide string
    int title_size = MultiByteToWideChar(CP_UTF8, 0, config.title.c_str(), -1, nullptr, 0);
    std::wstring title(title_size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, config.title.c_str(), -1, title.data(), title_size);

    // Create window
    impl_->hwnd = CreateWindowExW(
        ex_style,
        WINDOW_CLASS_NAME,
        title.c_str(),
        style,
        x, y,
        window_width, window_height,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );

    if (!impl_->hwnd) {
        return false;
    }

    SetWindowLongPtr(impl_->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl_.get()));

    // Enable drag and drop
    DragAcceptFiles(impl_->hwnd, TRUE);

    // Get DC for OpenGL
    impl_->hdc = GetDC(impl_->hwnd);

    // Show window
    if (config.visible) {
        int show_cmd = SW_SHOW;
        if (config.initial_state == WindowState::Maximized) {
            show_cmd = SW_SHOWMAXIMIZED;
        } else if (config.initial_state == WindowState::Minimized) {
            show_cmd = SW_SHOWMINIMIZED;
        }
        ShowWindow(impl_->hwnd, show_cmd);
    }

    if (config.focused) {
        SetForegroundWindow(impl_->hwnd);
        SetFocus(impl_->hwnd);
    }

    // Handle fullscreen
    if (config.initial_state == WindowState::Fullscreen) {
        set_fullscreen(true);
    } else if (config.initial_state == WindowState::FullscreenBorderless) {
        set_fullscreen_borderless(true);
    }

#else
    // X11 implementation
    impl_->display = XOpenDisplay(nullptr);
    if (!impl_->display) {
        return false;
    }

    int screen = DefaultScreen(impl_->display);
    ::Window root = RootWindow(impl_->display, screen);

    XSetWindowAttributes attrs = {};
    attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                       ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                       StructureNotifyMask | FocusChangeMask;

    impl_->window = XCreateWindow(
        impl_->display, root,
        config.x >= 0 ? config.x : 0,
        config.y >= 0 ? config.y : 0,
        config.width, config.height,
        0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWEventMask, &attrs
    );

    if (!impl_->window) {
        XCloseDisplay(impl_->display);
        return false;
    }

    // Set window title
    XStoreName(impl_->display, impl_->window, config.title.c_str());

    // Set WM_DELETE_WINDOW protocol
    impl_->wm_delete_window = XInternAtom(impl_->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(impl_->display, impl_->window, &impl_->wm_delete_window, 1);

    // Show window
    if (config.visible) {
        XMapWindow(impl_->display, impl_->window);
    }

#endif

    id_ = next_id_++;
    state_ = config.initial_state;

    return true;
}

void Window::destroy() {
    if (!is_valid()) {
        return;
    }

#ifdef _WIN32
    if (impl_->hdc) {
        ReleaseDC(impl_->hwnd, impl_->hdc);
        impl_->hdc = nullptr;
    }
    if (impl_->hwnd) {
        DestroyWindow(impl_->hwnd);
        impl_->hwnd = nullptr;
    }
    if (impl_->custom_cursor) {
        DestroyCursor(impl_->custom_cursor);
        impl_->custom_cursor = nullptr;
    }
#else
    if (impl_->window) {
        XDestroyWindow(impl_->display, impl_->window);
        impl_->window = 0;
    }
    if (impl_->display) {
        XCloseDisplay(impl_->display);
        impl_->display = nullptr;
    }
#endif

    id_ = 0;
}

bool Window::is_valid() const {
#ifdef _WIN32
    return impl_ && impl_->hwnd != nullptr;
#else
    return impl_ && impl_->window != 0;
#endif
}

std::string Window::title() const {
#ifdef _WIN32
    if (!impl_->hwnd) return "";
    int len = GetWindowTextLengthW(impl_->hwnd);
    std::wstring title(len + 1, 0);
    GetWindowTextW(impl_->hwnd, title.data(), len + 1);

    int utf8_size = WideCharToMultiByte(CP_UTF8, 0, title.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(utf8_size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, title.c_str(), -1, result.data(), utf8_size, nullptr, nullptr);
    return result;
#else
    // X11: Get window name
    char* name = nullptr;
    XFetchName(impl_->display, impl_->window, &name);
    std::string result = name ? name : "";
    if (name) XFree(name);
    return result;
#endif
}

void Window::set_title(const std::string& title) {
#ifdef _WIN32
    if (!impl_->hwnd) return;
    int wide_size = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, nullptr, 0);
    std::wstring wide_title(wide_size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, wide_title.data(), wide_size);
    SetWindowTextW(impl_->hwnd, wide_title.c_str());
#else
    XStoreName(impl_->display, impl_->window, title.c_str());
#endif
}

int Window::width() const {
    int w, h;
    get_size(w, h);
    return w;
}

int Window::height() const {
    int w, h;
    get_size(w, h);
    return h;
}

void Window::get_size(int& width, int& height) const {
#ifdef _WIN32
    if (!impl_->hwnd) {
        width = height = 0;
        return;
    }
    RECT rect;
    GetClientRect(impl_->hwnd, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
#else
    XWindowAttributes attrs;
    XGetWindowAttributes(impl_->display, impl_->window, &attrs);
    width = attrs.width;
    height = attrs.height;
#endif
}

void Window::set_size(int width, int height) {
#ifdef _WIN32
    if (!impl_->hwnd) return;

    DWORD style = GetWindowLong(impl_->hwnd, GWL_STYLE);
    DWORD ex_style = GetWindowLong(impl_->hwnd, GWL_EXSTYLE);

    RECT rect = {0, 0, width, height};
    AdjustWindowRectEx(&rect, style, FALSE, ex_style);

    SetWindowPos(impl_->hwnd, nullptr, 0, 0,
                 rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOMOVE | SWP_NOZORDER);
#else
    XResizeWindow(impl_->display, impl_->window, width, height);
#endif
}

void Window::get_framebuffer_size(int& width, int& height) const {
    // For now, same as client size (would differ with DPI scaling/OpenGL)
    get_size(width, height);
}

int Window::x() const {
    int x, y;
    get_position(x, y);
    return x;
}

int Window::y() const {
    int x, y;
    get_position(x, y);
    return y;
}

void Window::get_position(int& x, int& y) const {
#ifdef _WIN32
    if (!impl_->hwnd) {
        x = y = 0;
        return;
    }
    RECT rect;
    GetWindowRect(impl_->hwnd, &rect);
    x = rect.left;
    y = rect.top;
#else
    XWindowAttributes attrs;
    XGetWindowAttributes(impl_->display, impl_->window, &attrs);
    x = attrs.x;
    y = attrs.y;
#endif
}

void Window::set_position(int x, int y) {
#ifdef _WIN32
    if (!impl_->hwnd) return;
    SetWindowPos(impl_->hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
#else
    XMoveWindow(impl_->display, impl_->window, x, y);
#endif
}

void Window::set_size_limits(int min_w, int min_h, int max_w, int max_h) {
#ifdef _WIN32
    impl_->config.min_width = min_w;
    impl_->config.min_height = min_h;
    impl_->config.max_width = max_w;
    impl_->config.max_height = max_h;
#else
    XSizeHints hints = {};
    hints.flags = PMinSize | PMaxSize;
    hints.min_width = min_w > 0 ? min_w : 1;
    hints.min_height = min_h > 0 ? min_h : 1;
    hints.max_width = max_w > 0 ? max_w : 32767;
    hints.max_height = max_h > 0 ? max_h : 32767;
    XSetWMNormalHints(impl_->display, impl_->window, &hints);
#endif
}

float Window::aspect_ratio() const {
    int w, h;
    get_size(w, h);
    return h > 0 ? static_cast<float>(w) / h : 1.0f;
}

void Window::set_aspect_ratio(int numerator, int denominator) {
#ifdef _WIN32
    // Not directly supported, would need to handle in WM_SIZING
#else
    XSizeHints hints = {};
    hints.flags = PAspect;
    hints.min_aspect.x = hints.max_aspect.x = numerator;
    hints.min_aspect.y = hints.max_aspect.y = denominator;
    XSetWMNormalHints(impl_->display, impl_->window, &hints);
#endif
}

void Window::get_content_scale(float& x_scale, float& y_scale) const {
#ifdef _WIN32
    if (!impl_->hwnd) {
        x_scale = y_scale = 1.0f;
        return;
    }
    HDC hdc = GetDC(impl_->hwnd);
    x_scale = GetDeviceCaps(hdc, LOGPIXELSX) / 96.0f;
    y_scale = GetDeviceCaps(hdc, LOGPIXELSY) / 96.0f;
    ReleaseDC(impl_->hwnd, hdc);
#else
    x_scale = y_scale = 1.0f;
#endif
}

void Window::minimize() {
#ifdef _WIN32
    if (impl_->hwnd) ShowWindow(impl_->hwnd, SW_MINIMIZE);
#else
    XIconifyWindow(impl_->display, impl_->window, DefaultScreen(impl_->display));
#endif
    state_ = WindowState::Minimized;
}

void Window::maximize() {
#ifdef _WIN32
    if (impl_->hwnd) ShowWindow(impl_->hwnd, SW_MAXIMIZE);
#else
    // X11 maximize via _NET_WM_STATE
#endif
    state_ = WindowState::Maximized;
}

void Window::restore() {
#ifdef _WIN32
    if (impl_->hwnd) ShowWindow(impl_->hwnd, SW_RESTORE);
#else
    XMapWindow(impl_->display, impl_->window);
#endif
    state_ = WindowState::Normal;
}

void Window::show() {
#ifdef _WIN32
    if (impl_->hwnd) ShowWindow(impl_->hwnd, SW_SHOW);
#else
    XMapWindow(impl_->display, impl_->window);
#endif
}

void Window::hide() {
#ifdef _WIN32
    if (impl_->hwnd) ShowWindow(impl_->hwnd, SW_HIDE);
#else
    XUnmapWindow(impl_->display, impl_->window);
#endif
}

bool Window::is_visible() const {
#ifdef _WIN32
    return impl_->hwnd && IsWindowVisible(impl_->hwnd);
#else
    XWindowAttributes attrs;
    XGetWindowAttributes(impl_->display, impl_->window, &attrs);
    return attrs.map_state == IsViewable;
#endif
}

void Window::focus() {
#ifdef _WIN32
    if (impl_->hwnd) {
        SetForegroundWindow(impl_->hwnd);
        SetFocus(impl_->hwnd);
    }
#else
    XSetInputFocus(impl_->display, impl_->window, RevertToParent, CurrentTime);
#endif
}

bool Window::is_focused() const {
#ifdef _WIN32
    return impl_->hwnd && GetForegroundWindow() == impl_->hwnd;
#else
    ::Window focused;
    int revert;
    XGetInputFocus(impl_->display, &focused, &revert);
    return focused == impl_->window;
#endif
}

void Window::set_fullscreen(bool fullscreen, int monitor) {
#ifdef _WIN32
    if (!impl_->hwnd) return;

    static WINDOWPLACEMENT prev_placement = {sizeof(prev_placement)};
    static LONG prev_style = 0;

    DWORD style = GetWindowLong(impl_->hwnd, GWL_STYLE);

    if (fullscreen) {
        MONITORINFO mi = {sizeof(mi)};
        if (GetWindowPlacement(impl_->hwnd, &prev_placement) &&
            GetMonitorInfo(MonitorFromWindow(impl_->hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            prev_style = style;
            SetWindowLong(impl_->hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(impl_->hwnd, HWND_TOP,
                mi.rcMonitor.left, mi.rcMonitor.top,
                mi.rcMonitor.right - mi.rcMonitor.left,
                mi.rcMonitor.bottom - mi.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
        state_ = WindowState::Fullscreen;
    } else {
        SetWindowLong(impl_->hwnd, GWL_STYLE, prev_style);
        SetWindowPlacement(impl_->hwnd, &prev_placement);
        SetWindowPos(impl_->hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        state_ = WindowState::Normal;
    }
#else
    // X11 fullscreen via _NET_WM_STATE_FULLSCREEN
#endif
}

void Window::set_fullscreen_borderless(bool borderless, int monitor) {
    set_fullscreen(borderless, monitor);
    if (borderless) {
        state_ = WindowState::FullscreenBorderless;
    }
}

bool Window::is_fullscreen() const {
    return state_ == WindowState::Fullscreen || state_ == WindowState::FullscreenBorderless;
}

bool Window::should_close() const {
    return impl_->should_close;
}

void Window::set_should_close(bool close) {
    impl_->should_close = close;
}

void Window::set_cursor_mode(CursorMode mode) {
#ifdef _WIN32
    switch (mode) {
        case CursorMode::Normal:
            impl_->cursor_hidden = false;
            ClipCursor(nullptr);
            ShowCursor(TRUE);
            break;
        case CursorMode::Hidden:
            impl_->cursor_hidden = true;
            ShowCursor(FALSE);
            break;
        case CursorMode::Disabled:
        case CursorMode::Captured:
            impl_->cursor_hidden = true;
            ShowCursor(FALSE);
            RECT rect;
            GetClientRect(impl_->hwnd, &rect);
            MapWindowPoints(impl_->hwnd, nullptr, reinterpret_cast<POINT*>(&rect), 2);
            ClipCursor(&rect);
            break;
    }
#endif
}

CursorMode Window::cursor_mode() const {
    return CursorMode::Normal;
}

void Window::get_cursor_position(double& x, double& y) const {
#ifdef _WIN32
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(impl_->hwnd, &pt);
    x = pt.x;
    y = pt.y;
#else
    ::Window root, child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;
    XQueryPointer(impl_->display, impl_->window, &root, &child,
                  &root_x, &root_y, &win_x, &win_y, &mask);
    x = win_x;
    y = win_y;
#endif
}

void Window::set_cursor_position(double x, double y) {
#ifdef _WIN32
    POINT pt = {static_cast<LONG>(x), static_cast<LONG>(y)};
    ClientToScreen(impl_->hwnd, &pt);
    SetCursorPos(pt.x, pt.y);
#else
    XWarpPointer(impl_->display, None, impl_->window, 0, 0, 0, 0,
                 static_cast<int>(x), static_cast<int>(y));
#endif
}

void* Window::native_handle() const {
#ifdef _WIN32
    return impl_->hwnd;
#else
    return reinterpret_cast<void*>(impl_->window);
#endif
}

void Window::make_context_current() {
#ifdef _WIN32
    if (impl_->hglrc) {
        wglMakeCurrent(impl_->hdc, impl_->hglrc);
    }
#endif
}

void Window::swap_buffers() {
#ifdef _WIN32
    if (impl_->hdc) {
        SwapBuffers(impl_->hdc);
    }
#else
    // X11/GLX swap would go here
#endif
}

void Window::poll_events() {
#ifdef _WIN32
    MSG msg;
    while (PeekMessage(&msg, impl_->hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#else
    while (XPending(impl_->display)) {
        XEvent event;
        XNextEvent(impl_->display, &event);

        if (event.type == ClientMessage) {
            if (static_cast<Atom>(event.xclient.data.l[0]) == impl_->wm_delete_window) {
                impl_->should_close = true;
            }
        }
    }
#endif
}

void Window::wait_events() {
#ifdef _WIN32
    WaitMessage();
    poll_events();
#else
    XEvent event;
    XPeekEvent(impl_->display, &event);
    poll_events();
#endif
}

void Window::wait_events(double timeout) {
    // Simple timeout implementation using sleep + poll
    std::this_thread::sleep_for(std::chrono::duration<double>(timeout));
    poll_events();
}

void Window::set_event_callback(WindowEventCallback callback) {
    event_callback_ = std::move(callback);
}

// =============================================================================
// Monitor Functions
// =============================================================================

std::vector<MonitorInfo> Window::all_monitors() {
    std::vector<MonitorInfo> monitors;

#ifdef _WIN32
    EnumDisplayMonitors(nullptr, nullptr,
        [](HMONITOR hmon, HDC, LPRECT, LPARAM data) -> BOOL {
            auto& monitors = *reinterpret_cast<std::vector<MonitorInfo>*>(data);

            MONITORINFOEXW mi = {};
            mi.cbSize = sizeof(mi);
            GetMonitorInfoW(hmon, &mi);

            MonitorInfo info;
            int name_size = WideCharToMultiByte(CP_UTF8, 0, mi.szDevice, -1, nullptr, 0, nullptr, nullptr);
            info.name.resize(name_size - 1);
            WideCharToMultiByte(CP_UTF8, 0, mi.szDevice, -1, info.name.data(), name_size, nullptr, nullptr);

            info.x = mi.rcMonitor.left;
            info.y = mi.rcMonitor.top;
            info.current_mode.width = mi.rcMonitor.right - mi.rcMonitor.left;
            info.current_mode.height = mi.rcMonitor.bottom - mi.rcMonitor.top;
            info.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

            monitors.push_back(info);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&monitors));
#else
    // X11 monitor enumeration
    MonitorInfo primary;
    primary.name = "Default";
    primary.primary = true;
    // Get screen dimensions
    Display* display = XOpenDisplay(nullptr);
    if (display) {
        int screen = DefaultScreen(display);
        primary.current_mode.width = DisplayWidth(display, screen);
        primary.current_mode.height = DisplayHeight(display, screen);
        XCloseDisplay(display);
    }
    monitors.push_back(primary);
#endif

    return monitors;
}

MonitorInfo Window::primary_monitor() {
    auto monitors = all_monitors();
    for (const auto& mon : monitors) {
        if (mon.primary) {
            return mon;
        }
    }
    return monitors.empty() ? MonitorInfo{} : monitors[0];
}

MonitorInfo Window::current_monitor() const {
#ifdef _WIN32
    if (!impl_->hwnd) return primary_monitor();

    HMONITOR hmon = MonitorFromWindow(impl_->hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hmon, &mi);

    MonitorInfo info;
    int name_size = WideCharToMultiByte(CP_UTF8, 0, mi.szDevice, -1, nullptr, 0, nullptr, nullptr);
    info.name.resize(name_size - 1);
    WideCharToMultiByte(CP_UTF8, 0, mi.szDevice, -1, info.name.data(), name_size, nullptr, nullptr);

    info.x = mi.rcMonitor.left;
    info.y = mi.rcMonitor.top;
    info.current_mode.width = mi.rcMonitor.right - mi.rcMonitor.left;
    info.current_mode.height = mi.rcMonitor.bottom - mi.rcMonitor.top;
    info.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

    return info;
#else
    return primary_monitor();
#endif
}

// =============================================================================
// Window Manager Implementation
// =============================================================================

struct WindowManager::Impl {
    std::unordered_map<WindowId, std::unique_ptr<Window>> windows;
};

static WindowManager* g_window_manager = nullptr;

WindowManager::WindowManager() : impl_(std::make_unique<Impl>()) {
    g_window_manager = this;
}

WindowManager::~WindowManager() {
    if (g_window_manager == this) {
        g_window_manager = nullptr;
    }
}

WindowManager& WindowManager::instance() {
    if (!g_window_manager) {
        static WindowManager static_instance;
        g_window_manager = &static_instance;
    }
    return *g_window_manager;
}

Window* WindowManager::create_window(const WindowConfig& config) {
    auto window = std::make_unique<Window>();
    if (!window->create(config)) {
        return nullptr;
    }

    WindowId id = window->id();
    Window* ptr = window.get();
    impl_->windows[id] = std::move(window);

    return ptr;
}

Window* WindowManager::get_window(WindowId id) {
    auto it = impl_->windows.find(id);
    return it != impl_->windows.end() ? it->second.get() : nullptr;
}

void WindowManager::destroy_window(WindowId id) {
    impl_->windows.erase(id);
}

std::vector<Window*> WindowManager::all_windows() {
    std::vector<Window*> result;
    for (auto& [id, window] : impl_->windows) {
        result.push_back(window.get());
    }
    return result;
}

std::size_t WindowManager::window_count() const {
    return impl_->windows.size();
}

void WindowManager::poll_all_events() {
    for (auto& [id, window] : impl_->windows) {
        window->poll_events();
    }
}

bool WindowManager::any_should_close() const {
    for (const auto& [id, window] : impl_->windows) {
        if (window->should_close()) {
            return true;
        }
    }
    return false;
}

} // namespace void_runtime
