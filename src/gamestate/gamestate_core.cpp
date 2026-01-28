/// @file gamestate_core.cpp
/// @brief Implementation of GameStateCore

#include <void_engine/gamestate/gamestate_core.hpp>

#include <cstring>

namespace void_gamestate {

// =============================================================================
// GameStateCore Implementation
// =============================================================================

GameStateCore::GameStateCore()
    : GameStateSystem() {}

GameStateCore::GameStateCore(const GameStateCoreConfig& config)
    : GameStateSystem(config.base_config)
    , m_core_config(config) {}

GameStateCore::~GameStateCore() {
    if (m_core_initialized) {
        shutdown();
    }
}

void GameStateCore::initialize() {
    // Initialize base system
    GameStateSystem::initialize();

    // Setup command processor
    setup_command_processor();

    // Setup plugin API
    setup_plugin_api();

    m_core_initialized = true;
}

void GameStateCore::shutdown() {
    // Stop plugin watcher first
    stop_watching();
    m_watcher.reset();

    // Unload all dynamically loaded plugins
    for (auto& [name, loaded] : m_loaded_plugins) {
        if (loaded->plugin) {
            loaded->plugin->on_plugin_unload();
        }
    }
    m_loaded_plugins.clear();

    // Unload all registry plugins
    m_plugin_registry.unload_all(m_type_registry);

    // Clear custom plugin state
    for (const auto& plugin_name : m_state_registry.registered_plugins()) {
        m_state_registry.unregister_plugin(plugin_name);
    }

    // Clear path mappings
    m_path_to_plugin.clear();
    m_plugin_to_path.clear();

    // Clear state stores
    m_ai_state.clear();
    m_combat_state.clear();
    m_inventory_state.clear();

    // Shutdown base system
    GameStateSystem::shutdown();

    m_core_initialized = false;
}

void GameStateCore::setup_command_processor() {
    m_command_processor = std::make_unique<void_plugin_api::CommandProcessor>(
        m_ai_state, m_combat_state, m_inventory_state
    );

    // Register command callbacks for events
    m_command_processor->on_command([this](const void_plugin_api::IStateCommand& cmd,
                                           void_plugin_api::CommandResult result) {
        // Could log commands, emit events, etc.
        if (m_core_config.log_commands) {
            // spdlog::debug("Command {}: {}", cmd.type_name(),
            //               result == void_plugin_api::CommandResult::Success ? "success" : "failed");
        }
    });
}

void GameStateCore::setup_plugin_api() {
    m_plugin_api = std::make_unique<void_plugin_api::PluginAPIImpl>(
        m_ai_state, m_combat_state, m_inventory_state, *m_command_processor
    );
}

// =============================================================================
// Command Processing
// =============================================================================

void_plugin_api::CommandResult GameStateCore::execute_command(void_plugin_api::CommandPtr command) {
    void_plugin_api::CommandContext ctx;
    ctx.timestamp = current_time();
    ctx.frame = m_frame_number;
    ctx.can_undo = true;

    return m_command_processor->execute(std::move(command), ctx);
}

void GameStateCore::queue_command(void_plugin_api::CommandPtr command) {
    void_plugin_api::CommandContext ctx;
    ctx.timestamp = current_time();
    ctx.frame = m_frame_number;
    ctx.can_undo = true;

    m_command_processor->queue(std::move(command), ctx);
}

void GameStateCore::process_commands() {
    m_command_processor->process_queue();
}

// =============================================================================
// Plugin Management
// =============================================================================

void_core::Result<void> GameStateCore::register_plugin(
    std::unique_ptr<void_plugin_api::GameplayPlugin> plugin) {
    if (!plugin) {
        return void_core::Err(void_core::Error("Cannot register null plugin"));
    }

    return m_plugin_registry.register_plugin(std::move(plugin));
}

void_core::Result<void> GameStateCore::load_plugin(const std::string& name) {
    void_core::PluginId id(name);

    // Add plugin API to context
    void_core::PluginContext ctx;
    ctx.types = &m_type_registry;
    ctx.plugins = &m_plugin_registry;
    ctx.insert("plugin_api", static_cast<void_plugin_api::IPluginAPI*>(m_plugin_api.get()));

    return m_plugin_registry.load(id, m_type_registry);
}

void_core::Result<void> GameStateCore::unload_plugin(const std::string& name) {
    void_core::PluginId id(name);
    auto result = m_plugin_registry.unload(id, m_type_registry);
    if (!result) {
        return void_core::Err(result.error());
    }
    return void_core::Ok();
}

void_core::Result<void> GameStateCore::hot_reload_plugin(
    const std::string& name,
    std::unique_ptr<void_plugin_api::GameplayPlugin> new_plugin) {
    void_core::PluginId id(name);
    return m_plugin_registry.hot_reload(id, std::move(new_plugin), m_type_registry);
}

std::size_t GameStateCore::active_plugin_count() const {
    // Count both registry plugins and dynamically loaded plugins
    return m_plugin_registry.active_count() + m_loaded_plugins.size();
}

void GameStateCore::update_plugins(float dt) {
    m_delta_time = dt;
    m_plugin_api->set_delta_time(dt);
    m_plugin_api->set_frame_number(m_frame_number);
    m_plugin_api->set_time(current_time());
    m_plugin_api->set_paused(state_machine().is_paused());

    // Update registry plugins
    m_plugin_registry.for_each_active([dt](void_core::Plugin& plugin) {
        if (auto* gameplay = dynamic_cast<void_plugin_api::GameplayPlugin*>(&plugin)) {
            gameplay->on_tick(dt);
        }
    });

    // Update dynamically loaded plugins
    for (const auto& [name, loaded] : m_loaded_plugins) {
        if (loaded->plugin) {
            loaded->plugin->on_tick(dt);
        }
    }
}

void GameStateCore::fixed_update_plugins(float fixed_dt) {
    // Fixed update registry plugins
    m_plugin_registry.for_each_active([fixed_dt](void_core::Plugin& plugin) {
        if (auto* gameplay = dynamic_cast<void_plugin_api::GameplayPlugin*>(&plugin)) {
            gameplay->on_fixed_tick(fixed_dt);
        }
    });

    // Fixed update dynamically loaded plugins
    for (const auto& [name, loaded] : m_loaded_plugins) {
        if (loaded->plugin) {
            loaded->plugin->on_fixed_tick(fixed_dt);
        }
    }
}

// =============================================================================
// Update
// =============================================================================

void GameStateCore::update(float dt) {
    // Update base system
    GameStateSystem::update(dt);

    // Increment frame
    ++m_frame_number;
    m_delta_time = dt;

    // Process queued commands
    process_commands();

    // Update projectiles
    for (auto it = m_combat_state.active_projectiles.begin();
         it != m_combat_state.active_projectiles.end(); ) {
        it->elapsed += dt;
        it->position.x += it->velocity.x * dt;
        it->position.y += it->velocity.y * dt;
        it->position.z += it->velocity.z * dt;

        if (it->elapsed >= it->lifetime) {
            it = m_combat_state.active_projectiles.erase(it);
        } else {
            ++it;
        }
    }

    // Update status effects
    for (auto& [entity, effects] : m_combat_state.status_effects) {
        for (auto it = effects.begin(); it != effects.end(); ) {
            it->remaining -= dt;
            if (!it->permanent && it->remaining <= 0) {
                it = effects.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Update crafting queues
    for (auto& [entity, crafting] : m_inventory_state.crafting_queues) {
        if (!crafting.queue.empty() && !crafting.queue[0].paused) {
            auto& entry = crafting.queue[0];
            entry.progress += dt * crafting.craft_speed_multiplier;
            if (entry.progress >= entry.total_time) {
                // Crafting complete - in production would create item
                crafting.queue.erase(crafting.queue.begin());
            }
        }
    }

    // Update plugins
    update_plugins(dt);
}

void GameStateCore::fixed_update(float fixed_dt) {
    fixed_update_plugins(fixed_dt);
}

// =============================================================================
// State Management
// =============================================================================

void GameStateCore::clear_gameplay_state() {
    m_ai_state.clear();
    m_combat_state.clear();
    m_inventory_state.clear();
}

void GameStateCore::clear_entity_state(void_plugin_api::EntityId entity) {
    m_ai_state.entity_blackboards.erase(entity);
    m_ai_state.tree_states.erase(entity);
    m_ai_state.nav_states.erase(entity);
    m_ai_state.perception_states.erase(entity);

    m_combat_state.entity_vitals.erase(entity);
    m_combat_state.status_effects.erase(entity);
    m_combat_state.combat_stats.erase(entity);
    m_combat_state.damage_history.erase(entity);

    m_inventory_state.entity_inventories.erase(entity);
    m_inventory_state.equipment.erase(entity);
    m_inventory_state.crafting_queues.erase(entity);
}

void GameStateCore::register_entity(void_plugin_api::EntityId entity) {
    // Initialize default state for entity
    m_ai_state.entity_blackboards[entity] = void_plugin_api::BlackboardData{};

    void_plugin_api::VitalsState vitals;
    vitals.current_health = 100.0f;
    vitals.max_health = 100.0f;
    vitals.alive = true;
    m_combat_state.entity_vitals[entity] = vitals;

    m_combat_state.combat_stats[entity] = void_plugin_api::CombatStats{};

    void_plugin_api::InventoryData inv;
    inv.capacity = 20;
    inv.slots.resize(inv.capacity);
    for (std::uint32_t i = 0; i < inv.capacity; ++i) {
        inv.slots[i].index = i;
    }
    m_inventory_state.entity_inventories[entity] = std::move(inv);
}

void GameStateCore::unregister_entity(void_plugin_api::EntityId entity) {
    clear_entity_state(entity);
}

// =============================================================================
// Serialization
// =============================================================================

std::vector<std::uint8_t> GameStateCore::serialize_state() const {
    std::vector<std::uint8_t> data;

    // Serialize each store
    auto ai_data = m_ai_state.serialize();
    auto combat_data = m_combat_state.serialize();
    auto inventory_data = m_inventory_state.serialize();

    // Combine with headers
    std::uint32_t ai_size = static_cast<std::uint32_t>(ai_data.size());
    std::uint32_t combat_size = static_cast<std::uint32_t>(combat_data.size());
    std::uint32_t inventory_size = static_cast<std::uint32_t>(inventory_data.size());

    data.resize(sizeof(std::uint32_t) * 3 + ai_data.size() + combat_data.size() + inventory_data.size());

    std::size_t offset = 0;
    std::memcpy(data.data() + offset, &ai_size, sizeof(ai_size));
    offset += sizeof(ai_size);
    std::memcpy(data.data() + offset, &combat_size, sizeof(combat_size));
    offset += sizeof(combat_size);
    std::memcpy(data.data() + offset, &inventory_size, sizeof(inventory_size));
    offset += sizeof(inventory_size);

    std::memcpy(data.data() + offset, ai_data.data(), ai_data.size());
    offset += ai_data.size();
    std::memcpy(data.data() + offset, combat_data.data(), combat_data.size());
    offset += combat_data.size();
    std::memcpy(data.data() + offset, inventory_data.data(), inventory_data.size());

    return data;
}

void GameStateCore::deserialize_state(const std::vector<std::uint8_t>& data) {
    if (data.size() < sizeof(std::uint32_t) * 3) {
        return;
    }

    std::size_t offset = 0;
    std::uint32_t ai_size, combat_size, inventory_size;

    std::memcpy(&ai_size, data.data() + offset, sizeof(ai_size));
    offset += sizeof(ai_size);
    std::memcpy(&combat_size, data.data() + offset, sizeof(combat_size));
    offset += sizeof(combat_size);
    std::memcpy(&inventory_size, data.data() + offset, sizeof(inventory_size));
    offset += sizeof(inventory_size);

    if (data.size() < offset + ai_size + combat_size + inventory_size) {
        return;
    }

    std::vector<std::uint8_t> ai_data(data.begin() + offset, data.begin() + offset + ai_size);
    offset += ai_size;
    std::vector<std::uint8_t> combat_data(data.begin() + offset, data.begin() + offset + combat_size);
    offset += combat_size;
    std::vector<std::uint8_t> inventory_data(data.begin() + offset, data.begin() + offset + inventory_size);

    m_ai_state = void_plugin_api::AIStateStore::deserialize(ai_data);
    m_combat_state = void_plugin_api::CombatStateStore::deserialize(combat_data);
    m_inventory_state = void_plugin_api::InventoryStateStore::deserialize(inventory_data);
}

// =============================================================================
// Statistics
// =============================================================================

GameStateCore::Stats GameStateCore::stats() const {
    Stats s;
    s.commands_executed = m_command_processor->commands_executed();
    s.commands_failed = m_command_processor->commands_failed();
    s.ai_entities = m_ai_state.active_count();
    s.combat_entities = m_combat_state.active_count();
    s.inventory_entities = m_inventory_state.entity_inventories.size();
    s.active_projectiles = m_combat_state.active_projectiles.size();
    s.world_items = m_inventory_state.world_items.size();
    s.active_plugins = static_cast<std::uint32_t>(m_plugin_registry.active_count());
    return s;
}

// =============================================================================
// Event Notifications
// =============================================================================

void GameStateCore::notify_damage(void_plugin_api::EntityId target, void_plugin_api::EntityId source,
                                   float damage, bool killed) {
    if (m_on_damage) {
        m_on_damage(target, source, damage, killed);
    }
}

void GameStateCore::notify_death(void_plugin_api::EntityId entity, void_plugin_api::EntityId killer) {
    if (m_on_death) {
        m_on_death(entity, killer);
    }
}

void GameStateCore::notify_item_acquired(void_plugin_api::EntityId entity,
                                          void_plugin_api::ItemInstanceId item) {
    if (m_on_item_acquired) {
        m_on_item_acquired(entity, item);
    }
}

void GameStateCore::notify_item_lost(void_plugin_api::EntityId entity,
                                      void_plugin_api::ItemInstanceId item) {
    if (m_on_item_lost) {
        m_on_item_lost(entity, item);
    }
}

// =============================================================================
// IPluginLoader Interface Implementation (for PluginWatcher)
// =============================================================================

bool GameStateCore::watcher_load_plugin(const std::filesystem::path& path) {
    // Extract plugin name from path
    std::string name = path.stem().string();

    // Remove platform prefixes/suffixes
    if (name.starts_with("lib")) {
        name = name.substr(3);
    }
    for (const auto& suffix : {"_d", "_debug", "_release"}) {
        if (name.ends_with(suffix)) {
            name = name.substr(0, name.length() - std::strlen(suffix));
            break;
        }
    }

    // Check if already loaded
    if (m_loaded_plugins.find(name) != m_loaded_plugins.end()) {
        return true;  // Already loaded
    }

    // Create loaded plugin container
    auto loaded = std::make_unique<void_plugin_api::LoadedPlugin>();
    loaded->name = name;
    loaded->library = std::make_unique<void_plugin_api::DynamicLibrary>();

    // Load the dynamic library
    if (!loaded->library->load(path)) {
        // Failed to load library
        return false;
    }

    // Get factory functions
    auto create_func = loaded->library->get_function<void_plugin_api::CreatePluginFunc>("create_plugin");
    if (!create_func) {
        // No create_plugin function found
        return false;
    }

    loaded->destroy_func = loaded->library->get_function<void_plugin_api::DestroyPluginFunc>("destroy_plugin");

    // Create the plugin instance
    loaded->plugin = create_func();
    if (!loaded->plugin) {
        return false;
    }

    // Store mapping
    m_path_to_plugin[path] = name;
    m_plugin_to_path[name] = path;

    // Initialize the plugin directly (not through registry - we manage lifecycle ourselves)
    loaded->plugin->on_plugin_load(m_plugin_api.get());

    // Store the loaded plugin
    m_loaded_plugins[name] = std::move(loaded);

    return true;
}

bool GameStateCore::watcher_unload_plugin(const std::string& name) {
    auto it = m_loaded_plugins.find(name);
    if (it == m_loaded_plugins.end()) {
        return false;  // Plugin not found
    }

    // Snapshot custom state before unload
    auto state_snapshot = m_state_registry.snapshot_plugin(name);

    // Call plugin's unload callback
    if (it->second->plugin) {
        it->second->plugin->on_plugin_unload();
    }

    // Clear mappings
    auto path_it = m_plugin_to_path.find(name);
    if (path_it != m_plugin_to_path.end()) {
        m_path_to_plugin.erase(path_it->second);
        m_plugin_to_path.erase(path_it);
    }

    // Clear custom state
    m_state_registry.unregister_plugin(name);

    // Remove and destroy the plugin (LoadedPlugin destructor handles cleanup)
    m_loaded_plugins.erase(it);

    return true;
}

bool GameStateCore::watcher_hot_reload_plugin(const std::string& name, const std::filesystem::path& new_path) {
    auto it = m_loaded_plugins.find(name);
    if (it == m_loaded_plugins.end()) {
        return false;  // Plugin not found
    }

    // Snapshot custom plugin state before reload
    auto custom_state = m_state_registry.snapshot_plugin(name);

    // Snapshot plugin's runtime state
    std::vector<std::uint8_t> runtime_state;
    if (it->second->plugin) {
        runtime_state = it->second->plugin->serialize_runtime_state();
        // Call unload
        it->second->plugin->on_plugin_unload();
    }

    // The core state stores (AI, Combat, Inventory) are already preserved
    // because GameStateCore owns them, not the plugin

    // Get the path to reload from
    std::filesystem::path reload_path = new_path.empty() ? m_plugin_to_path[name] : new_path;

    // Destroy old plugin instance
    if (it->second->plugin && it->second->destroy_func) {
        it->second->destroy_func(it->second->plugin);
        it->second->plugin = nullptr;
    }

    // Unload old library
    it->second->library->unload();

    // Load new library
    if (!it->second->library->load(reload_path)) {
        // Failed to reload - plugin is now unloaded
        m_loaded_plugins.erase(it);
        return false;
    }

    // Get factory functions
    auto create_func = it->second->library->get_function<void_plugin_api::CreatePluginFunc>("create_plugin");
    if (!create_func) {
        m_loaded_plugins.erase(it);
        return false;
    }

    it->second->destroy_func = it->second->library->get_function<void_plugin_api::DestroyPluginFunc>("destroy_plugin");

    // Create new plugin instance
    it->second->plugin = create_func();
    if (!it->second->plugin) {
        m_loaded_plugins.erase(it);
        return false;
    }

    // Restore runtime state
    if (!runtime_state.empty()) {
        it->second->plugin->deserialize_runtime_state(runtime_state);
    }

    // Update path mapping if changed
    if (!new_path.empty()) {
        auto old_path_it = m_plugin_to_path.find(name);
        if (old_path_it != m_plugin_to_path.end()) {
            m_path_to_plugin.erase(old_path_it->second);
        }
        m_path_to_plugin[new_path] = name;
        m_plugin_to_path[name] = new_path;
    }

    // Initialize new plugin
    it->second->plugin->on_plugin_load(m_plugin_api.get());

    // Restore custom plugin state
    m_state_registry.restore_plugin(name, custom_state);

    return true;
}

bool GameStateCore::watcher_is_plugin_loaded(const std::string& name) const {
    return m_loaded_plugins.find(name) != m_loaded_plugins.end();
}

std::vector<std::string> GameStateCore::watcher_loaded_plugins() const {
    std::vector<std::string> result;
    result.reserve(m_loaded_plugins.size());
    for (const auto& [name, loaded] : m_loaded_plugins) {
        result.push_back(name);
    }
    return result;
}

// =============================================================================
// Plugin Watcher
// =============================================================================

void_plugin_api::PluginWatcher* GameStateCore::watcher() {
    if (!m_watcher && m_core_config.enable_hot_reload) {
        void_plugin_api::IPluginLoader& loader = *this;
        m_watcher.reset(new void_plugin_api::PluginWatcher(loader, m_watcher_config));
    }
    return m_watcher.get();
}

void GameStateCore::start_watching(const std::vector<std::filesystem::path>& paths) {
    if (!m_core_config.enable_hot_reload) {
        return;
    }

    // Ensure watcher exists
    if (!m_watcher) {
        m_watcher_config.watch_paths = paths;
        if (m_watcher_config.watch_paths.empty()) {
            m_watcher_config.watch_paths.push_back(m_core_config.plugin_directory);
        }
        void_plugin_api::IPluginLoader& loader = *this;
        m_watcher.reset(new void_plugin_api::PluginWatcher(loader, m_watcher_config));
    } else {
        // Add additional paths
        for (const auto& path : paths) {
            m_watcher->add_watch_path(path);
        }
    }

    m_watcher->start();
}

void GameStateCore::stop_watching() {
    if (m_watcher) {
        m_watcher->stop();
    }
}

bool GameStateCore::is_watching() const {
    return m_watcher && m_watcher->is_running();
}

void GameStateCore::configure_watcher(const void_plugin_api::PluginWatcherConfig& config) {
    m_watcher_config = config;
    if (m_watcher) {
        m_watcher->set_config(config);
    }
}

} // namespace void_gamestate
