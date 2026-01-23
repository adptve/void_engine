#pragma once

/// @file presenter_module.hpp
/// @brief Main include header for void_presenter
///
/// void_presenter provides production-ready frame presentation:
///
/// ## Features
///
/// - **Multi-Backend Support** (like Unity/Unreal RHI)
///   - wgpu-native (Vulkan, D3D12, Metal, OpenGL)
///   - WebGPU (web browsers)
///   - OpenXR (native VR/XR)
///   - WebXR (web VR/XR)
///
/// - **Surface/Swapchain Management**
///   - Triple buffering by default
///   - Automatic resize handling
///   - V-Sync and VRR support
///   - HDR output
///
/// - **XR/VR Support**
///   - Stereo rendering
///   - Hand tracking
///   - Controller input
///   - Foveated rendering
///
/// - **Hot-Swap**
///   - Switch backends at runtime
///   - State preservation across switches
///   - Seamless XR transitions
///
/// ## Architecture
///
/// ```
/// Application
///     │
///     ▼
/// ┌─────────────────────────────────────────┐
/// │        MultiBackendPresenter            │
/// │  (unified API for all platforms)        │
/// └─────────────────────────────────────────┘
///     │           │           │           │
///     ▼           ▼           ▼           ▼
/// ┌───────┐  ┌───────┐  ┌───────┐  ┌───────┐
/// │ wgpu  │  │WebGPU │  │OpenXR │  │ WebXR │
/// │native │  │ (web) │  │(VR/XR)│  │(webVR)│
/// └───────┘  └───────┘  └───────┘  └───────┘
///     │
///     ▼
/// ┌─────────────────────────────────────────┐
/// │   Vulkan  │  D3D12  │  Metal  │ OpenGL  │
/// └─────────────────────────────────────────┘
/// ```
///
/// ## Quick Start
///
/// ### Desktop Application
/// ```cpp
/// #include <void_engine/presenter/presenter_module.hpp>
///
/// using namespace void_presenter;
///
/// // Create presenter
/// MultiBackendPresenterConfig config;
/// config.backend_config.preferred_type = BackendType::Wgpu;
/// config.target_fps = 60;
///
/// MultiBackendPresenter presenter(config);
/// presenter.initialize();
///
/// // Create window output
/// WindowHandle window{/* ... */};
/// auto target = presenter.create_output_target(
///     window,
///     OutputTargetConfig{.type = OutputTargetType::Window, .is_primary = true}
/// );
///
/// // Frame loop
/// while (running) {
///     if (auto frame = presenter.begin_frame()) {
///         AcquiredImage image;
///         if (presenter.begin_frame_for_target(target, image)) {
///             // Render to image...
///             presenter.end_frame_for_target(target);
///         }
///         presenter.end_frame(*frame);
///     }
/// }
///
/// presenter.shutdown();
/// ```
///
/// ### VR Application
/// ```cpp
/// // Configure for VR
/// MultiBackendPresenterConfig config;
/// config.xr_config = xr::XrSessionConfig{
///     .enable_hand_tracking = true,
///     .primary_reference_space = xr::ReferenceSpaceType::LocalFloor
/// };
///
/// MultiBackendPresenter presenter(config);
/// presenter.initialize();
/// presenter.start_xr_session();
///
/// // VR frame loop
/// while (running) {
///     if (auto frame = presenter.begin_frame()) {
///         if (auto xr_frame = presenter.current_xr_frame()) {
///             // Render left eye
///             render_eye(xr_frame->views.left);
///             // Render right eye
///             render_eye(xr_frame->views.right);
///         }
///         presenter.end_frame(*frame);
///     }
/// }
/// ```
///
/// ### Hot-Swap Backends
/// ```cpp
/// // Switch from Vulkan to D3D12 at runtime
/// presenter.switch_backend(BackendType::D3D12, BackendSwitchReason::UserRequested);
///
/// // Listen for backend switch events
/// presenter.set_backend_switch_callback([](const BackendSwitchEvent& event) {
///     if (event.success) {
///         std::cout << "Switched from " << to_string(event.old_backend)
///                   << " to " << to_string(event.new_backend) << "\n";
///     }
/// });
/// ```

// Core types and enums
#include "fwd.hpp"
#include "types.hpp"

// Surface and swapchain
#include "surface.hpp"
#include "swapchain.hpp"

// Frame management
#include "frame.hpp"
#include "timing.hpp"

// Backend abstraction
#include "backend.hpp"

// XR support
#include "xr/xr_types.hpp"
#include "xr/xr_system.hpp"

// Hot-swap support
#include "rehydration.hpp"

// Main presenter
#include "presenter.hpp"
#include "multi_backend_presenter.hpp"

// Concrete backends
#include "backends/null_backend.hpp"

// Platform-specific backends (conditionally included)
#if defined(VOID_HAS_WGPU)
#include "backends/wgpu_backend.hpp"
#endif

namespace void_presenter {

/// Prelude - commonly used types for convenience
namespace prelude {
    // Types
    using void_presenter::SurfaceFormat;
    using void_presenter::PresentMode;
    using void_presenter::VSync;
    using void_presenter::AlphaMode;
    using void_presenter::SurfaceState;
    using void_presenter::FrameState;
    using void_presenter::BackendType;

    // Surface
    using void_presenter::SurfaceConfig;
    using void_presenter::SurfaceCapabilities;
    using void_presenter::SurfaceTexture;
    using void_presenter::ISurface;

    // Swapchain
    using void_presenter::SwapchainConfig;
    using void_presenter::SwapchainState;
    using void_presenter::ISwapchain;
    using void_presenter::ManagedSwapchain;
    using void_presenter::SwapchainBuilder;

    // Frame
    using void_presenter::Frame;
    using void_presenter::FrameOutput;
    using void_presenter::FrameStats;

    // Timing
    using void_presenter::FrameTiming;
    using void_presenter::FrameLimiter;

    // Backend
    using void_presenter::BackendConfig;
    using void_presenter::BackendCapabilities;
    using void_presenter::BackendFeatures;
    using void_presenter::BackendLimits;
    using void_presenter::AdapterInfo;
    using void_presenter::IBackend;
    using void_presenter::IBackendSurface;
    using void_presenter::BackendFactory;
    using void_presenter::WindowHandle;
    using void_presenter::CanvasHandle;
    using void_presenter::SurfaceTarget;
    using void_presenter::AcquiredImage;
    using void_presenter::GpuResourceHandle;

    // Rehydration
    using void_presenter::RehydrationState;
    using void_presenter::RehydrationStore;
    using void_presenter::IRehydratable;

    // Presenter
    using void_presenter::PresenterId;
    using void_presenter::PresenterConfig;
    using void_presenter::PresenterCapabilities;
    using void_presenter::IPresenter;
    using void_presenter::PresenterManager;
    using void_presenter::NullPresenter;

    // Multi-backend presenter
    using void_presenter::MultiBackendPresenter;
    using void_presenter::MultiBackendPresenterConfig;
    using void_presenter::OutputTargetId;
    using void_presenter::OutputTargetType;
    using void_presenter::OutputTargetConfig;
    using void_presenter::OutputTargetStatus;
    using void_presenter::PresenterStatistics;
    using void_presenter::BackendSwitchEvent;
    using void_presenter::BackendSwitchReason;

    // XR
    namespace xr = void_presenter::xr;
} // namespace prelude

} // namespace void_presenter
