/// @file platform.cpp
/// @brief Platform abstraction layer implementation

#include <void_engine/runtime/platform.hpp>
#include <void_engine/runtime/runtime_config.hpp>

#include <spdlog/spdlog.h>

#include <chrono>
#include <queue>
#include <mutex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <xinput.h>
#include <GL/gl.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "xinput.lib")
#pragma comment(lib, "opengl32.lib")
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace void_runtime {

// =============================================================================
// HeadlessPlatform Implementation
// =============================================================================

/// Headless platform for server/compute workloads
class HeadlessPlatform : public IPlatform {
public:
    HeadlessPlatform() = default;
    ~HeadlessPlatform() override { shutdown(); }

    void_core::Result<void> initialize(
        const PlatformWindowConfig& /*window_config*/,
        const PlatformGpuConfig& /*gpu_config*/) override {

        m_info.name = "Headless";
        m_info.version = "1.0";
        m_info.capabilities.has_window = false;
        m_info.capabilities.has_input = false;
        m_info.capabilities.has_gpu = false;
        m_info.capabilities.gpu_backend = void_render::GpuBackend::Null;
        m_info.capabilities.display_backend = void_render::DisplayBackend::Headless;

        m_start_time = std::chrono::high_resolution_clock::now();
        m_initialized = true;

        spdlog::info("HeadlessPlatform initialized");
        return void_core::Ok();
    }

    void shutdown() override {
        if (!m_initialized) return;
        m_initialized = false;
        spdlog::info("HeadlessPlatform shutdown");
    }

    [[nodiscard]] bool is_initialized() const override { return m_initialized; }

    void poll_events(const PlatformEventCallback& callback) override {
        std::lock_guard<std::mutex> lock(m_event_mutex);
        while (!m_event_queue.empty()) {
            callback(m_event_queue.front());
            m_event_queue.pop();
        }
    }

    void wait_events(std::chrono::milliseconds timeout) override {
        if (timeout.count() > 0) {
            std::this_thread::sleep_for(timeout);
        }
    }

    void request_quit() override {
        m_should_quit = true;
        std::lock_guard<std::mutex> lock(m_event_mutex);
        m_event_queue.push(PlatformEvent{PlatformEventType::Quit});
    }

    [[nodiscard]] bool should_quit() const override { return m_should_quit; }

    void get_window_size(std::uint32_t& width, std::uint32_t& height) const override {
        width = m_width;
        height = m_height;
    }

    void set_window_size(std::uint32_t width, std::uint32_t height) override {
        m_width = width;
        m_height = height;
    }

    void get_framebuffer_size(std::uint32_t& width, std::uint32_t& height) const override {
        width = m_width;
        height = m_height;
    }

    void get_window_position(std::int32_t& x, std::int32_t& y) const override {
        x = 0; y = 0;
    }

    void set_window_position(std::int32_t /*x*/, std::int32_t /*y*/) override {}
    void set_window_title(const std::string& /*title*/) override {}
    void set_fullscreen(bool /*fullscreen*/, std::uint32_t /*monitor*/) override {}
    [[nodiscard]] bool is_fullscreen() const override { return false; }
    void minimize_window() override {}
    void maximize_window() override {}
    void restore_window() override {}
    void focus_window() override {}
    [[nodiscard]] bool is_window_focused() const override { return true; }

    void get_content_scale(float& x_scale, float& y_scale) const override {
        x_scale = 1.0f;
        y_scale = 1.0f;
    }

    [[nodiscard]] void* native_window_handle() const override { return nullptr; }

    void set_cursor_visible(bool /*visible*/) override {}
    void set_cursor_captured(bool /*captured*/) override {}
    void get_cursor_position(double& x, double& y) const override { x = 0; y = 0; }
    void set_cursor_position(double /*x*/, double /*y*/) override {}

    void begin_frame() override {}
    void end_frame() override {}

    [[nodiscard]] void_render::GpuBackend gpu_backend() const override {
        return void_render::GpuBackend::Null;
    }

    [[nodiscard]] void* native_gpu_context() const override { return nullptr; }

    [[nodiscard]] const PlatformInfo& info() const override { return m_info; }
    [[nodiscard]] const PlatformCapabilities& capabilities() const override {
        return m_info.capabilities;
    }

    [[nodiscard]] double get_time() const override {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(now - m_start_time).count() + m_time_offset;
    }

    void set_time(double time) override {
        m_time_offset = time - get_time() + m_time_offset;
    }

private:
    bool m_initialized{false};
    bool m_should_quit{false};
    std::uint32_t m_width{1920};
    std::uint32_t m_height{1080};
    PlatformInfo m_info;
    std::chrono::high_resolution_clock::time_point m_start_time;
    double m_time_offset{0};

    std::queue<PlatformEvent> m_event_queue;
    std::mutex m_event_mutex;
};

// =============================================================================
// WindowedPlatform Implementation (Win32)
// =============================================================================

#ifdef _WIN32

/// Windowed platform for desktop (Win32)
class WindowedPlatformWin32 : public IPlatform {
public:
    WindowedPlatformWin32() = default;
    ~WindowedPlatformWin32() override { shutdown(); }

    void_core::Result<void> initialize(
        const PlatformWindowConfig& window_config,
        const PlatformGpuConfig& gpu_config) override {

        m_config = window_config;

        // Register window class
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = window_proc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.lpszClassName = L"VoidEnginePlatform";

        if (!RegisterClassExW(&wc)) {
            DWORD error = GetLastError();
            if (error != ERROR_CLASS_ALREADY_EXISTS) {
                return void_core::Error("Failed to register window class");
            }
        }

        // Calculate window style
        DWORD style = WS_OVERLAPPEDWINDOW;
        DWORD ex_style = WS_EX_APPWINDOW;

        if (!window_config.resizable) {
            style &= ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
        }
        if (window_config.borderless) {
            style = WS_POPUP;
        }
        if (window_config.floating) {
            ex_style |= WS_EX_TOPMOST;
        }

        // Calculate window size for client area
        RECT rect = {0, 0, static_cast<LONG>(window_config.width),
                     static_cast<LONG>(window_config.height)};
        AdjustWindowRectEx(&rect, style, FALSE, ex_style);

        int window_width = rect.right - rect.left;
        int window_height = rect.bottom - rect.top;

        // Position
        int x = window_config.x;
        int y = window_config.y;
        if (x < 0 || y < 0) {
            x = (GetSystemMetrics(SM_CXSCREEN) - window_width) / 2;
            y = (GetSystemMetrics(SM_CYSCREEN) - window_height) / 2;
        }

        // Convert title to wide string
        int title_size = MultiByteToWideChar(CP_UTF8, 0, window_config.title.c_str(), -1, nullptr, 0);
        std::wstring title(title_size - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, window_config.title.c_str(), -1, title.data(), title_size);

        // Create window
        m_hwnd = CreateWindowExW(
            ex_style,
            L"VoidEnginePlatform",
            title.c_str(),
            style,
            x, y,
            window_width, window_height,
            nullptr, nullptr,
            GetModuleHandle(nullptr),
            this  // Pass 'this' for WM_NCCREATE
        );

        if (!m_hwnd) {
            return void_core::Error("Failed to create window");
        }

        // Enable DPI awareness
        SetProcessDPIAware();

        // Enable drag and drop
        DragAcceptFiles(m_hwnd, TRUE);

        // Get device context
        m_hdc = GetDC(m_hwnd);
        if (!m_hdc) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
            return void_core::Error("Failed to get device context");
        }

        // Initialize OpenGL context if using OpenGL backend
        if (gpu_config.preferred_backend == void_render::GpuBackend::Auto ||
            gpu_config.preferred_backend == void_render::GpuBackend::OpenGL) {

            if (!init_opengl_context(gpu_config)) {
                // OpenGL failed, try software/null
                spdlog::warn("OpenGL initialization failed, continuing without GPU");
                m_gpu_backend = void_render::GpuBackend::Null;
            } else {
                m_gpu_backend = void_render::GpuBackend::OpenGL;
            }
        }

        // Show window
        if (window_config.visible) {
            int show_cmd = SW_SHOW;
            if (window_config.fullscreen) {
                set_fullscreen(true, 0);
            }
            ShowWindow(m_hwnd, show_cmd);
        }

        if (window_config.focused) {
            SetForegroundWindow(m_hwnd);
            SetFocus(m_hwnd);
        }

        // Query platform info
        query_platform_info();

        m_start_time = std::chrono::high_resolution_clock::now();
        m_initialized = true;

        spdlog::info("WindowedPlatformWin32 initialized ({}x{}, GPU: {})",
                     window_config.width, window_config.height,
                     void_render::gpu_backend_name(m_gpu_backend));

        return void_core::Ok();
    }

    void shutdown() override {
        if (!m_initialized) return;

        if (m_hglrc) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(m_hglrc);
            m_hglrc = nullptr;
        }

        if (m_hdc && m_hwnd) {
            ReleaseDC(m_hwnd, m_hdc);
            m_hdc = nullptr;
        }

        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }

        m_initialized = false;
        spdlog::info("WindowedPlatformWin32 shutdown");
    }

    [[nodiscard]] bool is_initialized() const override { return m_initialized; }

    void poll_events(const PlatformEventCallback& callback) override {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_should_quit = true;
                PlatformEvent evt(PlatformEventType::Quit);
                callback(evt);
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        // Process queued events
        std::lock_guard<std::mutex> lock(m_event_mutex);
        while (!m_event_queue.empty()) {
            callback(m_event_queue.front());
            m_event_queue.pop();
        }

        // Poll gamepads
        poll_gamepads(callback);
    }

    void wait_events(std::chrono::milliseconds timeout) override {
        if (timeout.count() == 0) {
            WaitMessage();
        } else {
            MsgWaitForMultipleObjects(0, nullptr, FALSE,
                static_cast<DWORD>(timeout.count()), QS_ALLINPUT);
        }
    }

    void request_quit() override {
        m_should_quit = true;
        PostQuitMessage(0);
    }

    [[nodiscard]] bool should_quit() const override { return m_should_quit; }

    void get_window_size(std::uint32_t& width, std::uint32_t& height) const override {
        if (!m_hwnd) { width = height = 0; return; }
        RECT rect;
        GetClientRect(m_hwnd, &rect);
        width = static_cast<std::uint32_t>(rect.right - rect.left);
        height = static_cast<std::uint32_t>(rect.bottom - rect.top);
    }

    void set_window_size(std::uint32_t width, std::uint32_t height) override {
        if (!m_hwnd) return;
        DWORD style = GetWindowLong(m_hwnd, GWL_STYLE);
        DWORD ex_style = GetWindowLong(m_hwnd, GWL_EXSTYLE);
        RECT rect = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
        AdjustWindowRectEx(&rect, style, FALSE, ex_style);
        SetWindowPos(m_hwnd, nullptr, 0, 0,
                     rect.right - rect.left, rect.bottom - rect.top,
                     SWP_NOMOVE | SWP_NOZORDER);
    }

    void get_framebuffer_size(std::uint32_t& width, std::uint32_t& height) const override {
        get_window_size(width, height);
        // Apply DPI scaling
        float scale_x, scale_y;
        get_content_scale(scale_x, scale_y);
        width = static_cast<std::uint32_t>(width * scale_x);
        height = static_cast<std::uint32_t>(height * scale_y);
    }

    void get_window_position(std::int32_t& x, std::int32_t& y) const override {
        if (!m_hwnd) { x = y = 0; return; }
        RECT rect;
        GetWindowRect(m_hwnd, &rect);
        x = rect.left;
        y = rect.top;
    }

    void set_window_position(std::int32_t x, std::int32_t y) override {
        if (!m_hwnd) return;
        SetWindowPos(m_hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    void set_window_title(const std::string& title) override {
        if (!m_hwnd) return;
        int wide_size = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, nullptr, 0);
        std::wstring wide_title(wide_size - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, wide_title.data(), wide_size);
        SetWindowTextW(m_hwnd, wide_title.c_str());
    }

    void set_fullscreen(bool fullscreen, std::uint32_t /*monitor*/) override {
        if (!m_hwnd) return;

        if (fullscreen && !m_fullscreen) {
            // Save current placement
            GetWindowPlacement(m_hwnd, &m_prev_placement);
            m_prev_style = GetWindowLong(m_hwnd, GWL_STYLE);

            // Go fullscreen
            MONITORINFO mi = {sizeof(mi)};
            if (GetMonitorInfo(MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
                SetWindowLong(m_hwnd, GWL_STYLE, m_prev_style & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(m_hwnd, HWND_TOP,
                    mi.rcMonitor.left, mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top,
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            }
            m_fullscreen = true;
        } else if (!fullscreen && m_fullscreen) {
            // Restore window
            SetWindowLong(m_hwnd, GWL_STYLE, m_prev_style);
            SetWindowPlacement(m_hwnd, &m_prev_placement);
            SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            m_fullscreen = false;
        }
    }

    [[nodiscard]] bool is_fullscreen() const override { return m_fullscreen; }

    void minimize_window() override {
        if (m_hwnd) ShowWindow(m_hwnd, SW_MINIMIZE);
    }

    void maximize_window() override {
        if (m_hwnd) ShowWindow(m_hwnd, SW_MAXIMIZE);
    }

    void restore_window() override {
        if (m_hwnd) ShowWindow(m_hwnd, SW_RESTORE);
    }

    void focus_window() override {
        if (m_hwnd) {
            SetForegroundWindow(m_hwnd);
            SetFocus(m_hwnd);
        }
    }

    [[nodiscard]] bool is_window_focused() const override {
        return m_hwnd && GetForegroundWindow() == m_hwnd;
    }

    void get_content_scale(float& x_scale, float& y_scale) const override {
        if (!m_hwnd) { x_scale = y_scale = 1.0f; return; }
        HDC hdc = GetDC(m_hwnd);
        x_scale = GetDeviceCaps(hdc, LOGPIXELSX) / 96.0f;
        y_scale = GetDeviceCaps(hdc, LOGPIXELSY) / 96.0f;
        ReleaseDC(m_hwnd, hdc);
    }

    [[nodiscard]] void* native_window_handle() const override {
        return m_hwnd;
    }

    void set_cursor_visible(bool visible) override {
        if (visible != m_cursor_visible) {
            m_cursor_visible = visible;
            ShowCursor(visible ? TRUE : FALSE);
        }
    }

    void set_cursor_captured(bool captured) override {
        if (!m_hwnd) return;
        if (captured) {
            RECT rect;
            GetClientRect(m_hwnd, &rect);
            MapWindowPoints(m_hwnd, nullptr, reinterpret_cast<POINT*>(&rect), 2);
            ClipCursor(&rect);
        } else {
            ClipCursor(nullptr);
        }
        m_cursor_captured = captured;
    }

    void get_cursor_position(double& x, double& y) const override {
        POINT pt;
        GetCursorPos(&pt);
        if (m_hwnd) ScreenToClient(m_hwnd, &pt);
        x = pt.x;
        y = pt.y;
    }

    void set_cursor_position(double x, double y) override {
        POINT pt = {static_cast<LONG>(x), static_cast<LONG>(y)};
        if (m_hwnd) ClientToScreen(m_hwnd, &pt);
        SetCursorPos(pt.x, pt.y);
    }

    void begin_frame() override {
        if (!m_hglrc) return;

        // Get current window size for viewport
        std::uint32_t width, height;
        get_window_size(width, height);

        // Set viewport to match window
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));

        // Note: Screen clearing is handled by SceneRenderer::render()
    }

    void end_frame() override {
        if (m_hdc && m_hglrc) {
            SwapBuffers(m_hdc);
        }
    }

    [[nodiscard]] void_render::GpuBackend gpu_backend() const override {
        return m_gpu_backend;
    }

    [[nodiscard]] void* native_gpu_context() const override {
        return m_hglrc;
    }

    [[nodiscard]] const PlatformInfo& info() const override { return m_info; }
    [[nodiscard]] const PlatformCapabilities& capabilities() const override {
        return m_info.capabilities;
    }

    [[nodiscard]] std::optional<std::string> get_clipboard_text() const override {
        if (!OpenClipboard(nullptr)) return std::nullopt;

        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (!hData) {
            CloseClipboard();
            return std::nullopt;
        }

        wchar_t* wstr = static_cast<wchar_t*>(GlobalLock(hData));
        if (!wstr) {
            CloseClipboard();
            return std::nullopt;
        }

        int utf8_size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
        std::string result(utf8_size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, result.data(), utf8_size, nullptr, nullptr);

        GlobalUnlock(hData);
        CloseClipboard();
        return result;
    }

    void set_clipboard_text(const std::string& text) override {
        if (!OpenClipboard(nullptr)) return;
        EmptyClipboard();

        int wide_size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, wide_size * sizeof(wchar_t));
        if (!hMem) {
            CloseClipboard();
            return;
        }

        wchar_t* wstr = static_cast<wchar_t*>(GlobalLock(hMem));
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wstr, wide_size);
        GlobalUnlock(hMem);

        SetClipboardData(CF_UNICODETEXT, hMem);
        CloseClipboard();
    }

    [[nodiscard]] double get_time() const override {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(now - m_start_time).count() + m_time_offset;
    }

    void set_time(double time) override {
        m_time_offset = time - get_time() + m_time_offset;
    }

