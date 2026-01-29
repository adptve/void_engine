/// @file resolver.cpp
/// @brief Package dependency resolver implementation

#include <void_engine/package/resolver.hpp>
#include <sstream>
#include <algorithm>

namespace void_package {

// =============================================================================
// Error Formatting
// =============================================================================

std::string DependencyCycle::format() const {
    std::ostringstream oss;
    oss << "Dependency cycle detected: ";
    for (std::size_t i = 0; i < cycle_path.size(); ++i) {
        if (i > 0) oss << " -> ";
        oss << cycle_path[i];
    }
    return oss.str();
}

std::string MissingDependency::format() const {
    std::ostringstream oss;
    oss << "Package '" << package_name << "' requires '"
        << dependency_name << "' (" << constraint.to_string() << ")";
    if (is_optional) {
        oss << " [optional]";
    }
    return oss.str();
}

std::string VersionConflict::format() const {
    std::ostringstream oss;
    oss << "Version conflict for '" << dependency_name << "': ";

    for (std::size_t i = 0; i < requiring_packages.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "'" << requiring_packages[i] << "' requires " << constraints[i].to_string();
    }

    if (available.has_value()) {
        oss << "; available: " << available->to_string();
    } else {
        oss << "; no version available";
    }

    return oss.str();
}

std::string LayerViolation::format() const {
    std::ostringstream oss;
    oss << "Plugin layer violation: '" << package_name
        << "' (layer " << package_layer << ") cannot depend on '"
        << dependency_name << "' (layer " << dependency_layer << ")";
    return oss.str();
}

// =============================================================================
// PackageResolver Implementation
// =============================================================================

void PackageResolver::add_available(PackageManifest manifest, std::filesystem::path path) {
    std::string name = manifest.name;
    m_available[name] = AvailablePackage{std::move(manifest), std::move(path)};
}

bool PackageResolver::remove_available(const std::string& name) {
    return m_available.erase(name) > 0;
}

void PackageResolver::clear() {
    m_available.clear();
}

void_core::Result<std::vector<ResolvedPackage>> PackageResolver::resolve(
    const std::string& package_name) const {

    // Check package exists
    auto it = m_available.find(package_name);
    if (it == m_available.end()) {
        return void_core::Err<std::vector<ResolvedPackage>>(
            void_core::Error(void_core::ErrorCode::NotFound,
                "Package not found: " + package_name));
    }

    // Validate all dependencies are satisfiable first
    auto validate_result = validate_all();
    if (!validate_result) {
        return void_core::Err<std::vector<ResolvedPackage>>(validate_result.error());
    }

    // Topological sort
    std::vector<std::string> order;
    std::set<std::string> visited;
    std::set<std::string> in_stack;
    std::vector<std::string> current_path;

    auto result = topological_visit(package_name, order, visited, in_stack, current_path);
    if (!result) {
        return void_core::Err<std::vector<ResolvedPackage>>(result.error());
    }

    // Build resolved packages
    std::vector<ResolvedPackage> resolved;
    resolved.reserve(order.size());

    for (const auto& name : order) {
        auto pkg_it = m_available.find(name);
        if (pkg_it == m_available.end()) {
            continue;  // Should not happen after validation
        }

        ResolvedPackage rp;
        rp.manifest = pkg_it->second.manifest;
        rp.path = pkg_it->second.path;

        // Build resolved deps list (names already in order before this package)
        for (const auto& dep : rp.manifest.all_dependencies()) {
            auto dep_it = std::find(order.begin(), order.end(), dep.name);
            if (dep_it != order.end() && dep_it < std::find(order.begin(), order.end(), name)) {
                rp.resolved_deps.push_back(dep.name);
            } else if (dep.optional) {
                rp.missing_optional.push_back(dep.name);
            }
        }

        resolved.push_back(std::move(rp));
    }

    return void_core::Ok(std::move(resolved));
}

void_core::Result<std::vector<ResolvedPackage>> PackageResolver::resolve_all(
    const std::vector<std::string>& package_names) const {

    // Collect all packages and their dependencies
    std::vector<std::string> all_order;
    std::set<std::string> visited;
    std::set<std::string> in_stack;
    std::vector<std::string> current_path;

    for (const auto& name : package_names) {
        if (!has_package(name)) {
            return void_core::Err<std::vector<ResolvedPackage>>(
                void_core::Error(void_core::ErrorCode::NotFound,
                    "Package not found: " + name));
        }

        auto result = topological_visit(name, all_order, visited, in_stack, current_path);
        if (!result) {
            return void_core::Err<std::vector<ResolvedPackage>>(result.error());
        }
    }

    // Build resolved packages
    std::vector<ResolvedPackage> resolved;
    resolved.reserve(all_order.size());

    for (const auto& name : all_order) {
        auto pkg_it = m_available.find(name);
        if (pkg_it == m_available.end()) {
            continue;
        }

        ResolvedPackage rp;
        rp.manifest = pkg_it->second.manifest;
        rp.path = pkg_it->second.path;

        for (const auto& dep : rp.manifest.all_dependencies()) {
            auto dep_it = std::find(all_order.begin(), all_order.end(), dep.name);
            if (dep_it != all_order.end() && dep_it < std::find(all_order.begin(), all_order.end(), name)) {
                rp.resolved_deps.push_back(dep.name);
            } else if (dep.optional) {
                rp.missing_optional.push_back(dep.name);
            }
        }

        resolved.push_back(std::move(rp));
    }

    return void_core::Ok(std::move(resolved));
}

void_core::Result<void> PackageResolver::validate_acyclic() const {
    std::vector<std::string> order;
    std::set<std::string> visited;
    std::set<std::string> in_stack;
    std::vector<std::string> current_path;

    for (const auto& [name, _] : m_available) {
        if (!visited.count(name)) {
            auto result = topological_visit(name, order, visited, in_stack, current_path);
            if (!result) {
                return result;
            }
        }
    }

    return void_core::Ok();
}

void_core::Result<void> PackageResolver::validate_plugin_layers() const {
    for (const auto& [name, pkg] : m_available) {
        if (pkg.manifest.type != PackageType::Plugin) {
            continue;
        }

        int my_layer = pkg.manifest.plugin_layer_level();
        if (my_layer < 0) {
            continue;  // Unknown namespace, skip validation
        }

        for (const auto& dep : pkg.manifest.plugin_deps) {
            int dep_layer = get_plugin_layer_level(dep.name);
            if (dep_layer < 0) {
                continue;  // Unknown namespace
            }

            if (dep_layer > my_layer) {
                LayerViolation violation{name, my_layer, dep.name, dep_layer};
                return void_core::Err(void_core::Error(void_core::ErrorCode::ValidationError,
                    violation.format()));
            }
        }
    }

    return void_core::Ok();
}

void_core::Result<void> PackageResolver::validate_all() const {
    // Check for cycles
    auto cycle_result = validate_acyclic();
    if (!cycle_result) {
        return cycle_result;
    }

    // Check plugin layers
    auto layer_result = validate_plugin_layers();
    if (!layer_result) {
        return layer_result;
    }

    // Check all required dependencies exist
    for (const auto& [name, pkg] : m_available) {
        for (const auto& dep : pkg.manifest.all_dependencies()) {
            if (dep.optional) continue;

            std::string error;
            if (!is_dependency_satisfied(dep, &error)) {
                return void_core::Err(void_core::Error(void_core::ErrorCode::DependencyMissing,
                    "Package '" + name + "': " + error));
            }
        }
    }

    return void_core::Ok();
}

bool PackageResolver::has_package(const std::string& name) const {
    return m_available.count(name) > 0;
}

const PackageManifest* PackageResolver::get_manifest(const std::string& name) const {
    auto it = m_available.find(name);
    if (it == m_available.end()) {
        return nullptr;
    }
    return &it->second.manifest;
}

std::vector<std::string> PackageResolver::available_packages() const {
    std::vector<std::string> names;
    names.reserve(m_available.size());
    for (const auto& [name, _] : m_available) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> PackageResolver::packages_of_type(PackageType type) const {
    std::vector<std::string> names;
    for (const auto& [name, pkg] : m_available) {
        if (pkg.manifest.type == type) {
            names.push_back(name);
        }
    }
    return names;
}

std::vector<std::string> PackageResolver::get_dependents(const std::string& package_name) const {
    std::vector<std::string> dependents;

    for (const auto& [name, pkg] : m_available) {
        for (const auto& dep : pkg.manifest.all_dependencies()) {
            if (dep.name == package_name) {
                dependents.push_back(name);
                break;
            }
        }
    }

    return dependents;
}

std::vector<std::string> PackageResolver::get_dependencies(const std::string& package_name) const {
    auto it = m_available.find(package_name);
    if (it == m_available.end()) {
        return {};
    }

    auto all_deps = it->second.manifest.all_dependencies();
    std::vector<std::string> names;
    names.reserve(all_deps.size());

    for (const auto& dep : all_deps) {
        names.push_back(dep.name);
    }

    return names;
}

bool PackageResolver::would_create_cycle(
    const std::string& from_package,
    const std::string& to_package) const {

    if (from_package == to_package) {
        return true;
    }

    // Check if to_package transitively depends on from_package
    std::set<std::string> visited;
    std::vector<std::string> to_visit = {to_package};

    while (!to_visit.empty()) {
        std::string current = to_visit.back();
        to_visit.pop_back();

        if (visited.count(current)) {
            continue;
        }
        visited.insert(current);

        if (current == from_package) {
            return true;
        }

        auto it = m_available.find(current);
        if (it != m_available.end()) {
            for (const auto& dep : it->second.manifest.all_dependencies()) {
                if (!visited.count(dep.name)) {
                    to_visit.push_back(dep.name);
                }
            }
        }
    }

    return false;
}

std::string PackageResolver::to_dot_graph() const {
    std::ostringstream oss;
    oss << "digraph packages {\n";
    oss << "  rankdir=TB;\n";
    oss << "  node [shape=box];\n\n";

    // Define nodes with colors by type
    for (const auto& [name, pkg] : m_available) {
        const char* color = "white";
        switch (pkg.manifest.type) {
            case PackageType::World:  color = "lightblue"; break;
            case PackageType::Layer:  color = "lightyellow"; break;
            case PackageType::Plugin: color = "lightgreen"; break;
            case PackageType::Widget: color = "lightpink"; break;
            case PackageType::Asset:  color = "lightgray"; break;
        }
        oss << "  \"" << name << "\" [style=filled, fillcolor=" << color << "];\n";
    }
    oss << "\n";

    // Define edges
    for (const auto& [name, pkg] : m_available) {
        for (const auto& dep : pkg.manifest.all_dependencies()) {
            oss << "  \"" << name << "\" -> \"" << dep.name << "\"";
            if (dep.optional) {
                oss << " [style=dashed]";
            }
            oss << ";\n";
        }
    }

    oss << "}\n";
    return oss.str();
}

std::string PackageResolver::format_dependency_tree(const std::string& root) const {
    std::string output;
    std::set<std::string> visited;
    format_tree_recursive(root, output, "", visited);
    return output;
}

// =============================================================================
// Private Methods
// =============================================================================

void_core::Result<void> PackageResolver::topological_visit(
    const std::string& name,
    std::vector<std::string>& order,
    std::set<std::string>& visited,
    std::set<std::string>& in_stack,
    std::vector<std::string>& current_path) const {

    if (in_stack.count(name)) {
        // Cycle detected - build cycle path
        current_path.push_back(name);
        DependencyCycle cycle;
        cycle.cycle_path = current_path;
        return void_core::Err(void_core::Error(void_core::ErrorCode::ValidationError,
            cycle.format()));
    }

    if (visited.count(name)) {
        return void_core::Ok();
    }

    auto it = m_available.find(name);
    if (it == m_available.end()) {
        // Not found - this is an error for required deps, warning for optional
        return void_core::Err(void_core::Error(void_core::ErrorCode::NotFound,
            "Package not found: " + name));
    }

    in_stack.insert(name);
    current_path.push_back(name);

    // Visit all dependencies first
    for (const auto& dep : it->second.manifest.all_dependencies()) {
        if (!has_package(dep.name)) {
            if (!dep.optional) {
                return void_core::Err(void_core::Error(void_core::ErrorCode::DependencyMissing,
                    "Package '" + name + "' requires '" + dep.name + "' which is not available"));
            }
            continue;  // Skip optional missing dependencies
        }

        auto result = topological_visit(dep.name, order, visited, in_stack, current_path);
        if (!result) {
            return result;
        }
    }

    in_stack.erase(name);
    current_path.pop_back();
    visited.insert(name);
    order.push_back(name);

    return void_core::Ok();
}

bool PackageResolver::is_dependency_satisfied(
    const PackageDependency& dep,
    std::string* out_error) const {

    auto it = m_available.find(dep.name);
    if (it == m_available.end()) {
        if (out_error) {
            *out_error = "Dependency '" + dep.name + "' not found";
        }
        return false;
    }

    if (!dep.constraint.satisfies(it->second.manifest.version)) {
        if (out_error) {
            *out_error = "Dependency '" + dep.name + "' version " +
                it->second.manifest.version.to_string() +
                " does not satisfy constraint " + dep.constraint.to_string();
        }
        return false;
    }

    return true;
}

void PackageResolver::format_tree_recursive(
    const std::string& name,
    std::string& output,
    const std::string& prefix,
    std::set<std::string>& visited) const {

    bool already_visited = visited.count(name) > 0;
    visited.insert(name);

    auto it = m_available.find(name);
    if (it == m_available.end()) {
        output += prefix + name + " (NOT FOUND)\n";
        return;
    }

    const auto& pkg = it->second;
    output += prefix + name + " v" + pkg.manifest.version.to_string();
    output += " [" + std::string(package_type_to_string(pkg.manifest.type)) + "]";

    if (already_visited) {
        output += " (see above)\n";
        return;
    }
    output += "\n";

    auto deps = pkg.manifest.all_dependencies();
    for (std::size_t i = 0; i < deps.size(); ++i) {
        bool is_last = (i == deps.size() - 1);
        std::string new_prefix = prefix + (is_last ? "  " : "| ");
        std::string branch = prefix + (is_last ? "`-" : "|-");

        const auto& dep = deps[i];
        if (dep.optional) {
            output += branch + "(optional) ";
            new_prefix = prefix + (is_last ? "  " : "| ");
            format_tree_recursive(dep.name, output, new_prefix, visited);
        } else {
            output += branch;
            format_tree_recursive(dep.name, output, new_prefix, visited);
        }
    }
}

} // namespace void_package
