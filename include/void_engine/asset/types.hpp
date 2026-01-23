#pragma once

/// @file types.hpp
/// @brief Core types for void_asset module

#include "fwd.hpp"
#include <void_engine/core/error.hpp>
#include <void_engine/core/id.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <typeindex>
#include <functional>

namespace void_asset {

// =============================================================================
// LoadState
// =============================================================================

/// Asset loading state
enum class LoadState : std::uint8_t {
    NotLoaded,   // Asset registered but not loaded
    Loading,     // Currently being loaded
    Loaded,      // Successfully loaded
    Failed,      // Load failed
    Reloading,   // Being hot-reloaded
};

/// Get load state name
[[nodiscard]] inline const char* load_state_name(LoadState state) {
    switch (state) {
        case LoadState::NotLoaded: return "NotLoaded";
        case LoadState::Loading: return "Loading";
        case LoadState::Loaded: return "Loaded";
        case LoadState::Failed: return "Failed";
        case LoadState::Reloading: return "Reloading";
        default: return "Unknown";
    }
}

// =============================================================================
// AssetId
// =============================================================================

/// Unique identifier for an asset
struct AssetId {
    std::uint64_t id = 0;

    /// Default constructor (invalid ID)
    constexpr AssetId() noexcept = default;

    /// Construct from raw ID
    explicit constexpr AssetId(std::uint64_t raw) noexcept : id(raw) {}

    /// Check if valid
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return id != 0;
    }

    /// Get raw value
    [[nodiscard]] constexpr std::uint64_t raw() const noexcept {
        return id;
    }

    /// Comparison
    constexpr bool operator==(const AssetId&) const noexcept = default;
    constexpr auto operator<=>(const AssetId&) const noexcept = default;

    /// Create invalid ID
    [[nodiscard]] static constexpr AssetId invalid() noexcept {
        return AssetId{0};
    }
};

// =============================================================================
// AssetPath
// =============================================================================

/// Asset path with normalized format
struct AssetPath {
    std::string path;
    std::uint64_t hash = 0;

    /// Default constructor
    AssetPath() = default;

    /// Construct from string
    explicit AssetPath(std::string p)
        : path(normalize(std::move(p)))
        , hash(void_core::detail::fnv1a_hash(path)) {}

    explicit AssetPath(const char* p)
        : path(normalize(p))
        , hash(void_core::detail::fnv1a_hash(path)) {}

    /// Get path
    [[nodiscard]] const std::string& str() const noexcept { return path; }

    /// Get extension (without dot)
    [[nodiscard]] std::string extension() const {
        auto pos = path.rfind('.');
        if (pos == std::string::npos) return "";
        return path.substr(pos + 1);
    }

    /// Get filename (without directory)
    [[nodiscard]] std::string filename() const {
        auto pos = path.rfind('/');
        if (pos == std::string::npos) return path;
        return path.substr(pos + 1);
    }

    /// Get directory
    [[nodiscard]] std::string directory() const {
        auto pos = path.rfind('/');
        if (pos == std::string::npos) return "";
        return path.substr(0, pos);
    }

    /// Get stem (filename without extension)
    [[nodiscard]] std::string stem() const {
        std::string fname = filename();
        auto pos = fname.rfind('.');
        if (pos == std::string::npos) return fname;
        return fname.substr(0, pos);
    }

    /// Comparison
    bool operator==(const AssetPath& other) const noexcept {
        return hash == other.hash && path == other.path;
    }

    bool operator<(const AssetPath& other) const noexcept {
        if (hash != other.hash) return hash < other.hash;
        return path < other.path;
    }

private:
    static std::string normalize(std::string p) {
        // Convert backslashes to forward slashes
        for (char& c : p) {
            if (c == '\\') c = '/';
        }
        // Remove trailing slash
        while (!p.empty() && p.back() == '/') {
            p.pop_back();
        }
        return p;
    }
};

// =============================================================================
// AssetTypeId
// =============================================================================

/// Type identifier for assets
struct AssetTypeId {
    std::type_index type_id;
    std::string name;

    /// Construct from type
    template<typename T>
    [[nodiscard]] static AssetTypeId of() {
        return AssetTypeId{
            std::type_index(typeid(T)),
            typeid(T).name()
        };
    }

    /// Construct from type_index
    explicit AssetTypeId(std::type_index tid)
        : type_id(tid), name(tid.name()) {}

    AssetTypeId(std::type_index tid, std::string n)
        : type_id(tid), name(std::move(n)) {}

    /// Comparison
    bool operator==(const AssetTypeId& other) const noexcept {
        return type_id == other.type_id;
    }

    bool operator<(const AssetTypeId& other) const noexcept {
        return type_id < other.type_id;
    }
};

// =============================================================================
// AssetMetadata
// =============================================================================

/// Metadata about an asset
struct AssetMetadata {
    AssetId id;
    AssetPath path;
    AssetTypeId type_id{std::type_index(typeid(void))};
    LoadState state = LoadState::NotLoaded;
    std::uint32_t generation = 0;
    std::size_t size_bytes = 0;
    std::chrono::system_clock::time_point loaded_at;
    std::chrono::system_clock::time_point modified_at;
    std::vector<AssetId> dependencies;
    std::vector<AssetId> dependents;
    std::string error_message;

    /// Default constructor
    AssetMetadata() = default;

