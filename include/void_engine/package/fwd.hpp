#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_package module

#include <cstdint>
#include <string>

namespace void_package {

// =============================================================================
// Version Types
// =============================================================================

struct SemanticVersion;
struct VersionConstraint;

// =============================================================================
// Package Types
// =============================================================================

/// Package type enumeration
enum class PackageType : std::uint8_t {
    World,    ///< world.package - composition root
    Layer,    ///< layer.package - patches/variants
    Plugin,   ///< plugin.package - systems/components/logic
    Widget,   ///< widget.package - UI/tooling/overlays
    Asset     ///< asset.bundle - pure content (models, textures, etc.)
};

/// Package status during lifecycle
enum class PackageStatus : std::uint8_t {
    Available,  ///< Known but not loaded
    Loading,    ///< Currently loading
    Loaded,     ///< Fully loaded and active
    Unloading,  ///< Currently unloading
    Failed      ///< Failed to load
};

// =============================================================================
// Manifest Types
// =============================================================================

struct PackageDependency;
struct PackageManifest;

// =============================================================================
// Resolver Types
// =============================================================================

struct ResolvedPackage;
class PackageResolver;

// =============================================================================
// Loader Types
// =============================================================================

class LoadContext;
class PackageLoader;

// =============================================================================
// Registry Types
// =============================================================================

struct LoadedPackage;
class PackageRegistry;

// =============================================================================
// Component Schema Types (Phase 2)
// =============================================================================

enum class FieldType : std::uint8_t;
struct FieldSchema;
struct ComponentSchema;
class ComponentSchemaRegistry;

// =============================================================================
// Definition Registry Types (Phase 2)
// =============================================================================

enum class CollisionPolicy : std::uint8_t;
struct DefinitionSource;
struct StoredDefinition;
struct RegistryTypeConfig;
class DefinitionRegistry;

// =============================================================================
// Plugin Package Types (Phase 3)
// =============================================================================

struct FieldDeclaration;
struct ComponentDeclaration;
struct SystemDeclaration;
struct EventHandlerDeclaration;
struct RegistryDeclaration;
struct PluginPackageManifest;

// =============================================================================
// Dynamic Library Types (Phase 3)
// =============================================================================

class DynamicLibrary;
class DynamicLibraryCache;

// =============================================================================
// Widget Package Types (Phase 4)
// =============================================================================

enum class BuildType : std::uint8_t;
enum class BindingType : std::uint8_t;
struct WidgetDeclaration;
struct WidgetBinding;
struct WidgetLibraryDeclaration;
struct WidgetPackageManifest;
struct WidgetHandle;
struct WidgetInstance;
class Widget;
class WidgetContext;
class WidgetManager;
class WidgetTypeRegistry;

// =============================================================================
// Layer Package Types (Phase 5)
// =============================================================================

enum class SpawnMode : std::uint8_t;
struct AdditiveSceneEntry;
struct SpawnerVolume;
struct SpawnerEntry;
struct LightEntry;
struct SunOverride;
struct AmbientOverride;
struct LightingOverride;
struct FogConfig;
struct PrecipitationConfig;
struct WindZone;
struct WeatherOverride;
struct ObjectiveEntry;
struct ModifierEntry;
struct LayerPackageManifest;
struct SpawnerState;
struct ModifierOriginalValue;
struct LightingOriginalState;
struct StagedLayer;
struct AppliedLayerState;
class LayerApplier;

// =============================================================================
// World Package Types (Phase 6)
// =============================================================================

enum class SpawnSelection : std::uint8_t;
enum class WorldState : std::uint8_t;
struct RootSceneConfig;
struct PlayerSpawnConfig;
struct WeatherConfig;
struct PostProcessConfig;
struct EnvironmentConfig;
struct WinCondition;
struct LoseCondition;
struct RoundFlowConfig;
struct GameplayConfig;
struct WorldLogicConfig;
struct WorldPackageManifest;
struct LoadedWorldInfo;
struct WorldLoadOptions;
struct WorldUnloadOptions;
struct WorldSwitchOptions;
struct WorldLoadedEvent;
struct WorldUnloadingEvent;
struct WorldUnloadedEvent;
struct WorldSwitchEvent;
class WorldComposer;

// =============================================================================
// Utility Functions
// =============================================================================

/// Convert PackageType to string
[[nodiscard]] const char* package_type_to_string(PackageType type) noexcept;

/// Parse PackageType from string
[[nodiscard]] bool package_type_from_string(const std::string& str, PackageType& out_type) noexcept;

/// Get file extension for package type
[[nodiscard]] const char* package_type_extension(PackageType type) noexcept;

/// Convert PackageStatus to string
[[nodiscard]] const char* package_status_to_string(PackageStatus status) noexcept;

} // namespace void_package
