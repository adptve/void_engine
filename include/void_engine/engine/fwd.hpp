/// @file fwd.hpp
/// @brief Forward declarations for void_engine

#pragma once

#include <cstdint>
#include <memory>

namespace void_engine {

// =============================================================================
// Forward Declarations
// =============================================================================

// Configuration
struct EngineConfig;
struct WindowConfig;
struct RenderConfig;
struct AudioConfig;
struct InputConfig;
struct AssetConfig;
class ConfigManager;
class ConfigLayer;

// State and Statistics
enum class EngineState : std::uint8_t;
enum class EngineFeature : std::uint32_t;
struct EngineStats;
struct FrameStats;

// Application Framework
class IApp;
class AppBase;
struct AppConfig;
class AppBuilder;

// Lifecycle
enum class LifecyclePhase : std::uint8_t;
struct LifecycleEvent;
class LifecycleManager;
struct LifecycleHook;

// Time
struct TimeState;
class TimeManager;

// Engine Core
class Engine;
class EngineBuilder;
class IEngineSubsystem;

// Smart pointer aliases
using EnginePtr = std::unique_ptr<Engine>;
using AppPtr = std::unique_ptr<IApp>;

} // namespace void_engine