private:
    bool init_opengl_context(const PlatformGpuConfig& gpu_config) {
        // Set pixel format
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;
        pfd.iLayerType = PFD_MAIN_PLANE;

        int pixel_format = ChoosePixelFormat(m_hdc, &pfd);
        if (!pixel_format) {
            spdlog::error("Failed to choose pixel format");
            return false;
        }

        if (!SetPixelFormat(m_hdc, pixel_format, &pfd)) {
            spdlog::error("Failed to set pixel format");
            return false;
        }

        // Create legacy context first
        m_hglrc = wglCreateContext(m_hdc);
        if (!m_hglrc) {
            spdlog::error("Failed to create OpenGL context");
            return false;
        }

        if (!wglMakeCurrent(m_hdc, m_hglrc)) {
            spdlog::error("Failed to make OpenGL context current");
            wglDeleteContext(m_hglrc);
            m_hglrc = nullptr;
            return false;
        }

        // Try to create modern context if available
        using PFNWGLCREATECONTEXTATTRIBSARBPROC = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
        auto wglCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(
            wglGetProcAddress("wglCreateContextAttribsARB"));

        if (wglCreateContextAttribsARB) {
            int attribs[] = {
                0x2091, 4,  // WGL_CONTEXT_MAJOR_VERSION_ARB
                0x2092, 5,  // WGL_CONTEXT_MINOR_VERSION_ARB
                0x9126, 0x0001,  // WGL_CONTEXT_PROFILE_MASK_ARB = CORE
                0x2094, gpu_config.enable_validation ? 0x0001 : 0,  // WGL_CONTEXT_FLAGS_ARB = DEBUG
                0
            };

            HGLRC modern_context = wglCreateContextAttribsARB(m_hdc, nullptr, attribs);
            if (modern_context) {
                wglMakeCurrent(nullptr, nullptr);
                wglDeleteContext(m_hglrc);
                m_hglrc = modern_context;
                wglMakeCurrent(m_hdc, m_hglrc);
                spdlog::info("Created OpenGL 4.5 core context");
            }
        }

        // Setup initial OpenGL state
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        return true;
    }

    void query_platform_info() {
        m_info.name = "Windows";

        // Get Windows version
        OSVERSIONINFOW osvi = {};
        osvi.dwOSVersionInfoSize = sizeof(osvi);
        // Note: GetVersionEx is deprecated but still works for basic info

        m_info.capabilities.has_window = true;
        m_info.capabilities.has_input = true;
        m_info.capabilities.has_gpu = (m_gpu_backend != void_render::GpuBackend::Null);
        m_info.capabilities.has_audio = true;
        m_info.capabilities.has_clipboard = true;
        m_info.capabilities.has_file_dialogs = true;
        m_info.capabilities.has_cursor_control = true;
        m_info.capabilities.has_fullscreen = true;
        m_info.capabilities.has_multi_monitor = true;
        m_info.capabilities.has_dpi_awareness = true;
        m_info.capabilities.has_gamepad = true;
        m_info.capabilities.gpu_backend = m_gpu_backend;
        m_info.capabilities.display_backend = void_render::DisplayBackend::Win32;

        // Get CPU info
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        m_info.cpu_cores = sysinfo.dwNumberOfProcessors;

        // Get memory info
        MEMORYSTATUSEX memstat = {};
        memstat.dwLength = sizeof(memstat);
        GlobalMemoryStatusEx(&memstat);
        m_info.system_memory_mb = static_cast<std::uint64_t>(memstat.ullTotalPhys / (1024 * 1024));
    }

    void poll_gamepads(const PlatformEventCallback& callback) {
        for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
            XINPUT_STATE state;
            if (XInputGetState(i, &state) == ERROR_SUCCESS) {
                if (!m_gamepad_connected[i]) {
                    m_gamepad_connected[i] = true;
                    PlatformEvent evt(PlatformEventType::GamepadConnect);
                    evt.data.gamepad_button.gamepad_id = i;
                    callback(evt);
                }

                // Compare with previous state and emit events
                // (simplified - full implementation would track all buttons/axes)
            } else {
                if (m_gamepad_connected[i]) {
                    m_gamepad_connected[i] = false;
                    PlatformEvent evt(PlatformEventType::GamepadDisconnect);
                    evt.data.gamepad_button.gamepad_id = i;
                    callback(evt);
                }
            }
        }
    }

    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        WindowedPlatformWin32* platform = nullptr;

        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
            platform = static_cast<WindowedPlatformWin32*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(platform));
        } else {
            platform = reinterpret_cast<WindowedPlatformWin32*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (platform) {
            return platform->handle_message(hwnd, msg, wparam, lparam);
        }

        return DefWindowProc(hwnd, msg, wparam, lparam);
    }

    LRESULT handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        PlatformEvent evt;
        evt.timestamp = get_time();

        switch (msg) {
            case WM_CLOSE:
                m_should_quit = true;
                evt.type = PlatformEventType::WindowClose;
                queue_event(evt);
                return 0;

            case WM_SIZE: {
                evt.type = PlatformEventType::WindowResize;
                evt.data.resize.width = LOWORD(lparam);
                evt.data.resize.height = HIWORD(lparam);

                if (wparam == SIZE_MINIMIZED) {
                    evt.type = PlatformEventType::WindowMinimize;
                } else if (wparam == SIZE_MAXIMIZED) {
                    evt.type = PlatformEventType::WindowMaximize;
                }
                queue_event(evt);
                break;
            }

            case WM_MOVE:
                evt.type = PlatformEventType::WindowMove;
                evt.data.position.x = GET_X_LPARAM(lparam);
                evt.data.position.y = GET_Y_LPARAM(lparam);
                queue_event(evt);
                break;

            case WM_SETFOCUS:
                evt.type = PlatformEventType::WindowFocus;
                queue_event(evt);
                break;

            case WM_KILLFOCUS:
                evt.type = PlatformEventType::WindowBlur;
                queue_event(evt);
                break;

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                evt.type = (lparam & 0x40000000) ? PlatformEventType::KeyRepeat : PlatformEventType::KeyDown;
                evt.data.key.key = static_cast<int>(wparam);
                evt.data.key.scancode = (lparam >> 16) & 0xFF;
                evt.data.key.mods = get_key_mods();
                evt.data.key.repeat = (lparam & 0x40000000) != 0;
                queue_event(evt);
                break;

            case WM_KEYUP:
            case WM_SYSKEYUP:
                evt.type = PlatformEventType::KeyUp;
                evt.data.key.key = static_cast<int>(wparam);
                evt.data.key.scancode = (lparam >> 16) & 0xFF;
                evt.data.key.mods = get_key_mods();
                evt.data.key.repeat = false;
                queue_event(evt);
                break;

            case WM_CHAR:
                evt.type = PlatformEventType::CharInput;
                evt.data.char_input.codepoint = static_cast<std::uint32_t>(wparam);
                queue_event(evt);
                break;

            case WM_MOUSEMOVE: {
                double x = GET_X_LPARAM(lparam);
                double y = GET_Y_LPARAM(lparam);
                evt.type = PlatformEventType::MouseMove;
                evt.data.mouse_move.x = x;
                evt.data.mouse_move.y = y;
                evt.data.mouse_move.dx = x - m_last_mouse_x;
                evt.data.mouse_move.dy = y - m_last_mouse_y;
                m_last_mouse_x = x;
                m_last_mouse_y = y;
                queue_event(evt);
                break;
            }

            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_XBUTTONDOWN:
                evt.type = PlatformEventType::MouseButton;
                evt.data.mouse_button.button = get_mouse_button(msg, wparam);
                evt.data.mouse_button.action = 1;  // Pressed
                evt.data.mouse_button.mods = get_key_mods();
                queue_event(evt);
                break;

            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
            case WM_XBUTTONUP:
                evt.type = PlatformEventType::MouseButton;
                evt.data.mouse_button.button = get_mouse_button(msg, wparam);
                evt.data.mouse_button.action = 0;  // Released
                evt.data.mouse_button.mods = get_key_mods();
                queue_event(evt);
                break;

            case WM_MOUSEWHEEL:
                evt.type = PlatformEventType::MouseScroll;
                evt.data.scroll.x_offset = 0;
                evt.data.scroll.y_offset = GET_WHEEL_DELTA_WPARAM(wparam) / 120.0;
                queue_event(evt);
                break;

            case WM_MOUSEHWHEEL:
                evt.type = PlatformEventType::MouseScroll;
                evt.data.scroll.x_offset = GET_WHEEL_DELTA_WPARAM(wparam) / 120.0;
                evt.data.scroll.y_offset = 0;
                queue_event(evt);
                break;

            case WM_DROPFILES: {
                HDROP hdrop = reinterpret_cast<HDROP>(wparam);
                UINT count = DragQueryFileW(hdrop, 0xFFFFFFFF, nullptr, 0);

                evt.type = PlatformEventType::WindowDrop;
                for (UINT i = 0; i < count; ++i) {
                    UINT size = DragQueryFileW(hdrop, i, nullptr, 0) + 1;
                    std::wstring path(size, 0);
                    DragQueryFileW(hdrop, i, path.data(), size);

                    int utf8_size = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    std::string utf8_path(utf8_size - 1, 0);
                    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, utf8_path.data(), utf8_size, nullptr, nullptr);
                    evt.dropped_files.push_back(utf8_path);
                }
                DragFinish(hdrop);
                queue_event(evt);
                break;
            }

            case WM_DPICHANGED: {
                RECT* suggested = reinterpret_cast<RECT*>(lparam);
                SetWindowPos(hwnd, nullptr,
                    suggested->left, suggested->top,
                    suggested->right - suggested->left,
                    suggested->bottom - suggested->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);

                evt.type = PlatformEventType::ContentScaleChange;
                evt.data.content_scale.x_scale = HIWORD(wparam) / 96.0f;
                evt.data.content_scale.y_scale = LOWORD(wparam) / 96.0f;
                queue_event(evt);
                break;
            }

            case WM_ERASEBKGND:
                return 1;

            case WM_SETCURSOR:
                if (LOWORD(lparam) == HTCLIENT && !m_cursor_visible) {
                    SetCursor(nullptr);
                    return TRUE;
                }
                break;
        }

        return DefWindowProc(hwnd, msg, wparam, lparam);
    }

    void queue_event(const PlatformEvent& evt) {
        std::lock_guard<std::mutex> lock(m_event_mutex);
        m_event_queue.push(evt);
    }

    static int get_key_mods() {
        int mods = 0;
        if (GetKeyState(VK_SHIFT) & 0x8000) mods |= 0x0001;
        if (GetKeyState(VK_CONTROL) & 0x8000) mods |= 0x0002;
        if (GetKeyState(VK_MENU) & 0x8000) mods |= 0x0004;
        if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000) mods |= 0x0008;
        if (GetKeyState(VK_CAPITAL) & 0x0001) mods |= 0x0010;
        if (GetKeyState(VK_NUMLOCK) & 0x0001) mods |= 0x0020;
        return mods;
    }

    static int get_mouse_button(UINT msg, WPARAM wparam) {
        switch (msg) {
            case WM_LBUTTONDOWN: case WM_LBUTTONUP: return 0;
            case WM_RBUTTONDOWN: case WM_RBUTTONUP: return 1;
            case WM_MBUTTONDOWN: case WM_MBUTTONUP: return 2;
            case WM_XBUTTONDOWN: case WM_XBUTTONUP:
                return (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) ? 3 : 4;
        }
        return 0;
    }

    // Window state
    HWND m_hwnd{nullptr};
    HDC m_hdc{nullptr};
    HGLRC m_hglrc{nullptr};
    PlatformWindowConfig m_config;

    // Fullscreen state
    bool m_fullscreen{false};
    WINDOWPLACEMENT m_prev_placement{sizeof(WINDOWPLACEMENT)};
    LONG m_prev_style{0};

    // Cursor state
    bool m_cursor_visible{true};
    bool m_cursor_captured{false};
    double m_last_mouse_x{0};
    double m_last_mouse_y{0};

    // Gamepad state
    bool m_gamepad_connected[XUSER_MAX_COUNT]{};

    // Platform info
    PlatformInfo m_info;
    void_render::GpuBackend m_gpu_backend{void_render::GpuBackend::Null};

    // State
    bool m_initialized{false};
    bool m_should_quit{false};

    // Timing
    std::chrono::high_resolution_clock::time_point m_start_time;
    double m_time_offset{0};

    // Event queue
    std::queue<PlatformEvent> m_event_queue;
    std::mutex m_event_mutex;
};

