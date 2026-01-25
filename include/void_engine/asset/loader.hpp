#pragma once

/// @file loader.hpp
/// @brief Asset loader system for void_asset

#include "fwd.hpp"
#include "types.hpp"
#include <void_engine/core/error.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <typeindex>
#include <type_traits>
#include <utility>

namespace void_asset {

// =============================================================================
// LoadContext
// =============================================================================

/// Context passed to asset loaders during loading
class LoadContext {
public:
    /// Constructor
    LoadContext(
        const std::vector<std::uint8_t>& data,
        const AssetPath& path,
        AssetId id)
        : m_data(data)
        , m_path(path)
        , m_id(id) {}

    /// Get raw data
    [[nodiscard]] const std::vector<std::uint8_t>& data() const noexcept {
        return m_data;
    }

    /// Get data as string
    [[nodiscard]] std::string data_as_string() const {
        return std::string(m_data.begin(), m_data.end());
    }

    /// Get asset path
    [[nodiscard]] const AssetPath& path() const noexcept {
        return m_path;
    }

    /// Get asset ID
    [[nodiscard]] AssetId id() const noexcept {
        return m_id;
    }

    /// Get file extension
    [[nodiscard]] std::string extension() const {
        return m_path.extension();
    }

    /// Get data size
    [[nodiscard]] std::size_t size() const noexcept {
        return m_data.size();
    }

    /// Add dependency
    void add_dependency(const AssetPath& dep_path) {
        m_dependencies.push_back(dep_path);
    }

    /// Add dependency by ID
    void add_dependency(AssetId dep_id) {
        m_dependency_ids.push_back(dep_id);
    }

    /// Get dependencies
    [[nodiscard]] const std::vector<AssetPath>& dependencies() const noexcept {
        return m_dependencies;
    }

    /// Get dependency IDs
    [[nodiscard]] const std::vector<AssetId>& dependency_ids() const noexcept {
        return m_dependency_ids;
    }

    /// Set metadata
    void set_metadata(const std::string& key, const std::string& value) {
        m_metadata[key] = value;
    }

    /// Get metadata
    [[nodiscard]] const std::string* get_metadata(const std::string& key) const {
        auto it = m_metadata.find(key);
        return it != m_metadata.end() ? &it->second : nullptr;
    }

private:
    const std::vector<std::uint8_t>& m_data;
    const AssetPath& m_path;
    AssetId m_id;
    std::vector<AssetPath> m_dependencies;
    std::vector<AssetId> m_dependency_ids;
    std::map<std::string, std::string> m_metadata;
};

// =============================================================================
// LoadResult<T>
// =============================================================================

/// Result of loading an asset
template<typename T>
using LoadResult = void_core::Result<std::unique_ptr<T>>;

// =============================================================================
// AssetLoader<T>
// =============================================================================

/// Interface for loading specific asset types
template<typename T>
class AssetLoader {
public:
    /// Asset type alias (for type deduction)
    using asset_type = T;

    virtual ~AssetLoader() = default;

    /// Get supported file extensions
    [[nodiscard]] virtual std::vector<std::string> extensions() const = 0;

    /// Load asset from context
    [[nodiscard]] virtual LoadResult<T> load(LoadContext& ctx) = 0;

    /// Get asset type ID
    [[nodiscard]] std::type_index type_id() const {
        return std::type_index(typeid(T));
    }

    /// Get asset type name
    [[nodiscard]] virtual std::string type_name() const {
        return typeid(T).name();
    }
};

// =============================================================================
// ErasedLoader
// =============================================================================

/// Type-erased loader interface
class ErasedLoader {
public:
    virtual ~ErasedLoader() = default;

    /// Get supported extensions
    [[nodiscard]] virtual std::vector<std::string> extensions() const = 0;

    /// Get asset type ID
    [[nodiscard]] virtual std::type_index type_id() const = 0;

