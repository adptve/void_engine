/// @file definition_registry.cpp
/// @brief Implementation of generic definition registry for data-driven content

#include <void_engine/package/definition_registry.hpp>

#include <sstream>
#include <algorithm>

namespace void_package {

// =============================================================================
// CollisionPolicy Utilities
// =============================================================================

const char* collision_policy_to_string(CollisionPolicy policy) noexcept {
    switch (policy) {
        case CollisionPolicy::Error: return "error";
        case CollisionPolicy::FirstWins: return "first_wins";
        case CollisionPolicy::LastWins: return "last_wins";
        case CollisionPolicy::Merge: return "merge";
        default: return "unknown";
    }
}

bool collision_policy_from_string(const std::string& str, CollisionPolicy& out) noexcept {
    if (str == "error") {
        out = CollisionPolicy::Error;
        return true;
    }
    if (str == "first_wins") {
        out = CollisionPolicy::FirstWins;
        return true;
    }
    if (str == "last_wins") {
        out = CollisionPolicy::LastWins;
        return true;
    }
    if (str == "merge") {
        out = CollisionPolicy::Merge;
        return true;
    }
    return false;
}

// =============================================================================
// RegistryTypeConfig
// =============================================================================

void_core::Result<RegistryTypeConfig> RegistryTypeConfig::from_json(const nlohmann::json& j) {
    RegistryTypeConfig config;

    if (j.contains("name") && j["name"].is_string()) {
        config.name = j["name"].get<std::string>();
    }

    if (j.contains("collision_policy") && j["collision_policy"].is_string()) {
        std::string policy_str = j["collision_policy"].get<std::string>();
        if (!collision_policy_from_string(policy_str, config.collision_policy)) {
            return void_core::Err<RegistryTypeConfig>("Invalid collision_policy: " + policy_str);
        }
    }

    if (j.contains("schema") && j["schema"].is_string()) {
        config.schema_path = j["schema"].get<std::string>();
    }

    if (j.contains("allow_dynamic_fields") && j["allow_dynamic_fields"].is_boolean()) {
        config.allow_dynamic_fields = j["allow_dynamic_fields"].get<bool>();
    }

    return void_core::Ok(std::move(config));
}

// =============================================================================
// DefinitionRegistry
// =============================================================================

void DefinitionRegistry::configure_type(const std::string& type_name, RegistryTypeConfig config) {
    auto& reg_data = m_registries[type_name];
    reg_data.config = std::move(config);
    reg_data.config.name = type_name;  // Ensure name matches key
}

void DefinitionRegistry::set_collision_policy(const std::string& type_name, CollisionPolicy policy) {
    auto& reg_data = m_registries[type_name];
    reg_data.config.collision_policy = policy;
}

CollisionPolicy DefinitionRegistry::get_collision_policy(const std::string& type_name) const {
    auto it = m_registries.find(type_name);
    if (it != m_registries.end()) {
        return it->second.config.collision_policy;
    }
    return m_default_policy;
}

void_core::Result<void> DefinitionRegistry::register_definition(
    const std::string& registry_type,
    const std::string& id,
    nlohmann::json data,
    DefinitionSource source)
{
    if (id.empty()) {
        return void_core::Err("Cannot register definition with empty ID");
    }

    auto& reg_data = m_registries[registry_type];
    CollisionPolicy policy = reg_data.config.collision_policy;

    // Check for existing definition
    auto existing_it = reg_data.definitions.find(id);
    if (existing_it != reg_data.definitions.end()) {
        switch (policy) {
            case CollisionPolicy::Error:
                return void_core::Err("Definition '" + id + "' already exists in registry '" +
                                       registry_type + "' (collision policy: error)");

            case CollisionPolicy::FirstWins:
                // Keep existing, ignore new
                return void_core::Ok();

            case CollisionPolicy::LastWins:
                // Replace with new (fall through)
                break;

            case CollisionPolicy::Merge:
                // Merge new data into existing
                for (auto& [key, value] : data.items()) {
                    existing_it->second.data[key] = value;
                }
                existing_it->second.source = std::move(source);
                return void_core::Ok();
        }
    }

    // Store definition
    StoredDefinition stored;
    stored.id = id;
    stored.data = std::move(data);
    stored.source = std::move(source);

    reg_data.definitions[id] = std::move(stored);

    // Track bundle
    if (!stored.source.bundle_name.empty()) {
        m_known_bundles.insert(stored.source.bundle_name);
    }

    return void_core::Ok();
}

void_core::Result<void> DefinitionRegistry::register_definition(
    const std::string& registry_type,
    const std::string& id,
    nlohmann::json data,
    const std::string& bundle_name)
{
    DefinitionSource source;
    source.bundle_name = bundle_name;
    return register_definition(registry_type, id, std::move(data), std::move(source));
}

void_core::Result<void> DefinitionRegistry::register_definitions(
    const std::string& registry_type,
    const std::vector<std::pair<std::string, nlohmann::json>>& definitions,
    const std::string& bundle_name)
{
    for (const auto& [id, data] : definitions) {
        auto result = register_definition(registry_type, id, data, bundle_name);
        if (!result) {
            return result;
        }
    }
    return void_core::Ok();
}

bool DefinitionRegistry::unregister_definition(const std::string& registry_type, const std::string& id) {
    auto reg_it = m_registries.find(registry_type);
    if (reg_it == m_registries.end()) {
        return false;
    }
    return reg_it->second.definitions.erase(id) > 0;
}

std::size_t DefinitionRegistry::unregister_bundle(const std::string& bundle_name) {
    std::size_t count = 0;

    for (auto& [type, reg_data] : m_registries) {
        for (auto it = reg_data.definitions.begin(); it != reg_data.definitions.end();) {
            if (it->second.source.bundle_name == bundle_name) {
                it = reg_data.definitions.erase(it);
                ++count;
            } else {
                ++it;
            }
        }
    }

    m_known_bundles.erase(bundle_name);
    return count;
}

std::size_t DefinitionRegistry::unregister_type(const std::string& registry_type) {
    auto it = m_registries.find(registry_type);
    if (it == m_registries.end()) {
        return 0;
    }

    std::size_t count = it->second.definitions.size();
    m_registries.erase(it);
    return count;
}

void DefinitionRegistry::clear() {
    m_registries.clear();
    m_known_bundles.clear();
}

std::optional<nlohmann::json> DefinitionRegistry::get_definition(
    const std::string& registry_type,
    const std::string& id) const
{
    auto reg_it = m_registries.find(registry_type);
    if (reg_it == m_registries.end()) {
        return std::nullopt;
    }

    auto def_it = reg_it->second.definitions.find(id);
    if (def_it == reg_it->second.definitions.end()) {
        return std::nullopt;
    }

    return def_it->second.data;
}

const StoredDefinition* DefinitionRegistry::get_definition_full(
    const std::string& registry_type,
    const std::string& id) const
{
    auto reg_it = m_registries.find(registry_type);
    if (reg_it == m_registries.end()) {
        return nullptr;
    }

    auto def_it = reg_it->second.definitions.find(id);
    if (def_it == reg_it->second.definitions.end()) {
        return nullptr;
    }

    return &def_it->second;
}

bool DefinitionRegistry::has_definition(
    const std::string& registry_type,
    const std::string& id) const
{
    auto reg_it = m_registries.find(registry_type);
    if (reg_it == m_registries.end()) {
        return false;
    }
    return reg_it->second.definitions.count(id) > 0;
}

std::vector<std::string> DefinitionRegistry::list_definitions(
    const std::string& registry_type) const
{
    std::vector<std::string> ids;

    auto reg_it = m_registries.find(registry_type);
    if (reg_it == m_registries.end()) {
        return ids;
    }

    ids.reserve(reg_it->second.definitions.size());
    for (const auto& [id, _] : reg_it->second.definitions) {
        ids.push_back(id);
    }

    return ids;
}

std::vector<std::string> DefinitionRegistry::list_registry_types() const {
    std::vector<std::string> types;
    types.reserve(m_registries.size());
    for (const auto& [type, _] : m_registries) {
        types.push_back(type);
    }
    return types;
}

std::vector<StoredDefinition> DefinitionRegistry::all_definitions(
    const std::string& registry_type) const
{
    std::vector<StoredDefinition> defs;

    auto reg_it = m_registries.find(registry_type);
    if (reg_it == m_registries.end()) {
        return defs;
    }

    defs.reserve(reg_it->second.definitions.size());
    for (const auto& [_, def] : reg_it->second.definitions) {
        defs.push_back(def);
    }

    return defs;
}

std::size_t DefinitionRegistry::definition_count(const std::string& registry_type) const {
    auto reg_it = m_registries.find(registry_type);
    if (reg_it == m_registries.end()) {
        return 0;
    }
    return reg_it->second.definitions.size();
}

std::size_t DefinitionRegistry::total_definition_count() const {
    std::size_t total = 0;
    for (const auto& [_, reg_data] : m_registries) {
        total += reg_data.definitions.size();
    }
    return total;
}

bool DefinitionRegistry::has_registry_type(const std::string& registry_type) const {
    auto it = m_registries.find(registry_type);
    return it != m_registries.end() && !it->second.definitions.empty();
}

void DefinitionRegistry::for_each(
    const std::string& registry_type,
    const std::function<void(const std::string& id, const nlohmann::json& data)>& callback) const
{
    auto reg_it = m_registries.find(registry_type);
    if (reg_it == m_registries.end()) {
        return;
    }

    for (const auto& [id, def] : reg_it->second.definitions) {
        callback(id, def.data);
    }
}

void DefinitionRegistry::for_each_all(
    const std::function<void(const std::string& type, const std::string& id,
                              const nlohmann::json& data)>& callback) const
{
    for (const auto& [type, reg_data] : m_registries) {
        for (const auto& [id, def] : reg_data.definitions) {
            callback(type, id, def.data);
        }
    }
}

std::string DefinitionRegistry::format_state() const {
    std::ostringstream ss;
    ss << "DefinitionRegistry:\n";
    ss << "  Registry types: " << m_registries.size() << "\n";
    ss << "  Total definitions: " << total_definition_count() << "\n";
    ss << "  Known bundles: " << m_known_bundles.size() << "\n";
    ss << "  Default policy: " << collision_policy_to_string(m_default_policy) << "\n";

    if (!m_registries.empty()) {
        ss << "\n  Types:\n";
        for (const auto& [type, reg_data] : m_registries) {
            ss << "    " << type << ": " << reg_data.definitions.size() << " definitions";
            if (reg_data.config.collision_policy != m_default_policy) {
                ss << " (policy: " << collision_policy_to_string(reg_data.config.collision_policy) << ")";
            }
            ss << "\n";

            // List first few definitions
            int count = 0;
            for (const auto& [id, _] : reg_data.definitions) {
                if (count++ >= 5) {
                    ss << "      ... and " << (reg_data.definitions.size() - 5) << " more\n";
                    break;
                }
                ss << "      - " << id << "\n";
            }
        }
    }

    return ss.str();
}

DefinitionRegistry::Stats DefinitionRegistry::get_stats() const {
    Stats stats;
    stats.registry_types = m_registries.size();
    stats.bundles = m_known_bundles.size();

    for (const auto& [type, reg_data] : m_registries) {
        std::size_t count = reg_data.definitions.size();
        stats.definitions_per_type[type] = count;
        stats.total_definitions += count;
    }

    return stats;
}

} // namespace void_package