#endif // _WIN32

// =============================================================================
// WindowedPlatform Implementation (X11/Linux)
// =============================================================================

#ifndef _WIN32

/// Windowed platform for desktop (X11)
class WindowedPlatformX11 : public IPlatform {
public:
    WindowedPlatformX11() = default;
    ~WindowedPlatformX11() override { shutdown(); }

    void_core::Result<void> initialize(
        const PlatformWindowConfig& window_config,
        const PlatformGpuConfig& /*gpu_config*/) override {

        m_display = XOpenDisplay(nullptr);
        if (!m_display) {
            return void_core::Error("Failed to open X display");
        }

        int screen = DefaultScreen(m_display);
        ::Window root = RootWindow(m_display, screen);

        XSetWindowAttributes attrs = {};
        attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                          ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                          StructureNotifyMask | FocusChangeMask | EnterWindowMask |
                          LeaveWindowMask;
        attrs.background_pixel = BlackPixel(m_display, screen);

        int x = window_config.x >= 0 ? window_config.x : 0;
        int y = window_config.y >= 0 ? window_config.y : 0;

        m_window = XCreateWindow(
            m_display, root,
            x, y,
            window_config.width, window_config.height,
            0,
            CopyFromParent, InputOutput, CopyFromParent,
            CWEventMask | CWBackPixel, &attrs
        );

        if (!m_window) {
            XCloseDisplay(m_display);
            m_display = nullptr;
            return void_core::Error("Failed to create X window");
        }

        // Set window title
        XStoreName(m_display, m_window, window_config.title.c_str());

        // Set WM_DELETE_WINDOW protocol
        m_wm_delete_window = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(m_display, m_window, &m_wm_delete_window, 1);

        // Show window
        if (window_config.visible) {
            XMapWindow(m_display, m_window);
        }

        // Center if requested
        if (window_config.x < 0 || window_config.y < 0) {
            int screen_width = DisplayWidth(m_display, screen);
            int screen_height = DisplayHeight(m_display, screen);
            x = (screen_width - window_config.width) / 2;
            y = (screen_height - window_config.height) / 2;
            XMoveWindow(m_display, m_window, x, y);
        }

        // Query platform info
        m_info.name = "Linux/X11";
        m_info.capabilities.has_window = true;
        m_info.capabilities.has_input = true;
        m_info.capabilities.has_cursor_control = true;
        m_info.capabilities.has_fullscreen = true;
        m_info.capabilities.display_backend = void_render::DisplayBackend::X11;

        gettimeofday(&m_start_time, nullptr);
        m_initialized = true;

        spdlog::info("WindowedPlatformX11 initialized ({}x{})",
                     window_config.width, window_config.height);

        return void_core::Ok();
    }