    /// Get asset type name
    [[nodiscard]] virtual std::string type_name() const = 0;

    /// Load asset (returns void* owning unique_ptr internally)
    [[nodiscard]] virtual void_core::Result<void*> load_erased(LoadContext& ctx) = 0;

    /// Delete loaded asset
    virtual void delete_asset(void* asset) = 0;
};

/// Wrapper to create ErasedLoader from AssetLoader<T>
template<typename T>
class TypedErasedLoader : public ErasedLoader {
public:
    explicit TypedErasedLoader(std::unique_ptr<AssetLoader<T>> loader)
        : m_loader(std::move(loader)) {}

    [[nodiscard]] std::vector<std::string> extensions() const override {
        return m_loader->extensions();
    }

    [[nodiscard]] std::type_index type_id() const override {
        return m_loader->type_id();
    }

    [[nodiscard]] std::string type_name() const override {
        return m_loader->type_name();
    }

    [[nodiscard]] void_core::Result<void*> load_erased(LoadContext& ctx) override {
        auto result = m_loader->load(ctx);
        if (!result) {
            return void_core::Err<void*>(result.error());
        }
        return void_core::Ok(static_cast<void*>(std::move(result).value().release()));
    }

    void delete_asset(void* asset) override {
        delete static_cast<T*>(asset);
    }

private:
    std::unique_ptr<AssetLoader<T>> m_loader;
};

// =============================================================================
// LoaderRegistry
// =============================================================================

/// Registry for all asset loaders
class LoaderRegistry {
public:
    /// Default constructor
    LoaderRegistry() = default;

    /// Register typed loader (base type)
    template<typename T>
    void register_loader(std::unique_ptr<AssetLoader<T>> loader) {
        auto extensions = loader->extensions();
        auto type_id = loader->type_id();

        auto erased = std::make_unique<TypedErasedLoader<T>>(std::move(loader));

        // Map extensions to loader
        for (const auto& ext : extensions) {
            m_by_extension[ext].push_back(erased.get());
        }

        // Map type to loader
        m_by_type[type_id].push_back(erased.get());

        m_loaders.push_back(std::move(erased));
    }

    /// Register derived loader type (automatically extracts asset type from loader)
    /// Requires Derived to inherit from AssetLoader<T> and have asset_type typedef
    template<typename Derived,
             typename T = typename Derived::asset_type,
             typename = std::enable_if_t<std::is_base_of_v<AssetLoader<T>, Derived>>>
    void register_loader(std::unique_ptr<Derived> loader) {
        // Convert derived to base and delegate
        register_loader<T>(std::unique_ptr<AssetLoader<T>>(std::move(loader)));
    }

    /// Register erased loader
    void register_erased(std::unique_ptr<ErasedLoader> loader) {
        auto extensions = loader->extensions();
        auto type_id = loader->type_id();

        for (const auto& ext : extensions) {
            m_by_extension[ext].push_back(loader.get());
        }

        m_by_type[type_id].push_back(loader.get());

        m_loaders.push_back(std::move(loader));
    }

    /// Find loaders for extension
    [[nodiscard]] std::vector<ErasedLoader*> find_by_extension(const std::string& ext) const {
        auto it = m_by_extension.find(ext);
        if (it != m_by_extension.end()) {
            return it->second;
        }
        return {};
    }

    /// Find loaders for type
    [[nodiscard]] std::vector<ErasedLoader*> find_by_type(std::type_index type) const {
        auto it = m_by_type.find(type);
        if (it != m_by_type.end()) {
            return it->second;
        }
        return {};
    }

    /// Find first loader for extension
    [[nodiscard]] ErasedLoader* find_first(const std::string& ext) const {
        auto loaders = find_by_extension(ext);
        return loaders.empty() ? nullptr : loaders.front();
    }

    /// Check if extension is supported
    [[nodiscard]] bool supports_extension(const std::string& ext) const {
        return m_by_extension.find(ext) != m_by_extension.end();
    }

