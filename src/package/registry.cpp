/// @file registry.cpp
/// @brief Package registry implementation

#include <void_engine/package/registry.hpp>
#include <sstream>
#include <algorithm>

namespace void_package {

// =============================================================================
// PackageRegistry Implementation
// =============================================================================

PackageRegistry::PackageRegistry() = default;
PackageRegistry::~PackageRegistry() = default;

// =============================================================================
// Discovery
// =============================================================================

void_core::Result<std::size_t> PackageRegistry::scan_directory(
    const std::filesystem::path& path,
    bool recursive) {

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return void_core::Err<std::size_t>(
            void_core::Error(void_core::ErrorCode::NotFound,
                "Directory not found: " + path.string()));
    }

    if (!std::filesystem::is_directory(path, ec)) {
        return void_core::Err<std::size_t>(
            void_core::Error(void_core::ErrorCode::InvalidArgument,
                "Path is not a directory: " + path.string()));
    }

    std::size_t count = 0;

    auto process_entry = [this, &count](const std::filesystem::directory_entry& entry) {
        if (entry.is_regular_file() && is_package_manifest_path(entry.path())) {
            scan_file(entry.path());
            ++count;
        }
    };

    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path, ec)) {
            process_entry(entry);
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
            process_entry(entry);
        }
    }

    return void_core::Ok(count);
}

void_core::Result<void> PackageRegistry::register_manifest(
    const std::filesystem::path& manifest_path) {

    auto manifest_result = PackageManifest::load(manifest_path);
    if (!manifest_result) {
        return void_core::Err(manifest_result.error());
    }

    auto validate_result = manifest_result->validate();
    if (!validate_result) {
        return validate_result;
    }

    // Store file modification time for change detection
    std::error_code ec;
    auto mod_time = std::filesystem::last_write_time(manifest_path, ec);
    if (!ec) {
        m_file_times[manifest_result->name] = mod_time;
    }

    // Pass the manifest file path directly - loaders handle both file and directory paths
    m_resolver.add_available(std::move(*manifest_result), manifest_path);

    return void_core::Ok();
}

void_core::Result<void> PackageRegistry::unregister_package(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_loaded.count(name)) {
        return void_core::Err(void_core::Error(void_core::ErrorCode::InvalidState,
            "Cannot unregister loaded package: " + name));
    }

    if (!m_resolver.remove_available(name)) {
        return void_core::Err(void_core::Error(void_core::ErrorCode::NotFound,
            "Package not found: " + name));
    }

    m_file_times.erase(name);
    m_failed.erase(name);

    return void_core::Ok();
}

void PackageRegistry::clear_available() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Only remove packages that aren't loaded
    auto names = m_resolver.available_packages();
    for (const auto& name : names) {
        if (!m_loaded.count(name)) {
            m_resolver.remove_available(name);
            m_file_times.erase(name);
        }
    }
    m_failed.clear();
}

// =============================================================================
// Loading
// =============================================================================

void_core::Result<void> PackageRegistry::load_package(
    const std::string& name,
    LoadContext& ctx) {

    // Check if already loaded
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_loaded.count(name) && m_loaded[name].status == PackageStatus::Loaded) {
            return void_core::Ok();  // Already loaded
        }
    }

    // Resolve dependencies
    auto resolve_result = m_resolver.resolve(name);
    if (!resolve_result) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_failed[name] = resolve_result.error().message();
        return void_core::Err(resolve_result.error());
    }

    // Load each package in order
    for (const auto& resolved : *resolve_result) {
        auto load_result = load_resolved(resolved, ctx);
        if (!load_result) {
            return load_result;
        }
    }

    return void_core::Ok();
}

void_core::Result<void> PackageRegistry::load_packages(
    const std::vector<std::string>& names,
    LoadContext& ctx) {

    // Resolve all together
    auto resolve_result = m_resolver.resolve_all(names);
    if (!resolve_result) {
        return void_core::Err(resolve_result.error());
    }

    // Load in order
    for (const auto& resolved : *resolve_result) {
        auto load_result = load_resolved(resolved, ctx);
        if (!load_result) {
            return load_result;
        }
    }

    return void_core::Ok();
}