    void shutdown() override {
        if (!m_initialized) return;

        if (m_window) {
            XDestroyWindow(m_display, m_window);
            m_window = 0;
        }
        if (m_display) {
            XCloseDisplay(m_display);
            m_display = nullptr;
        }

        m_initialized = false;
        spdlog::info("WindowedPlatformX11 shutdown");
    }

    [[nodiscard]] bool is_initialized() const override { return m_initialized; }

    void poll_events(const PlatformEventCallback& callback) override {
        while (XPending(m_display)) {
            XEvent xevent;
            XNextEvent(m_display, &xevent);

            PlatformEvent evt;
            evt.timestamp = get_time();

            switch (xevent.type) {
                case ClientMessage:
                    if (static_cast<Atom>(xevent.xclient.data.l[0]) == m_wm_delete_window) {
                        m_should_quit = true;
                        evt.type = PlatformEventType::WindowClose;
                        callback(evt);
                    }
                    break;

                case ConfigureNotify:
                    evt.type = PlatformEventType::WindowResize;
                    evt.data.resize.width = xevent.xconfigure.width;
                    evt.data.resize.height = xevent.xconfigure.height;
                    callback(evt);
                    break;

                case FocusIn:
                    evt.type = PlatformEventType::WindowFocus;
                    callback(evt);
                    break;

                case FocusOut:
                    evt.type = PlatformEventType::WindowBlur;
                    callback(evt);
                    break;

                case KeyPress:
                    evt.type = PlatformEventType::KeyDown;
                    evt.data.key.key = XLookupKeysym(&xevent.xkey, 0);
                    evt.data.key.scancode = xevent.xkey.keycode;
                    evt.data.key.mods = xevent.xkey.state;
                    callback(evt);
                    break;

                case KeyRelease:
                    evt.type = PlatformEventType::KeyUp;
                    evt.data.key.key = XLookupKeysym(&xevent.xkey, 0);
                    evt.data.key.scancode = xevent.xkey.keycode;
                    evt.data.key.mods = xevent.xkey.state;
                    callback(evt);
                    break;

                case MotionNotify:
                    evt.type = PlatformEventType::MouseMove;
                    evt.data.mouse_move.x = xevent.xmotion.x;
                    evt.data.mouse_move.y = xevent.xmotion.y;
                    callback(evt);
                    break;

                case ButtonPress:
                case ButtonRelease:
                    if (xevent.xbutton.button <= 3) {
                        evt.type = PlatformEventType::MouseButton;
                        evt.data.mouse_button.button = xevent.xbutton.button - 1;
                        evt.data.mouse_button.action = (xevent.type == ButtonPress) ? 1 : 0;
                        callback(evt);
                    } else if (xevent.xbutton.button == 4 || xevent.xbutton.button == 5) {
                        // Scroll
                        evt.type = PlatformEventType::MouseScroll;
                        evt.data.scroll.y_offset = (xevent.xbutton.button == 4) ? 1.0 : -1.0;
                        callback(evt);
                    }
                    break;

                case EnterNotify:
                    evt.type = PlatformEventType::MouseEnter;
                    callback(evt);
                    break;

                case LeaveNotify:
                    evt.type = PlatformEventType::MouseLeave;
                    callback(evt);
                    break;
            }
        }
    }