    /// Check if type is supported
    [[nodiscard]] bool supports_type(std::type_index type) const {
        return m_by_type.find(type) != m_by_type.end();
    }

    /// Get all supported extensions
    [[nodiscard]] std::vector<std::string> supported_extensions() const {
        std::vector<std::string> exts;
        exts.reserve(m_by_extension.size());
        for (const auto& [ext, loaders] : m_by_extension) {
            exts.push_back(ext);
        }
        return exts;
    }

    /// Get loader count
    [[nodiscard]] std::size_t len() const noexcept {
        return m_loaders.size();
    }

    /// Clear all loaders
    void clear() {
        m_loaders.clear();
        m_by_extension.clear();
        m_by_type.clear();
    }

private:
    std::vector<std::unique_ptr<ErasedLoader>> m_loaders;
    std::map<std::string, std::vector<ErasedLoader*>> m_by_extension;
    std::map<std::type_index, std::vector<ErasedLoader*>> m_by_type;
};

// =============================================================================
// Built-in Loaders
// =============================================================================

/// Raw bytes asset
struct BytesAsset {
    std::vector<std::uint8_t> data;
};

/// Bytes loader
class BytesLoader : public AssetLoader<BytesAsset> {
public:
    [[nodiscard]] std::vector<std::string> extensions() const override {
        return {"bin", "dat"};
    }

    [[nodiscard]] LoadResult<BytesAsset> load(LoadContext& ctx) override {
        auto asset = std::make_unique<BytesAsset>();
        asset->data = ctx.data();
        return void_core::Ok(std::move(asset));
    }

    [[nodiscard]] std::string type_name() const override {
        return "BytesAsset";
    }
};

/// Text asset
struct TextAsset {
    std::string text;
};

/// Text loader
class TextLoader : public AssetLoader<TextAsset> {
public:
    [[nodiscard]] std::vector<std::string> extensions() const override {
        return {"txt", "text", "md", "json", "toml", "yaml", "yml", "xml"};
    }

    [[nodiscard]] LoadResult<TextAsset> load(LoadContext& ctx) override {
        auto asset = std::make_unique<TextAsset>();
        asset->text = ctx.data_as_string();
        return void_core::Ok(std::move(asset));
    }

    [[nodiscard]] std::string type_name() const override {
        return "TextAsset";
    }
};

// =============================================================================
// Loader Utilities (Implemented in loader.cpp)
// =============================================================================

/// Normalize extension (lowercase, no leading dot)
std::string normalize_extension(const std::string& ext);

/// Check if extension is supported by registry
bool is_supported_extension(const LoaderRegistry& registry, const std::string& ext);

/// Get all extensions for a type
std::vector<std::string> get_extensions_for_type(const LoaderRegistry& registry, std::type_index type);

/// Check if extension indicates binary content
bool is_binary_extension(const std::string& ext);

// =============================================================================
// Loader Statistics (Implemented in loader.cpp)
// =============================================================================

/// Record a loader operation
void record_loader_operation(bool success, std::size_t bytes = 0);

/// Format loader statistics
std::string format_loader_statistics();

/// Reset loader statistics
void reset_loader_statistics();

// =============================================================================
// MIME Type Utilities (Implemented in loader.cpp)
// =============================================================================

/// Convert extension to MIME type
std::string extension_to_mime_type(const std::string& ext);

/// Convert MIME type to extension
std::string mime_type_to_extension(const std::string& mime);

// =============================================================================
// Debug Utilities (Implemented in loader.cpp)
// =============================================================================

namespace debug {

/// Format LoadContext for debugging
std::string format_load_context(const LoadContext& ctx);

/// Format LoaderRegistry for debugging
std::string format_loader_registry(const LoaderRegistry& registry);

/// Format ErasedLoader for debugging
std::string format_erased_loader(const ErasedLoader& loader);

} // namespace debug

} // namespace void_asset
