/// @file loader.cpp
/// @brief Package loader implementation

#include <void_engine/package/loader.hpp>

namespace void_package {

// =============================================================================
// LoadContext Implementation
// =============================================================================

LoadContext::LoadContext(void_ecs::World* ecs_world, void_event::EventBus* event_bus)
    : m_ecs_world(ecs_world)
    , m_event_bus(event_bus) {
}

void LoadContext::register_loader(std::unique_ptr<PackageLoader> loader) {
    if (!loader) {
        return;
    }
    PackageType type = loader->supported_type();
    m_loaders[type] = std::move(loader);
}

PackageLoader* LoadContext::get_loader(PackageType type) const {
    auto it = m_loaders.find(type);
    if (it == m_loaders.end()) {
        return nullptr;
    }
    return it->second.get();
}

bool LoadContext::has_loader(PackageType type) const {
    return m_loaders.count(type) > 0;
}

void LoadContext::begin_loading(const std::string& package_name) {
    m_loading.insert(package_name);
}

void LoadContext::end_loading(const std::string& package_name) {
    m_loading.erase(package_name);
}

bool LoadContext::is_loading(const std::string& package_name) const {
    return m_loading.count(package_name) > 0;
}

// =============================================================================
// PackageLoader Implementation
// =============================================================================

void_core::Result<void> PackageLoader::reload(
    const ResolvedPackage& package,
    LoadContext& ctx) {

    // Default implementation: unload then load
    auto unload_result = unload(package.manifest.name, ctx);
    if (!unload_result) {
        return unload_result;
    }

    return load(package, ctx);
}

// =============================================================================
// StubPackageLoader Implementation
// =============================================================================

void_core::Result<void> StubPackageLoader::load(
    const ResolvedPackage& package,
    LoadContext& /*ctx*/) {

    m_loaded.insert(package.manifest.name);
    return void_core::Ok();
}

void_core::Result<void> StubPackageLoader::unload(
    const std::string& package_name,
    LoadContext& /*ctx*/) {

    m_loaded.erase(package_name);
    return void_core::Ok();
}

bool StubPackageLoader::is_loaded(const std::string& package_name) const {
    return m_loaded.count(package_name) > 0;
}

std::vector<std::string> StubPackageLoader::loaded_packages() const {
    return std::vector<std::string>(m_loaded.begin(), m_loaded.end());
}

} // namespace void_package