    void wait_events(std::chrono::milliseconds timeout) override {
        if (timeout.count() == 0) {
            XEvent event;
            XPeekEvent(m_display, &event);
        } else {
            usleep(timeout.count() * 1000);
        }
    }

    void request_quit() override { m_should_quit = true; }
    [[nodiscard]] bool should_quit() const override { return m_should_quit; }

    void get_window_size(std::uint32_t& width, std::uint32_t& height) const override {
        XWindowAttributes attrs;
        XGetWindowAttributes(m_display, m_window, &attrs);
        width = attrs.width;
        height = attrs.height;
    }

    void set_window_size(std::uint32_t width, std::uint32_t height) override {
        XResizeWindow(m_display, m_window, width, height);
    }

    void get_framebuffer_size(std::uint32_t& width, std::uint32_t& height) const override {
        get_window_size(width, height);
    }

    void get_window_position(std::int32_t& x, std::int32_t& y) const override {
        XWindowAttributes attrs;
        XGetWindowAttributes(m_display, m_window, &attrs);
        x = attrs.x;
        y = attrs.y;
    }

    void set_window_position(std::int32_t x, std::int32_t y) override {
        XMoveWindow(m_display, m_window, x, y);
    }

    void set_window_title(const std::string& title) override {
        XStoreName(m_display, m_window, title.c_str());
    }

