#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_compositor

#include <cstdint>

namespace void_compositor {

// VRR types
enum class VrrMode : std::uint8_t;
struct VrrConfig;
struct VrrCapability;

// HDR types
enum class TransferFunction : std::uint8_t;
enum class ColorPrimaries : std::uint8_t;
struct HdrConfig;
struct HdrCapability;
struct HdrMetadata;

// Frame types
enum class FrameState : std::uint8_t;
struct PresentationFeedback;
class FrameScheduler;

// Input types
enum class KeyState : std::uint8_t;
enum class ButtonState : std::uint8_t;
enum class AxisSource : std::uint8_t;
enum class DeviceType : std::uint8_t;
struct Modifiers;
struct KeyboardEvent;
// Note: PointerEvent, TouchEvent, DeviceEvent, InputEvent are type aliases, not structs
// They cannot be forward-declared - include input.hpp for full definitions
class InputState;

// Output types
struct OutputMode;
enum class OutputTransform : std::uint8_t;
struct OutputInfo;
class IOutput;

// Compositor types
enum class RenderFormat : std::uint8_t;
struct CompositorConfig;
struct CompositorCapabilities;
class IRenderTarget;
class ICompositor;
class CompositorFactory;

} // namespace void_compositor
