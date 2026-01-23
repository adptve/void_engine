#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_presenter

#include <cstdint>
#include <memory>

namespace void_presenter {

// Types
enum class SurfaceFormat;
enum class PresentMode;
enum class VSync;
enum class AlphaMode;
enum class SurfaceState;
enum class FrameState;

// Surface
struct SurfaceConfig;
struct SurfaceCapabilities;
struct SurfaceTexture;
class ISurface;
class NullSurface;

// Frame
class Frame;
struct FrameOutput;
struct FrameStats;

// Timing
class FrameTiming;
class FrameLimiter;

// Rehydration
class RehydrationState;
class RehydrationStore;

// Presenter
struct PresenterId;
struct PresenterConfig;
struct PresenterCapabilities;
class IPresenter;
class PresenterManager;
class NullPresenter;

// Error types
enum class PresenterErrorKind;
struct PresenterError;
enum class SurfaceErrorKind;
struct SurfaceError;
enum class RehydrationErrorKind;
struct RehydrationError;

} // namespace void_presenter