    void set_fullscreen(bool /*fullscreen*/, std::uint32_t /*monitor*/) override {
        // X11 fullscreen via _NET_WM_STATE_FULLSCREEN (simplified)
    }

    [[nodiscard]] bool is_fullscreen() const override { return false; }
    void minimize_window() override { XIconifyWindow(m_display, m_window, DefaultScreen(m_display)); }
    void maximize_window() override {}
    void restore_window() override { XMapWindow(m_display, m_window); }
    void focus_window() override { XSetInputFocus(m_display, m_window, RevertToParent, CurrentTime); }
    [[nodiscard]] bool is_window_focused() const override { return true; }

    void get_content_scale(float& x_scale, float& y_scale) const override {
        x_scale = y_scale = 1.0f;
    }

    [[nodiscard]] void* native_window_handle() const override {
        return reinterpret_cast<void*>(m_window);
    }

    void set_cursor_visible(bool /*visible*/) override {}
    void set_cursor_captured(bool /*captured*/) override {}

    void get_cursor_position(double& x, double& y) const override {
        ::Window root, child;
        int root_x, root_y, win_x, win_y;
        unsigned int mask;
        XQueryPointer(m_display, m_window, &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);
        x = win_x;
        y = win_y;
    }

    void set_cursor_position(double x, double y) override {
        XWarpPointer(m_display, None, m_window, 0, 0, 0, 0, static_cast<int>(x), static_cast<int>(y));
    }

