/// @file fwd.hpp
/// @brief Forward declarations for void_runtime

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace void_runtime {

// Forward declarations
class Application;
class ApplicationConfig;
class Bootstrap;
class Window;
class WindowConfig;
class InputManager;
class SceneLoader;
class CrashHandler;
class RuntimeStats;

// Handle types
using WindowId = std::uint32_t;
using InputDeviceId = std::uint32_t;

// Callbacks
using UpdateCallback = std::function<void(double delta_time)>;
using FixedUpdateCallback = std::function<void(double fixed_time)>;
using RenderCallback = std::function<void()>;
using WindowEventCallback = std::function<void(WindowId, const struct WindowEvent&)>;
using InputEventCallback = std::function<void(const struct InputEvent&)>;

// Enumerations
enum class WindowState {
    Normal,
    Minimized,
    Maximized,
    Fullscreen,
    FullscreenBorderless
};

enum class CursorMode {
    Normal,
    Hidden,
    Disabled,
    Captured
};

enum class KeyState {
    Released,
    Pressed,
    Held
};

enum class MouseButton {
    Left,
    Right,
    Middle,
    Button4,
    Button5
};

enum class GamepadButton {
    A,
    B,
    X,
    Y,
    LeftBumper,
    RightBumper,
    Back,
    Start,
    Guide,
    LeftThumb,
    RightThumb,
    DPadUp,
    DPadRight,
    DPadDown,
    DPadLeft
};

enum class GamepadAxis {
    LeftX,
    LeftY,
    RightX,
    RightY,
    LeftTrigger,
    RightTrigger
};

// Error enumeration
enum class RuntimeError {
    None,
    WindowCreationFailed,
    GraphicsInitFailed,
    AudioInitFailed,
    SceneLoadFailed,
    ModuleLoadFailed,
    OutOfMemory,
    InvalidConfig,
    PlatformError
};

} // namespace void_runtime
