/// @file world_package_loader.cpp
/// @brief World package loader implementation

#include <void_engine/package/world_composer.hpp>
#include <void_engine/package/world_package.hpp>
#include <void_engine/package/loader.hpp>
#include <void_engine/package/registry.hpp>

#include <set>

namespace void_package {

// =============================================================================
// WorldState Utilities
// =============================================================================

const char* world_state_to_string(WorldState state) noexcept {
    switch (state) {
        case WorldState::Unloaded:  return "unloaded";
        case WorldState::Loading:   return "loading";
        case WorldState::Ready:     return "ready";
        case WorldState::Unloading: return "unloading";
        case WorldState::Failed:    return "failed";
    }
    return "unknown";
}

// =============================================================================
// LoadedWorldInfo
// =============================================================================

double LoadedWorldInfo::load_duration_ms() const {
    auto duration = load_finished - load_started;
    return std::chrono::duration<double, std::milli>(duration).count();
}

std::size_t LoadedWorldInfo::total_entity_count() const {
    std::size_t count = scene_entities.size();
    if (player_entity.has_value()) {
        count++;
    }
    return count;
}

// =============================================================================
// WorldPackageLoader
// =============================================================================

/// Internal WorldPackageLoader that wraps WorldComposer
class WorldPackageLoader : public PackageLoader {
public:
    WorldPackageLoader() = default;

    [[nodiscard]] PackageType supported_type() const override {
        return PackageType::World;
    }

    [[nodiscard]] const char* name() const override {
        return "WorldPackageLoader";
    }

    [[nodiscard]] void_core::Result<void> load(
        const ResolvedPackage& package,
        LoadContext& ctx) override {

        // Parse the world manifest
        auto manifest_result = WorldPackageManifest::load(package.path);
        if (!manifest_result) {
            return void_core::Err("Failed to parse world manifest: " +
                                  manifest_result.error().message());
        }

        // Store as loaded
        m_loaded_packages.insert(package.manifest.name);
        m_manifests[package.manifest.name] = std::move(*manifest_result);

        // Note: Actual world composition is done by WorldComposer, not this loader.
        // This loader just tracks what worlds have been "loaded" (manifests parsed).
        // The WorldComposer handles the full boot sequence.

        return void_core::Ok();
    }

    [[nodiscard]] void_core::Result<void> unload(
        const std::string& package_name,
        LoadContext& ctx) override {

        auto it = m_loaded_packages.find(package_name);
        if (it == m_loaded_packages.end()) {
            return void_core::Err("Package not loaded: " + package_name);
        }

        m_loaded_packages.erase(it);
        m_manifests.erase(package_name);

        return void_core::Ok();
    }

    [[nodiscard]] bool is_loaded(const std::string& package_name) const override {
        return m_loaded_packages.count(package_name) > 0;
    }

    [[nodiscard]] std::vector<std::string> loaded_packages() const override {
        return std::vector<std::string>(m_loaded_packages.begin(), m_loaded_packages.end());
    }

    [[nodiscard]] bool supports_hot_reload() const override {
        // World hot-reload requires full reload
        return false;
    }

    /// Get a loaded manifest
    [[nodiscard]] const WorldPackageManifest* get_manifest(const std::string& name) const {
        auto it = m_manifests.find(name);
        return it != m_manifests.end() ? &it->second : nullptr;
    }

private:
    std::set<std::string> m_loaded_packages;
    std::map<std::string, WorldPackageManifest> m_manifests;
};

// =============================================================================
// Factory Function
// =============================================================================

std::unique_ptr<PackageLoader> create_world_package_loader() {
    return std::make_unique<WorldPackageLoader>();
}

} // namespace void_package