    void begin_frame() override {}
    void end_frame() override {}

    [[nodiscard]] void_render::GpuBackend gpu_backend() const override {
        return void_render::GpuBackend::Null;
    }

    [[nodiscard]] void* native_gpu_context() const override { return nullptr; }

    [[nodiscard]] const PlatformInfo& info() const override { return m_info; }
    [[nodiscard]] const PlatformCapabilities& capabilities() const override {
        return m_info.capabilities;
    }

    [[nodiscard]] double get_time() const override {
        struct timeval now;
        gettimeofday(&now, nullptr);
        return (now.tv_sec - m_start_time.tv_sec) + (now.tv_usec - m_start_time.tv_usec) / 1000000.0;
    }

    void set_time(double /*time*/) override {}

private:
    Display* m_display{nullptr};
    ::Window m_window{0};
    Atom m_wm_delete_window{0};
    PlatformInfo m_info;
    bool m_initialized{false};
    bool m_should_quit{false};
    struct timeval m_start_time{};
};

#endif // !_WIN32

// =============================================================================
// Platform Factory
// =============================================================================

std::unique_ptr<IPlatform> create_platform(const RuntimeConfig& config) {
    switch (config.mode) {
        case RuntimeMode::Headless:
            return std::make_unique<HeadlessPlatform>();

        case RuntimeMode::Windowed:
        case RuntimeMode::Editor:
#ifdef _WIN32
            return std::make_unique<WindowedPlatformWin32>();
#else
            return std::make_unique<WindowedPlatformX11>();
#endif

        case RuntimeMode::XR:
            // XR platform would go here
            spdlog::warn("XR platform not yet implemented, falling back to windowed");
#ifdef _WIN32
            return std::make_unique<WindowedPlatformWin32>();
#else
            return std::make_unique<WindowedPlatformX11>();
#endif
    }

    return std::make_unique<HeadlessPlatform>();
}

