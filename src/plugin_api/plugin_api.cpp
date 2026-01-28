/// @file plugin_api.cpp
/// @brief Implementation of plugin API and gameplay plugin base class

#include <void_engine/plugin_api/plugin_api.hpp>

namespace void_plugin_api {

// =============================================================================
// GameplayPlugin Implementation
// =============================================================================

void_core::Result<void> GameplayPlugin::on_load(void_core::PluginContext& ctx) {
    // Get plugin API from context
    auto* api_ptr = ctx.get<IPluginAPI*>("plugin_api");
    if (api_ptr) {
        m_api = *api_ptr;
    }

    // Call derived class initialization
    on_plugin_load(m_api);

    return void_core::Ok();
}

void_core::Result<void_core::PluginState> GameplayPlugin::on_unload(void_core::PluginContext& ctx) {
    (void)ctx;

    // Call derived class cleanup
    on_plugin_unload();

    // Serialize runtime state for potential hot-reload
    auto runtime_data = serialize_runtime_state();

    void_core::PluginState state;
    state.data = std::move(runtime_data);
    state.type_name = type_name();
    state.version = version();

    m_api = nullptr;

    return void_core::Ok(std::move(state));
}

void_core::Result<void> GameplayPlugin::on_reload(void_core::PluginContext& ctx, void_core::PluginState state) {
    // Restore API reference
    auto* api_ptr = ctx.get<IPluginAPI*>("plugin_api");
    if (api_ptr) {
        m_api = *api_ptr;
    }

    // Restore runtime state
    if (!state.data.empty()) {
        deserialize_runtime_state(state.data);
    }

    // Call derived class initialization
    on_plugin_load(m_api);

    return void_core::Ok();
}

void_core::Result<void_core::HotReloadSnapshot> GameplayPlugin::snapshot() {
    auto runtime_data = serialize_runtime_state();

    void_core::HotReloadSnapshot snap(
        std::move(runtime_data),
        std::type_index(typeid(*this)),
        type_name(),
        version()
    );

    return void_core::Ok(std::move(snap));
}

void_core::Result<void> GameplayPlugin::restore(void_core::HotReloadSnapshot snapshot) {
    if (!snapshot.data.empty()) {
        deserialize_runtime_state(snapshot.data);
    }
    return void_core::Ok();
}

bool GameplayPlugin::is_compatible(const void_core::Version& new_version) const {
    // Compatible if major version matches
    return new_version.major == version().major;
}

// =============================================================================
// PluginAPIImpl Implementation
// =============================================================================

PluginAPIImpl::PluginAPIImpl(AIStateStore& ai, CombatStateStore& combat,
                             InventoryStateStore& inventory, CommandProcessor& processor)
    : m_ai_state(ai)
    , m_combat_state(combat)
    , m_inventory_state(inventory)
    , m_command_processor(processor) {}

const AIStateStore& PluginAPIImpl::ai_state() const {
    return m_ai_state;
}

const CombatStateStore& PluginAPIImpl::combat_state() const {
    return m_combat_state;
}

const InventoryStateStore& PluginAPIImpl::inventory_state() const {
    return m_inventory_state;
}

CommandResult PluginAPIImpl::submit_command(CommandPtr command) {
    CommandContext ctx;
    ctx.timestamp = m_current_time;
    ctx.frame = m_frame_number;
    ctx.can_undo = true;
    return m_command_processor.execute(std::move(command), ctx);
}

void PluginAPIImpl::queue_command(CommandPtr command) {
    CommandContext ctx;
    ctx.timestamp = m_current_time;
    ctx.frame = m_frame_number;
    ctx.can_undo = true;
    m_command_processor.queue(std::move(command), ctx);
}

double PluginAPIImpl::current_time() const {
    return m_current_time;
}

float PluginAPIImpl::delta_time() const {
    return m_delta_time;
}

std::uint32_t PluginAPIImpl::frame_number() const {
    return m_frame_number;
}

bool PluginAPIImpl::is_paused() const {
    return m_paused;
}

bool PluginAPIImpl::entity_exists(EntityId entity) const {
    // Check if entity has any state in any store
    return m_ai_state.entity_blackboards.count(entity) > 0 ||
           m_combat_state.entity_vitals.count(entity) > 0 ||
           m_inventory_state.entity_inventories.count(entity) > 0;
}

Vec3 PluginAPIImpl::get_entity_position(EntityId entity) const {
    // Try to get position from nav state
    auto it = m_ai_state.nav_states.find(entity);
    if (it != m_ai_state.nav_states.end()) {
        return it->second.current_position;
    }
    return Vec3{0, 0, 0};
}

std::vector<EntityId> PluginAPIImpl::get_entities_in_radius(Vec3 center, float radius) const {
    std::vector<EntityId> result;
    float radius_sq = radius * radius;

    // Check all entities with nav states
    for (const auto& [entity, nav] : m_ai_state.nav_states) {
        float dx = nav.current_position.x - center.x;
        float dy = nav.current_position.y - center.y;
        float dz = nav.current_position.z - center.z;
        float dist_sq = dx * dx + dy * dy + dz * dz;
        if (dist_sq <= radius_sq) {
            result.push_back(entity);
        }
    }

    return result;
}

void PluginAPIImpl::emit_event(const std::string& event_name, const std::any& data) {
    auto it = m_event_subscriptions.find(event_name);
    if (it != m_event_subscriptions.end()) {
        for (auto& callback : it->second) {
            callback(data);
        }
    }
}

void PluginAPIImpl::subscribe_event(const std::string& event_name, EventCallback callback) {
    m_event_subscriptions[event_name].push_back(std::move(callback));
}

} // namespace void_plugin_api
