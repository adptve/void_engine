#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_asset module

#include <cstdint>

namespace void_asset {

// Types
enum class LoadState : std::uint8_t;
struct AssetId;
struct AssetPath;
struct AssetMetadata;

// Handles
template<typename T> class Handle;
template<typename T> class WeakHandle;
struct HandleData;

// Loader system
class LoadContext;
class LoadError;
template<typename T> class AssetLoader;
class ErasedLoader;
class LoaderRegistry;

// Storage
class AssetStorage;
template<typename T> class TypedStorage;

// Server
struct AssetServerConfig;
class AssetServer;

// Events
enum class AssetEventType : std::uint8_t;
struct AssetEvent;

// Hot-reload
struct AssetChange;
class AssetWatcher;
class AssetHotReloadManager;

// Injector
struct InjectorConfig;
struct InjectionEvent;
class AssetInjector;

} // namespace void_asset