void_core::Result<void> PackageRegistry::unload_package(
    const std::string& name,
    LoadContext& ctx,
    bool force) {

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_loaded.find(name);
    if (it == m_loaded.end()) {
        return void_core::Err(void_core::Error(void_core::ErrorCode::NotFound,
            "Package not loaded: " + name));
    }

    // Check for dependents
    if (!force) {
        auto dependents = affected_by_unload(name);
        if (!dependents.empty()) {
            std::ostringstream oss;
            oss << "Cannot unload '" << name << "': these packages depend on it: ";
            for (std::size_t i = 0; i < dependents.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << dependents[i];
            }
            return void_core::Err(void_core::Error(void_core::ErrorCode::InvalidState,
                oss.str()));
        }
    }

    // If forcing, unload dependents first (in reverse order)
    if (force) {
        auto dependents = affected_by_unload(name);
        // Unload in reverse topological order
        std::reverse(dependents.begin(), dependents.end());
        for (const auto& dep : dependents) {
            auto result = unload_single(dep, ctx);
            if (!result && !force) {
                return result;
            }
        }
    }

    return unload_single(name, ctx);
}

void_core::Result<void> PackageRegistry::unload_all(LoadContext& ctx) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Get all loaded package names
    std::vector<std::string> to_unload;
    for (const auto& [name, _] : m_loaded) {
        to_unload.push_back(name);
    }

    // Sort by reverse load order (unload dependents first)
    // For now, just reverse the order
    std::reverse(to_unload.begin(), to_unload.end());

    for (const auto& name : to_unload) {
        auto result = unload_single(name, ctx);
        if (!result) {
            return result;
        }
    }

    return void_core::Ok();
}

// =============================================================================
// Hot-Reload
// =============================================================================

void_core::Result<void> PackageRegistry::reload_package(
    const std::string& name,
    LoadContext& ctx) {

    const PackageManifest* manifest = m_resolver.get_manifest(name);
    if (!manifest) {
        return void_core::Err(void_core::Error(void_core::ErrorCode::NotFound,
            "Package not found: " + name));
    }

    PackageLoader* loader = ctx.get_loader(manifest->type);
    if (!loader) {
        return void_core::Err(void_core::Error(void_core::ErrorCode::NotSupported,
            "No loader registered for package type: " + std::string(package_type_to_string(manifest->type))));
    }

    // Resolve to get ResolvedPackage
    auto resolve_result = m_resolver.resolve(name);
    if (!resolve_result || resolve_result->empty()) {
        return void_core::Err(void_core::Error(void_core::ErrorCode::NotFound,
            "Failed to resolve package: " + name));
    }

    // Find the specific package in resolution order
    const ResolvedPackage* rp = nullptr;
    for (const auto& resolved : *resolve_result) {
        if (resolved.manifest.name == name) {
            rp = &resolved;
            break;
        }
    }

    if (!rp) {
        return void_core::Err(void_core::Error(void_core::ErrorCode::NotFound,
            "Package not found in resolution: " + name));
    }

    // Use loader's reload method
    auto result = loader->reload(*rp, ctx);

    if (result) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_loaded.count(name)) {
            m_loaded[name].load_time = std::chrono::steady_clock::now();
        }
    }

    return result;
}

std::vector<std::string> PackageRegistry::check_for_changes() const {
    std::vector<std::string> changed;

    for (const auto& [name, last_time] : m_file_times) {
        const PackageManifest* manifest = m_resolver.get_manifest(name);
        if (!manifest) continue;

        std::error_code ec;
        auto current_time = std::filesystem::last_write_time(manifest->source_path, ec);
        if (!ec && current_time != last_time) {
            changed.push_back(name);
        }
    }

    return changed;
}

// =============================================================================
// Status Queries
// =============================================================================