    /// Check if loaded
    [[nodiscard]] bool is_loaded() const noexcept {
        return state == LoadState::Loaded;
    }

    /// Check if loading
    [[nodiscard]] bool is_loading() const noexcept {
        return state == LoadState::Loading || state == LoadState::Reloading;
    }

    /// Check if failed
    [[nodiscard]] bool is_failed() const noexcept {
        return state == LoadState::Failed;
    }

    /// Mark as loading
    void mark_loading() {
        state = LoadState::Loading;
    }

    /// Mark as loaded
    void mark_loaded(std::size_t size = 0) {
        state = LoadState::Loaded;
        size_bytes = size;
        loaded_at = std::chrono::system_clock::now();
        generation++;
    }

    /// Mark as failed
    void mark_failed(const std::string& error) {
        state = LoadState::Failed;
        error_message = error;
    }

    /// Mark as reloading
    void mark_reloading() {
        state = LoadState::Reloading;
    }

    /// Add dependency
    void add_dependency(AssetId dep) {
        dependencies.push_back(dep);
    }

    /// Add dependent
    void add_dependent(AssetId dep) {
        dependents.push_back(dep);
    }
};

// =============================================================================
// AssetEvent
// =============================================================================

/// Type of asset event
enum class AssetEventType : std::uint8_t {
    Loaded,
    Failed,
    Reloaded,
    Unloaded,
    FileChanged,
};

/// Get event type name
[[nodiscard]] inline const char* asset_event_type_name(AssetEventType type) {
    switch (type) {
        case AssetEventType::Loaded: return "Loaded";
        case AssetEventType::Failed: return "Failed";
        case AssetEventType::Reloaded: return "Reloaded";
        case AssetEventType::Unloaded: return "Unloaded";
        case AssetEventType::FileChanged: return "FileChanged";
        default: return "Unknown";
    }
}

/// Asset event
struct AssetEvent {
    AssetEventType type;
    AssetId id;
    AssetPath path;
    std::string error;
    std::uint32_t generation = 0;
    std::chrono::steady_clock::time_point timestamp;

    /// Default constructor
    AssetEvent()
        : type(AssetEventType::Loaded)
        , timestamp(std::chrono::steady_clock::now()) {}

    /// Construct with values
    AssetEvent(AssetEventType t, AssetId aid, AssetPath p = {})
        : type(t)
        , id(aid)
        , path(std::move(p))
        , timestamp(std::chrono::steady_clock::now()) {}

    /// Factory methods
    [[nodiscard]] static AssetEvent loaded(AssetId id, const AssetPath& path) {
        return AssetEvent{AssetEventType::Loaded, id, path};
    }

    [[nodiscard]] static AssetEvent failed(AssetId id, const AssetPath& path, const std::string& err) {
        AssetEvent e{AssetEventType::Failed, id, path};
        e.error = err;
        return e;
    }

    [[nodiscard]] static AssetEvent reloaded(AssetId id, const AssetPath& path, std::uint32_t gen) {
        AssetEvent e{AssetEventType::Reloaded, id, path};
        e.generation = gen;
        return e;
    }

    [[nodiscard]] static AssetEvent unloaded(AssetId id, const AssetPath& path) {
        return AssetEvent{AssetEventType::Unloaded, id, path};
    }

    [[nodiscard]] static AssetEvent file_changed(const AssetPath& path) {
        return AssetEvent{AssetEventType::FileChanged, AssetId::invalid(), path};
    }
};

// =============================================================================
// AssetError
// =============================================================================

/// Asset-related errors
struct AssetError {
    /// Asset not found
    [[nodiscard]] static void_core::Error not_found(const std::string& path) {
        return void_core::Error(void_core::ErrorCode::NotFound,
            "Asset not found: " + path);
    }

    /// Asset already loaded
    [[nodiscard]] static void_core::Error already_loaded(const std::string& path) {
        return void_core::Error(void_core::ErrorCode::AlreadyExists,
            "Asset already loaded: " + path);
    }

    /// Load failed
    [[nodiscard]] static void_core::Error load_failed(const std::string& path, const std::string& reason) {
        return void_core::Error(void_core::ErrorCode::IOError,
            "Failed to load asset '" + path + "': " + reason);
    }

    /// No loader for type
    [[nodiscard]] static void_core::Error no_loader(const std::string& extension) {
        return void_core::Error(void_core::ErrorCode::NotFound,
            "No loader registered for extension: " + extension);
    }

    /// Parse error
    [[nodiscard]] static void_core::Error parse_error(const std::string& path, const std::string& reason) {
        return void_core::Error(void_core::ErrorCode::ParseError,
            "Failed to parse asset '" + path + "': " + reason);
    }

    /// Dependency failed
    [[nodiscard]] static void_core::Error dependency_failed(const std::string& asset, const std::string& dep) {
        return void_core::Error(void_core::ErrorCode::DependencyMissing,
            "Asset '" + asset + "' failed to load dependency: " + dep);
    }
};

} // namespace void_asset

/// Hash specializations
template<>
struct std::hash<void_asset::AssetId> {
    std::size_t operator()(const void_asset::AssetId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.id);
    }
};

template<>
struct std::hash<void_asset::AssetPath> {
    std::size_t operator()(const void_asset::AssetPath& path) const noexcept {
        return static_cast<std::size_t>(path.hash);
    }
};
