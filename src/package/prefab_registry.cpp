/// @file prefab_registry.cpp
/// @brief Implementation of prefab registry for runtime entity instantiation

#include <void_engine/package/prefab_registry.hpp>
#include <void_engine/package/component_schema.hpp>
#include <void_engine/ecs/world.hpp>

#include <spdlog/spdlog.h>

#include <sstream>

namespace void_package {

// =============================================================================
// TransformData
// =============================================================================

std::optional<TransformData> TransformData::from_json(const nlohmann::json& j) {
    TransformData data;

    if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 3) {
        data.position[0] = j["position"][0].get<float>();
        data.position[1] = j["position"][1].get<float>();
        data.position[2] = j["position"][2].get<float>();
    }

    if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() >= 4) {
        data.rotation[0] = j["rotation"][0].get<float>();
        data.rotation[1] = j["rotation"][1].get<float>();
        data.rotation[2] = j["rotation"][2].get<float>();
        data.rotation[3] = j["rotation"][3].get<float>();
    }

    if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
        data.scale[0] = j["scale"][0].get<float>();
        data.scale[1] = j["scale"][1].get<float>();
        data.scale[2] = j["scale"][2].get<float>();
    }

    return data;
}

nlohmann::json TransformData::to_json() const {
    return nlohmann::json{
        {"position", {position[0], position[1], position[2]}},
        {"rotation", {rotation[0], rotation[1], rotation[2], rotation[3]}},
        {"scale", {scale[0], scale[1], scale[2]}}
    };
}

// =============================================================================
// InstantiationContext
// =============================================================================

void_core::Result<void> InstantiationContext::validate() const {
    if (!world) {
        return void_core::Err("InstantiationContext: ECS world is null");
    }
    return void_core::Ok();
}

// =============================================================================
// PrefabRegistry
// =============================================================================

void_core::Result<void> PrefabRegistry::register_prefab(PrefabDefinition definition) {
    if (definition.id.empty()) {
        return void_core::Err("Cannot register prefab with empty ID");
    }

    if (m_prefabs.count(definition.id) > 0) {
        return void_core::Err("Prefab '" + definition.id + "' already registered");
    }

    m_prefabs[definition.id] = std::move(definition);
    return void_core::Ok();
}

void PrefabRegistry::register_prefab_overwrite(PrefabDefinition definition) {
    if (definition.id.empty()) {
        return;  // Silently ignore empty IDs
    }
    m_prefabs[definition.id] = std::move(definition);
}

bool PrefabRegistry::unregister_prefab(const std::string& prefab_id) {
    return m_prefabs.erase(prefab_id) > 0;
}

std::size_t PrefabRegistry::unregister_bundle(const std::string& bundle_name) {
    std::size_t count = 0;
    for (auto it = m_prefabs.begin(); it != m_prefabs.end();) {
        if (it->second.source_bundle == bundle_name) {
            it = m_prefabs.erase(it);
            ++count;
        } else {
            ++it;
        }
    }
    return count;
}

void PrefabRegistry::clear() {
    m_prefabs.clear();
}

void PrefabRegistry::register_instantiator(const std::string& component_name,
                                            ComponentInstantiator instantiator) {
    m_instantiators[component_name] = std::move(instantiator);
}

bool PrefabRegistry::has_instantiator(const std::string& component_name) const {
    return m_instantiators.count(component_name) > 0;
}

const PrefabDefinition* PrefabRegistry::get(const std::string& prefab_id) const {
    auto it = m_prefabs.find(prefab_id);
    return it != m_prefabs.end() ? &it->second : nullptr;
}

bool PrefabRegistry::contains(const std::string& prefab_id) const {
    return m_prefabs.count(prefab_id) > 0;
}

