#pragma once

/// @file compositor_module.hpp
/// @brief Main include header for void_compositor
///
/// void_compositor provides frame scheduling and display management:
///
/// ## Features
///
/// - **Frame Scheduling**
///   - Target framerate control
///   - Frame budget management
///   - Presentation feedback
///   - Statistics (P50, P95, P99)
///
/// - **VRR (Variable Refresh Rate)**
///   - FreeSync/G-Sync/AdaptiveSync support
///   - Content velocity-based adaptation
///   - Power-saving modes
///
/// - **HDR (High Dynamic Range)**
///   - HDR10 (PQ) support
///   - HLG (Hybrid Log-Gamma) support
///   - Wide color gamut (Rec.2020, DCI-P3)
///   - Dynamic metadata
///
/// - **Input Handling**
///   - Keyboard events
///   - Pointer (mouse) events
///   - Touch events
///   - Device hotplug
///
/// - **Multi-Display**
///   - Output enumeration
///   - Mode switching
///   - Transform and scale
///
/// ## Quick Start
///
/// ```cpp
/// #include <void_engine/compositor/compositor_module.hpp>
///
/// using namespace void_compositor;
///
/// // Create compositor
/// CompositorConfig config;
/// config.target_fps = 60;
/// config.enable_vrr = true;
/// config.enable_hdr = true;
///
/// auto compositor = CompositorFactory::create(config);
/// if (!compositor) {
///     // Fallback to null compositor for testing
///     compositor = CompositorFactory::create_null(config);
/// }
///
/// // Main loop
/// while (compositor->is_running()) {
///     // Dispatch events
///     compositor->dispatch();
///
///     // Handle input
///     for (const auto& event : compositor->poll_input()) {
///         handle_input(event);
///     }
///
///     // Render if ready
///     if (compositor->should_render()) {
///         if (auto target = compositor->begin_frame()) {
///             // Render to target...
///             render(target.get());
///
///             // Present
///             compositor->end_frame(std::move(target));
///         }
///     }
/// }
/// ```
///
/// ## VRR Usage
///
/// ```cpp
/// // Check VRR capability
/// if (auto cap = compositor->vrr_capability(); cap && cap->supported) {
///     std::cout << "VRR supported: " << cap->range_string() << "\n";
///
///     // Enable automatic VRR
///     compositor->enable_vrr(VrrMode::Auto);
///
///     // Update content velocity each frame (0.0 = static, 1.0 = fast motion)
///     compositor->update_content_velocity(0.5f);
/// }
/// ```
///
/// ## HDR Usage
///
/// ```cpp
/// // Check HDR capability
/// if (auto cap = compositor->hdr_capability(); cap && cap->supported) {
///     std::cout << "HDR supported, max " << cap->max_luminance.value_or(0) << " nits\n";
///
///     // Enable HDR10
///     compositor->enable_hdr(HdrConfig::hdr10(1000));
///
///     // Or HLG for broadcast content
///     compositor->enable_hdr(HdrConfig::hlg(600));
/// }
/// ```
///
/// ## Frame Statistics
///
/// ```cpp
/// auto& scheduler = compositor->frame_scheduler();
///
/// // Current FPS
/// double fps = scheduler.current_fps();
///
/// // Frame time percentiles
/// auto p50 = scheduler.frame_time_p50();  // Median
/// auto p95 = scheduler.frame_time_p95();  // 95th percentile
/// auto p99 = scheduler.frame_time_p99();  // 99th percentile (smoothness indicator)
///
/// // Check if hitting target
/// if (!scheduler.hitting_target()) {
///     std::cout << "Frame rate below target!\n";
/// }
/// ```

// Core types
#include "fwd.hpp"
#include "types.hpp"

// VRR and HDR
#include "vrr.hpp"
#include "hdr.hpp"

// Frame scheduling
#include "frame.hpp"

// Input handling
#include "input.hpp"

// Output management
#include "output.hpp"

// Main compositor
#include "compositor.hpp"

namespace void_compositor {

/// Prelude - commonly used types for convenience
namespace prelude {
    // Types
    using void_compositor::RenderFormat;
    using void_compositor::CompositorConfig;
    using void_compositor::CompositorCapabilities;
    using void_compositor::CompositorError;
    using void_compositor::OutputTransform;

    // VRR
    using void_compositor::VrrMode;
    using void_compositor::VrrConfig;
    using void_compositor::VrrCapability;

    // HDR
    using void_compositor::TransferFunction;
    using void_compositor::ColorPrimaries;
    using void_compositor::HdrConfig;
    using void_compositor::HdrCapability;
    using void_compositor::HdrMetadata;
    using void_compositor::CieXyCoordinates;

    // Frame
    using void_compositor::FrameState;
    using void_compositor::PresentationFeedback;
    using void_compositor::FrameScheduler;

    // Input
    using void_compositor::Vec2;
    using void_compositor::KeyState;
    using void_compositor::ButtonState;
    using void_compositor::Modifiers;
    using void_compositor::KeyboardEvent;
    using void_compositor::PointerButton;
    using void_compositor::AxisSource;
    using void_compositor::PointerMotionEvent;
    using void_compositor::PointerButtonEvent;
    using void_compositor::PointerAxisEvent;
    using void_compositor::PointerEvent;
    using void_compositor::TouchDownEvent;
    using void_compositor::TouchMotionEvent;
    using void_compositor::TouchUpEvent;
    using void_compositor::TouchCancelEvent;
    using void_compositor::TouchEvent;
    using void_compositor::DeviceType;
    using void_compositor::DeviceAddedEvent;
    using void_compositor::DeviceRemovedEvent;
    using void_compositor::DeviceEvent;
    using void_compositor::InputEvent;
    using void_compositor::InputState;

    // Output
    using void_compositor::OutputMode;
    using void_compositor::OutputInfo;
    using void_compositor::IOutput;
    using void_compositor::NullOutput;

    // Compositor
    using void_compositor::IRenderTarget;
    using void_compositor::NullRenderTarget;
    using void_compositor::ICompositor;
    using void_compositor::NullCompositor;
    using void_compositor::CompositorFactory;
} // namespace prelude

} // namespace void_compositor
