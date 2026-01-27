/// @file types.cpp
/// @brief Type implementations for void_input

#include <void_engine/input/types.hpp>
#include <algorithm>
#include <cctype>

namespace void_input {

// =============================================================================
// Key Code String Conversion
// =============================================================================

const char* to_string(KeyCode key) {
    switch (key) {
        case KeyCode::Unknown: return "Unknown";
        case KeyCode::Space: return "Space";
        case KeyCode::Apostrophe: return "Apostrophe";
        case KeyCode::Comma: return "Comma";
        case KeyCode::Minus: return "Minus";
        case KeyCode::Period: return "Period";
        case KeyCode::Slash: return "Slash";
        case KeyCode::Num0: return "0";
        case KeyCode::Num1: return "1";
        case KeyCode::Num2: return "2";
        case KeyCode::Num3: return "3";
        case KeyCode::Num4: return "4";
        case KeyCode::Num5: return "5";
        case KeyCode::Num6: return "6";
        case KeyCode::Num7: return "7";
        case KeyCode::Num8: return "8";
        case KeyCode::Num9: return "9";
        case KeyCode::Semicolon: return "Semicolon";
        case KeyCode::Equal: return "Equal";
        case KeyCode::A: return "A";
        case KeyCode::B: return "B";
        case KeyCode::C: return "C";
        case KeyCode::D: return "D";
        case KeyCode::E: return "E";
        case KeyCode::F: return "F";
        case KeyCode::G: return "G";
        case KeyCode::H: return "H";
        case KeyCode::I: return "I";
        case KeyCode::J: return "J";
        case KeyCode::K: return "K";
        case KeyCode::L: return "L";
        case KeyCode::M: return "M";
        case KeyCode::N: return "N";
        case KeyCode::O: return "O";
        case KeyCode::P: return "P";
        case KeyCode::Q: return "Q";
        case KeyCode::R: return "R";
        case KeyCode::S: return "S";
        case KeyCode::T: return "T";
        case KeyCode::U: return "U";
        case KeyCode::V: return "V";
        case KeyCode::W: return "W";
        case KeyCode::X: return "X";
        case KeyCode::Y: return "Y";
        case KeyCode::Z: return "Z";
        case KeyCode::LeftBracket: return "LeftBracket";
        case KeyCode::Backslash: return "Backslash";
        case KeyCode::RightBracket: return "RightBracket";
        case KeyCode::GraveAccent: return "GraveAccent";
        case KeyCode::Escape: return "Escape";
        case KeyCode::Enter: return "Enter";
        case KeyCode::Tab: return "Tab";
        case KeyCode::Backspace: return "Backspace";
        case KeyCode::Insert: return "Insert";
        case KeyCode::Delete: return "Delete";
        case KeyCode::Right: return "Right";
        case KeyCode::Left: return "Left";
        case KeyCode::Down: return "Down";
        case KeyCode::Up: return "Up";
        case KeyCode::PageUp: return "PageUp";
        case KeyCode::PageDown: return "PageDown";
        case KeyCode::Home: return "Home";
        case KeyCode::End: return "End";
        case KeyCode::CapsLock: return "CapsLock";
        case KeyCode::ScrollLock: return "ScrollLock";
        case KeyCode::NumLock: return "NumLock";
        case KeyCode::PrintScreen: return "PrintScreen";
        case KeyCode::Pause: return "Pause";
        case KeyCode::F1: return "F1";
        case KeyCode::F2: return "F2";
        case KeyCode::F3: return "F3";
        case KeyCode::F4: return "F4";
        case KeyCode::F5: return "F5";
        case KeyCode::F6: return "F6";
        case KeyCode::F7: return "F7";
        case KeyCode::F8: return "F8";
        case KeyCode::F9: return "F9";
        case KeyCode::F10: return "F10";
        case KeyCode::F11: return "F11";
        case KeyCode::F12: return "F12";
        case KeyCode::Kp0: return "Keypad0";
        case KeyCode::Kp1: return "Keypad1";
        case KeyCode::Kp2: return "Keypad2";
        case KeyCode::Kp3: return "Keypad3";
        case KeyCode::Kp4: return "Keypad4";
        case KeyCode::Kp5: return "Keypad5";
        case KeyCode::Kp6: return "Keypad6";
        case KeyCode::Kp7: return "Keypad7";
        case KeyCode::Kp8: return "Keypad8";
        case KeyCode::Kp9: return "Keypad9";
        case KeyCode::KpDecimal: return "KeypadDecimal";
        case KeyCode::KpDivide: return "KeypadDivide";
        case KeyCode::KpMultiply: return "KeypadMultiply";
        case KeyCode::KpSubtract: return "KeypadSubtract";
        case KeyCode::KpAdd: return "KeypadAdd";
        case KeyCode::KpEnter: return "KeypadEnter";
        case KeyCode::KpEqual: return "KeypadEqual";
        case KeyCode::LeftShift: return "LeftShift";
        case KeyCode::LeftControl: return "LeftControl";
        case KeyCode::LeftAlt: return "LeftAlt";
        case KeyCode::LeftSuper: return "LeftSuper";
        case KeyCode::RightShift: return "RightShift";
        case KeyCode::RightControl: return "RightControl";
        case KeyCode::RightAlt: return "RightAlt";
        case KeyCode::RightSuper: return "RightSuper";
        case KeyCode::Menu: return "Menu";
        default: return "Unknown";
    }
}

const char* to_string(MouseButton button) {
    switch (button) {
        case MouseButton::Left: return "MouseLeft";
        case MouseButton::Right: return "MouseRight";
        case MouseButton::Middle: return "MouseMiddle";
        case MouseButton::Button4: return "Mouse4";
        case MouseButton::Button5: return "Mouse5";
        case MouseButton::Button6: return "Mouse6";
        case MouseButton::Button7: return "Mouse7";
        case MouseButton::Button8: return "Mouse8";
        default: return "MouseUnknown";
    }
}

const char* to_string(GamepadButton button) {
    switch (button) {
        case GamepadButton::A: return "GamepadA";
        case GamepadButton::B: return "GamepadB";
        case GamepadButton::X: return "GamepadX";
        case GamepadButton::Y: return "GamepadY";
        case GamepadButton::LeftBumper: return "GamepadLB";
        case GamepadButton::RightBumper: return "GamepadRB";
        case GamepadButton::Back: return "GamepadBack";
        case GamepadButton::Start: return "GamepadStart";
        case GamepadButton::Guide: return "GamepadGuide";
        case GamepadButton::LeftThumb: return "GamepadLS";
        case GamepadButton::RightThumb: return "GamepadRS";
        case GamepadButton::DpadUp: return "GamepadDPadUp";
        case GamepadButton::DpadRight: return "GamepadDPadRight";
        case GamepadButton::DpadDown: return "GamepadDPadDown";
        case GamepadButton::DpadLeft: return "GamepadDPadLeft";
        default: return "GamepadUnknown";
    }
}

const char* to_string(GamepadAxis axis) {
    switch (axis) {
        case GamepadAxis::LeftX: return "GamepadLeftX";
        case GamepadAxis::LeftY: return "GamepadLeftY";
        case GamepadAxis::RightX: return "GamepadRightX";
        case GamepadAxis::RightY: return "GamepadRightY";
        case GamepadAxis::LeftTrigger: return "GamepadLT";
        case GamepadAxis::RightTrigger: return "GamepadRT";
        default: return "GamepadAxisUnknown";
    }
}

const char* to_string(ActionType type) {
    switch (type) {
        case ActionType::Button: return "Button";
        case ActionType::Axis1D: return "Axis1D";
        case ActionType::Axis2D: return "Axis2D";
        case ActionType::Axis3D: return "Axis3D";
        default: return "Unknown";
    }
}

// =============================================================================
// String to Key Code
// =============================================================================

KeyCode key_from_string(const std::string& name) {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    // Single characters
    if (upper.length() == 1) {
        char c = upper[0];
        if (c >= 'A' && c <= 'Z') {
            return static_cast<KeyCode>(static_cast<int>(KeyCode::A) + (c - 'A'));
        }
        if (c >= '0' && c <= '9') {
            return static_cast<KeyCode>(static_cast<int>(KeyCode::Num0) + (c - '0'));
        }
    }

    // Named keys
    if (upper == "SPACE") return KeyCode::Space;
    if (upper == "ESCAPE" || upper == "ESC") return KeyCode::Escape;
    if (upper == "ENTER" || upper == "RETURN") return KeyCode::Enter;
    if (upper == "TAB") return KeyCode::Tab;
    if (upper == "BACKSPACE") return KeyCode::Backspace;
    if (upper == "INSERT") return KeyCode::Insert;
    if (upper == "DELETE" || upper == "DEL") return KeyCode::Delete;
    if (upper == "RIGHT") return KeyCode::Right;
    if (upper == "LEFT") return KeyCode::Left;
    if (upper == "DOWN") return KeyCode::Down;
    if (upper == "UP") return KeyCode::Up;
    if (upper == "PAGEUP") return KeyCode::PageUp;
    if (upper == "PAGEDOWN") return KeyCode::PageDown;
    if (upper == "HOME") return KeyCode::Home;
    if (upper == "END") return KeyCode::End;
    if (upper == "CAPSLOCK") return KeyCode::CapsLock;
    if (upper == "SCROLLLOCK") return KeyCode::ScrollLock;
    if (upper == "NUMLOCK") return KeyCode::NumLock;
    if (upper == "PRINTSCREEN") return KeyCode::PrintScreen;
    if (upper == "PAUSE") return KeyCode::Pause;

    // Function keys
    if (upper == "F1") return KeyCode::F1;
    if (upper == "F2") return KeyCode::F2;
    if (upper == "F3") return KeyCode::F3;
    if (upper == "F4") return KeyCode::F4;
    if (upper == "F5") return KeyCode::F5;
    if (upper == "F6") return KeyCode::F6;
    if (upper == "F7") return KeyCode::F7;
    if (upper == "F8") return KeyCode::F8;
    if (upper == "F9") return KeyCode::F9;
    if (upper == "F10") return KeyCode::F10;
    if (upper == "F11") return KeyCode::F11;
    if (upper == "F12") return KeyCode::F12;

    // Modifiers
    if (upper == "LSHIFT" || upper == "LEFTSHIFT") return KeyCode::LeftShift;
    if (upper == "RSHIFT" || upper == "RIGHTSHIFT") return KeyCode::RightShift;
    if (upper == "LCTRL" || upper == "LEFTCONTROL" || upper == "LEFTCTRL") return KeyCode::LeftControl;
    if (upper == "RCTRL" || upper == "RIGHTCONTROL" || upper == "RIGHTCTRL") return KeyCode::RightControl;
    if (upper == "LALT" || upper == "LEFTALT") return KeyCode::LeftAlt;
    if (upper == "RALT" || upper == "RIGHTALT") return KeyCode::RightAlt;

    return KeyCode::Unknown;
}

MouseButton mouse_button_from_string(const std::string& name) {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (upper == "LEFT" || upper == "MOUSELEFT" || upper == "MOUSE1") return MouseButton::Left;
    if (upper == "RIGHT" || upper == "MOUSERIGHT" || upper == "MOUSE2") return MouseButton::Right;
    if (upper == "MIDDLE" || upper == "MOUSEMIDDLE" || upper == "MOUSE3") return MouseButton::Middle;
    if (upper == "MOUSE4") return MouseButton::Button4;
    if (upper == "MOUSE5") return MouseButton::Button5;

    return MouseButton::Left;
}

GamepadButton gamepad_button_from_string(const std::string& name) {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (upper == "A" || upper == "CROSS") return GamepadButton::A;
    if (upper == "B" || upper == "CIRCLE") return GamepadButton::B;
    if (upper == "X" || upper == "SQUARE") return GamepadButton::X;
    if (upper == "Y" || upper == "TRIANGLE") return GamepadButton::Y;
    if (upper == "LB" || upper == "L1" || upper == "LEFTBUMPER") return GamepadButton::LeftBumper;
    if (upper == "RB" || upper == "R1" || upper == "RIGHTBUMPER") return GamepadButton::RightBumper;
    if (upper == "BACK" || upper == "SELECT" || upper == "SHARE") return GamepadButton::Back;
    if (upper == "START" || upper == "OPTIONS") return GamepadButton::Start;
    if (upper == "GUIDE" || upper == "HOME" || upper == "PS") return GamepadButton::Guide;
    if (upper == "LS" || upper == "L3" || upper == "LEFTTHUMB") return GamepadButton::LeftThumb;
    if (upper == "RS" || upper == "R3" || upper == "RIGHTTHUMB") return GamepadButton::RightThumb;
    if (upper == "DPADUP") return GamepadButton::DpadUp;
    if (upper == "DPADDOWN") return GamepadButton::DpadDown;
    if (upper == "DPADLEFT") return GamepadButton::DpadLeft;
    if (upper == "DPADRIGHT") return GamepadButton::DpadRight;

    return GamepadButton::A;
}

} // namespace void_input