std::optional<PackageStatus> PackageRegistry::status(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_loaded.find(name);
    if (it != m_loaded.end()) {
        return it->second.status;
    }

    if (m_failed.count(name)) {
        return PackageStatus::Failed;
    }

    if (m_resolver.has_package(name)) {
        return PackageStatus::Available;
    }

    return std::nullopt;
}

const LoadedPackage* PackageRegistry::get_loaded(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_loaded.find(name);
    if (it == m_loaded.end()) {
        return nullptr;
    }
    return &it->second;
}

const PackageManifest* PackageRegistry::get_manifest(const std::string& name) const {
    return m_resolver.get_manifest(name);
}

bool PackageRegistry::is_loaded(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_loaded.find(name);
    return it != m_loaded.end() && it->second.status == PackageStatus::Loaded;
}

bool PackageRegistry::is_available(const std::string& name) const {
    return m_resolver.has_package(name);
}

// =============================================================================
// Package Listings
// =============================================================================

std::vector<std::string> PackageRegistry::loaded_packages() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> names;
    names.reserve(m_loaded.size());
    for (const auto& [name, pkg] : m_loaded) {
        if (pkg.status == PackageStatus::Loaded) {
            names.push_back(name);
        }
    }
    return names;
}

std::vector<std::string> PackageRegistry::available_packages() const {
    return m_resolver.available_packages();
}

std::vector<std::string> PackageRegistry::packages_of_type(PackageType type) const {
    return m_resolver.packages_of_type(type);
}

std::vector<std::string> PackageRegistry::packages_by_status(PackageStatus status) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> names;

    if (status == PackageStatus::Failed) {
        for (const auto& [name, _] : m_failed) {
            names.push_back(name);
        }
    } else if (status == PackageStatus::Available) {
        auto all = m_resolver.available_packages();
        for (const auto& name : all) {
            if (!m_loaded.count(name) && !m_failed.count(name)) {
                names.push_back(name);
            }
        }
    } else {
        for (const auto& [name, pkg] : m_loaded) {
            if (pkg.status == status) {
                names.push_back(name);
            }
        }
    }

    return names;
}

std::vector<std::string> PackageRegistry::get_dependents(const std::string& name) const {
    return m_resolver.get_dependents(name);
}

std::vector<std::string> PackageRegistry::get_dependencies(const std::string& name) const {
    return m_resolver.get_dependencies(name);
}

// =============================================================================
// Validation
// =============================================================================

void_core::Result<void> PackageRegistry::validate() const {
    return m_resolver.validate_all();
}

// =============================================================================
// Statistics
// =============================================================================

std::size_t PackageRegistry::loaded_count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::size_t count = 0;
    for (const auto& [_, pkg] : m_loaded) {
        if (pkg.status == PackageStatus::Loaded) {
            ++count;
        }
    }
    return count;
}

std::size_t PackageRegistry::available_count() const {
    return m_resolver.size();
}

std::size_t PackageRegistry::total_count() const {
    return m_resolver.size();
}

// =============================================================================
// Debugging
// =============================================================================

std::string PackageRegistry::format_state() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ostringstream oss;
    oss << "Package Registry State\n";
    oss << "======================\n\n";

    oss << "Available: " << m_resolver.size() << " packages\n";
    oss << "Loaded: " << loaded_count() << " packages\n";
    oss << "Failed: " << m_failed.size() << " packages\n\n";

    if (!m_loaded.empty()) {
        oss << "Loaded Packages:\n";
        for (const auto& [name, pkg] : m_loaded) {
            oss << "  " << name << " v" << pkg.resolved.manifest.version.to_string()
                << " [" << package_status_to_string(pkg.status) << "]\n";
        }
        oss << "\n";
    }

    if (!m_failed.empty()) {
        oss << "Failed Packages:\n";
        for (const auto& [name, error] : m_failed) {
            oss << "  " << name << ": " << error << "\n";
        }
        oss << "\n";
    }

    return oss.str();
}

std::string PackageRegistry::format_dependency_graph() const {
    return m_resolver.to_dot_graph();
}

// =============================================================================
// Private Methods
// =============================================================================