std::vector<std::string> PrefabRegistry::all_prefab_ids() const {
    std::vector<std::string> ids;
    ids.reserve(m_prefabs.size());
    for (const auto& [id, _] : m_prefabs) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<std::string> PrefabRegistry::prefabs_from_bundle(
    const std::string& bundle_name) const
{
    std::vector<std::string> ids;
    for (const auto& [id, def] : m_prefabs) {
        if (def.source_bundle == bundle_name) {
            ids.push_back(id);
        }
    }
    return ids;
}

void_core::Result<void_ecs::Entity> PrefabRegistry::instantiate(
    const std::string& prefab_id,
    void_ecs::World& world,
    const std::optional<TransformData>& transform_override)
{
    spdlog::info("[PrefabRegistry] instantiate('{}') called, this={}", prefab_id, static_cast<void*>(this));
    spdlog::default_logger()->flush();

    spdlog::info("[PrefabRegistry] Creating InstantiationContext...");
    spdlog::default_logger()->flush();

    InstantiationContext ctx;
    ctx.world = &world;
    ctx.schema_registry = m_schema_registry;
    ctx.transform_override = transform_override;

    spdlog::info("[PrefabRegistry] Calling instantiate_with_context...");
    spdlog::default_logger()->flush();

    auto result = instantiate_with_context(prefab_id, ctx);
    if (!result) {
        spdlog::warn("[PrefabRegistry] instantiate_with_context failed: {}", result.error().message());
        return void_core::Err<void_ecs::Entity>(result.error());
    }

    spdlog::info("[PrefabRegistry] instantiate succeeded, entity index: {}", result->entity.index);
    return void_core::Ok(result->entity);
}

void_core::Result<InstantiationResult> PrefabRegistry::instantiate_with_context(
    const std::string& prefab_id,
    InstantiationContext& ctx)
{
    spdlog::info("[PrefabRegistry] instantiate_with_context('{}') called", prefab_id);
    spdlog::default_logger()->flush();

    // Validate context
    spdlog::info("[PrefabRegistry] Validating context...");
    spdlog::default_logger()->flush();
    auto ctx_valid = ctx.validate();
    if (!ctx_valid) {
        return void_core::Err<InstantiationResult>(ctx_valid.error());
    }

    // Find prefab
    spdlog::info("[PrefabRegistry] Looking up prefab '{}', prefab count: {}", prefab_id, m_prefabs.size());
    spdlog::default_logger()->flush();
    const PrefabDefinition* prefab = get(prefab_id);
    if (!prefab) {
        spdlog::warn("[PrefabRegistry] Prefab '{}' not found!", prefab_id);
        return void_core::Err<InstantiationResult>("Prefab not found: " + prefab_id);
    }
    spdlog::info("[PrefabRegistry] Found prefab '{}', spawning entity...", prefab_id);
    spdlog::default_logger()->flush();

    // Spawn entity
    void_ecs::Entity entity = ctx.world->spawn();

    InstantiationResult result;
    result.entity = entity;

    // Apply each component
    for (const auto& [comp_name, comp_data] : prefab->components) {
        // Handle Transform specially if we have an override
        nlohmann::json data_to_use = comp_data;
        if (comp_name == "Transform" && ctx.transform_override) {
            // Merge transform override into component data
            data_to_use = ctx.transform_override->to_json();
            // Keep any extra fields from original
            for (auto& [key, val] : comp_data.items()) {
                if (!data_to_use.contains(key)) {
                    data_to_use[key] = val;
                }
            }
        }

        auto apply_result = apply_component(*ctx.world, entity, comp_name, data_to_use);
        if (!apply_result) {
            // Handle according to policy
            switch (m_unknown_policy) {
                case UnknownComponentPolicy::Error:
                    // Cleanup and fail
                    ctx.world->despawn(entity);
                    return void_core::Err<InstantiationResult>("Failed to apply component '" + comp_name +
                                           "': " + apply_result.error().message());

                case UnknownComponentPolicy::Skip:
                    result.skipped_components.push_back(comp_name);
                    break;

                case UnknownComponentPolicy::Defer:
                    result.skipped_components.push_back(comp_name);
                    break;
            }
        } else if (*apply_result) {
            result.applied_components.push_back(comp_name);
        } else {
            result.skipped_components.push_back(comp_name);
        }
    }

    return void_core::Ok(std::move(result));
}

void_core::Result<void_ecs::Entity> PrefabRegistry::instantiate_definition(
    const PrefabDefinition& definition,
    void_ecs::World& world,
    const std::optional<TransformData>& transform_override)
{
    InstantiationContext ctx;
    ctx.world = &world;
    ctx.schema_registry = m_schema_registry;
    ctx.transform_override = transform_override;

    auto ctx_valid = ctx.validate();
    if (!ctx_valid) {
        return void_core::Err<void_ecs::Entity>(ctx_valid.error());
    }

    void_ecs::Entity entity = world.spawn();

    for (const auto& [comp_name, comp_data] : definition.components) {
        nlohmann::json data_to_use = comp_data;
        if (comp_name == "Transform" && ctx.transform_override) {
            data_to_use = ctx.transform_override->to_json();
            for (auto& [key, val] : comp_data.items()) {
                if (!data_to_use.contains(key)) {
                    data_to_use[key] = val;
                }
            }
        }

        auto apply_result = apply_component(world, entity, comp_name, data_to_use);
        if (!apply_result) {
            if (m_unknown_policy == UnknownComponentPolicy::Error) {
                world.despawn(entity);
                return void_core::Err<void_ecs::Entity>("Failed to apply component '" + comp_name +
                                       "': " + apply_result.error().message());
            }
        }
    }

    return void_core::Ok(entity);
}

void_core::Result<bool> PrefabRegistry::apply_component(
    void_ecs::World& world,
    void_ecs::Entity entity,
    const std::string& component_name,
    const nlohmann::json& component_data)
{
    spdlog::debug("[PrefabRegistry] apply_component '{}' to entity {}v{} (schema_registry: {})",
                  component_name, entity.index, entity.generation,
                  static_cast<void*>(m_schema_registry));

    // First, try registered instantiator
    auto inst_it = m_instantiators.find(component_name);
    if (inst_it != m_instantiators.end()) {
        spdlog::debug("[PrefabRegistry] Using custom instantiator for '{}'", component_name);
        auto result = inst_it->second(component_data, world, entity);
        if (!result) {
            return void_core::Err<bool>(result.error());
        }
        return void_core::Ok(true);
    }

    // Second, try schema registry
    if (m_schema_registry) {
        // Check if the schema is registered before attempting to apply
        bool has_schema = m_schema_registry->has_schema(component_name);
        spdlog::debug("[PrefabRegistry] Schema registry has '{}': {}", component_name, has_schema);

        if (has_schema) {
            auto result = m_schema_registry->apply_to_entity(world, entity, component_name, component_data);
            if (result) {
                spdlog::debug("[PrefabRegistry] Successfully applied '{}' via schema registry", component_name);
                return void_core::Ok(true);
            }
            // Schema exists but application failed - propagate the actual error
            spdlog::error("[PrefabRegistry] Schema application failed for '{}': {}",
                         component_name, result.error().message());
            return void_core::Err<bool>("Schema application failed for '" + component_name +
                                   "': " + result.error().message());
        }
        // Schema not found in schema registry - check ECS registry for diagnostic purposes
        spdlog::warn("[PrefabRegistry] Component '{}' not found in schema registry", component_name);
    } else {
        spdlog::warn("[PrefabRegistry] No schema registry configured");
    }

    // Third, try ECS component registry by name
    auto comp_id = world.component_id_by_name(component_name);
    if (!comp_id) {
        // Component not registered anywhere
        std::string diag_msg = "Unknown component: " + component_name;
        if (m_schema_registry) {
            // Provide diagnostic information about what IS registered
            auto all_schemas = m_schema_registry->all_schema_names();
            if (!all_schemas.empty()) {
                diag_msg += " (schema registry has " + std::to_string(all_schemas.size()) + " schemas";
                if (all_schemas.size() <= 10) {
                    diag_msg += ": ";
                    for (std::size_t i = 0; i < all_schemas.size(); ++i) {
                        if (i > 0) diag_msg += ", ";
                        diag_msg += all_schemas[i];
                    }
                }
                diag_msg += ")";
            } else {
                diag_msg += " (schema registry is EMPTY - plugins may not have loaded)";
            }
        }
        return void_core::Err<bool>(diag_msg);
    }

    // Component is in ECS registry but not in schema registry
    // This indicates a registration path that bypassed the schema system.
    // Possible causes:
    // 1. Plugin loader used internal registry instead of external/shared registry
    // 2. Component was registered directly to ECS without going through ComponentSchemaRegistry
    // 3. Different schema registry instances are being used
    std::string diag_msg = "Component '" + component_name + "' found in ECS registry (id=" +
                           std::to_string(comp_id->value()) + ") but has no JSON instantiator in schema registry";
    if (m_schema_registry) {
        auto all_schemas = m_schema_registry->all_schema_names();
        diag_msg += " (schema registry has " + std::to_string(all_schemas.size()) + " schemas";
        if (!all_schemas.empty() && all_schemas.size() <= 10) {
            diag_msg += ": ";
            for (std::size_t i = 0; i < all_schemas.size(); ++i) {
                if (i > 0) diag_msg += ", ";
                diag_msg += all_schemas[i];
            }
        }
        diag_msg += ")";
    }
    return void_core::Err<bool>(diag_msg);
}

std::string PrefabRegistry::format_state() const {
    std::ostringstream ss;
    ss << "PrefabRegistry:\n";
    ss << "  Prefabs: " << m_prefabs.size() << "\n";
    ss << "  Instantiators: " << m_instantiators.size() << "\n";
    ss << "  Unknown policy: ";
    switch (m_unknown_policy) {
        case UnknownComponentPolicy::Error: ss << "Error"; break;
        case UnknownComponentPolicy::Skip: ss << "Skip"; break;
        case UnknownComponentPolicy::Defer: ss << "Defer"; break;
    }
    ss << "\n";

    if (!m_prefabs.empty()) {
        ss << "  Registered prefabs:\n";
        for (const auto& [id, def] : m_prefabs) {
            ss << "    - " << id << " (" << def.component_count() << " components";
            if (!def.source_bundle.empty()) {
                ss << ", from " << def.source_bundle;
            }
            ss << ")\n";
        }
    }

    if (!m_instantiators.empty()) {
        ss << "  Registered instantiators:\n";
        for (const auto& [name, _] : m_instantiators) {
            ss << "    - " << name << "\n";
        }
    }

    return ss.str();
}

} // namespace void_package