std::vector<void_render::GpuBackend> enumerate_gpu_backends() {
    std::vector<void_render::GpuBackend> backends;

#ifdef _WIN32
    backends.push_back(void_render::GpuBackend::OpenGL);
    // Could probe for Vulkan/D3D12 support here
#else
    backends.push_back(void_render::GpuBackend::OpenGL);
#endif

    backends.push_back(void_render::GpuBackend::Null);
    return backends;
}

std::vector<void_render::DisplayBackend> enumerate_display_backends() {
    std::vector<void_render::DisplayBackend> backends;

#ifdef _WIN32
    backends.push_back(void_render::DisplayBackend::Win32);
#else
    backends.push_back(void_render::DisplayBackend::X11);
    // Could probe for Wayland support
#endif

    backends.push_back(void_render::DisplayBackend::Headless);
    return backends;
}

PlatformCapabilities query_platform_capabilities() {
    PlatformCapabilities caps;

#ifdef _WIN32
    caps.has_window = true;
    caps.has_input = true;
    caps.has_gpu = true;
    caps.has_audio = true;
    caps.has_clipboard = true;
    caps.has_file_dialogs = true;
    caps.has_cursor_control = true;
    caps.has_fullscreen = true;
    caps.has_multi_monitor = true;
    caps.has_dpi_awareness = true;
    caps.has_gamepad = true;
    caps.display_backend = void_render::DisplayBackend::Win32;
    caps.gpu_backend = void_render::GpuBackend::OpenGL;
#else
    caps.has_window = true;
    caps.has_input = true;
    caps.has_cursor_control = true;
    caps.has_fullscreen = true;
    caps.display_backend = void_render::DisplayBackend::X11;
#endif

    return caps;
}

} // namespace void_runtime