void_core::Result<void> PackageRegistry::load_resolved(
    const ResolvedPackage& resolved,
    LoadContext& ctx) {

    const std::string& name = resolved.manifest.name;

    // Check if already loaded
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_loaded.count(name) && m_loaded[name].status == PackageStatus::Loaded) {
            return void_core::Ok();
        }
    }

    // Get appropriate loader
    PackageLoader* loader = ctx.get_loader(resolved.manifest.type);
    if (!loader) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_failed[name] = "No loader registered for type: " +
            std::string(package_type_to_string(resolved.manifest.type));
        return void_core::Err(void_core::Error(void_core::ErrorCode::NotSupported,
            m_failed[name]));
    }

    // Mark as loading
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        LoadedPackage lp;
        lp.resolved = resolved;
        lp.status = PackageStatus::Loading;
        lp.load_time = std::chrono::steady_clock::now();
        lp.last_access = lp.load_time;
        m_loaded[name] = std::move(lp);
    }

    ctx.begin_loading(name);

    // Actually load
    auto load_result = loader->load(resolved, ctx);

    ctx.end_loading(name);

    // Update status
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (load_result) {
            m_loaded[name].status = PackageStatus::Loaded;
            m_failed.erase(name);
        } else {
            m_loaded[name].status = PackageStatus::Failed;
            m_loaded[name].error_message = load_result.error().message();
            m_failed[name] = load_result.error().message();
        }
    }

    return load_result;
}

void_core::Result<void> PackageRegistry::unload_single(
    const std::string& name,
    LoadContext& ctx) {

    auto it = m_loaded.find(name);
    if (it == m_loaded.end()) {
        return void_core::Ok();  // Already unloaded
    }

    PackageLoader* loader = ctx.get_loader(it->second.resolved.manifest.type);
    if (!loader) {
        return void_core::Err(void_core::Error(void_core::ErrorCode::NotSupported,
            "No loader for package type"));
    }

    // Mark as unloading
    it->second.status = PackageStatus::Unloading;

    auto result = loader->unload(name, ctx);

    if (result) {
        m_loaded.erase(it);
    } else {
        it->second.status = PackageStatus::Failed;
        it->second.error_message = result.error().message();
    }

    return result;
}

std::vector<std::string> PackageRegistry::affected_by_unload(
    const std::string& name) const {

    std::vector<std::string> affected;

    // Find loaded packages that depend on this one
    for (const auto& [pkg_name, pkg] : m_loaded) {
        if (pkg_name == name) continue;
        if (pkg.status != PackageStatus::Loaded) continue;

        for (const auto& dep : pkg.resolved.resolved_deps) {
            if (dep == name) {
                affected.push_back(pkg_name);
                break;
            }
        }
    }

    return affected;
}

void PackageRegistry::scan_file(const std::filesystem::path& path) {
    auto result = register_manifest(path);
    // Silently ignore failures - they'll be logged elsewhere
    (void)result;
}

// =============================================================================
// Utility Functions
// =============================================================================

std::vector<std::string> package_manifest_extensions() {
    return {
        ".world.json",
        ".layer.json",
        ".plugin.json",
        ".widget.json",
        ".bundle.json"
    };
}

bool is_package_manifest_path(const std::filesystem::path& path) {
    std::string filename = path.filename().string();

    for (const auto& ext : package_manifest_extensions()) {
        if (filename.size() > ext.size() &&
            filename.compare(filename.size() - ext.size(), ext.size(), ext) == 0) {
            return true;
        }
    }

    return false;
}

std::optional<PackageType> package_type_from_extension(const std::filesystem::path& path) {
    std::string filename = path.filename().string();

    if (filename.ends_with(".world.json")) return PackageType::World;
    if (filename.ends_with(".layer.json")) return PackageType::Layer;
    if (filename.ends_with(".plugin.json")) return PackageType::Plugin;
    if (filename.ends_with(".widget.json")) return PackageType::Widget;
    if (filename.ends_with(".bundle.json")) return PackageType::Asset;

    return std::nullopt;
}

} // namespace void_package
