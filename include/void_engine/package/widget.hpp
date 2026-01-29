#pragma once

/// @file widget.hpp
/// @brief Widget interface for UI/tooling/overlays
///
/// Widgets are UI elements that can be loaded from external packages.
/// They bind to ECS queries and resources to display/manipulate game state.
///
/// Key features:
/// - Loadable from external sources (mods, debug tools)
/// - ECS bindings specified by component NAME, resolved at runtime
/// - Build-type filtering (debug/development/release)
/// - Hot-reload support

#include <void_engine/core/error.hpp>
#include <void_engine/ecs/fwd.hpp>
#include <void_engine/ecs/entity.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <optional>

namespace void_ecs { class World; class QueryState; }

namespace void_package {

// Forward declarations
class WidgetManager;

// =============================================================================
// WidgetHandle
// =============================================================================

/// Opaque handle to a widget instance
struct WidgetHandle {
    std::uint32_t index = 0;
    std::uint32_t generation = 0;

    /// Check if handle is valid (non-null)
    [[nodiscard]] bool is_valid() const noexcept {
        return generation != 0;
    }

    /// Null handle
    [[nodiscard]] static WidgetHandle null() noexcept {
        return WidgetHandle{0, 0};
    }

    bool operator==(const WidgetHandle& other) const noexcept = default;
    bool operator!=(const WidgetHandle& other) const noexcept = default;
};

// =============================================================================
// WidgetContext
// =============================================================================

/// Context provided to widgets during lifecycle callbacks
///
/// Provides access to:
/// - ECS World for queries and resources
/// - Bound queries (pre-built from component names)
/// - Widget configuration from manifest
class WidgetContext {
public:
    WidgetContext() = default;
    explicit WidgetContext(void_ecs::World* world)
        : m_world(world)
    {}

    // =========================================================================
    // ECS Access
    // =========================================================================

    /// Get the ECS world
    [[nodiscard]] void_ecs::World* world() const noexcept { return m_world; }

    /// Get a bound query by name
    [[nodiscard]] void_ecs::QueryState* get_bound_query(const std::string& name) const;

    /// Get a resource by name (generic JSON accessor)
    [[nodiscard]] const nlohmann::json* get_resource(const std::string& name) const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Get widget configuration
    [[nodiscard]] const nlohmann::json& config() const noexcept { return m_config; }

    /// Set widget configuration
    void set_config(nlohmann::json config) { m_config = std::move(config); }

    // =========================================================================
    // Internal API (used by WidgetManager)
    // =========================================================================

    /// Add a bound query
    void add_bound_query(const std::string& name, void_ecs::QueryState* query);

    /// Add a resource binding
    void add_resource_binding(const std::string& name, const nlohmann::json* resource);

    void set_world(void_ecs::World* world) { m_world = world; }

private:
    void_ecs::World* m_world = nullptr;
    nlohmann::json m_config;
    std::map<std::string, void_ecs::QueryState*> m_bound_queries;
    std::map<std::string, const nlohmann::json*> m_bound_resources;
};

// =============================================================================
// Widget
// =============================================================================

/// Abstract base class for all widgets
///
/// Widgets can be:
/// - Built-in (debug_hud, console, inspector)
/// - Loaded from plugins (custom gameplay HUDs)
/// - Provided by mods (external sources)
///
/// Lifecycle:
/// 1. Created by WidgetManager based on manifest
/// 2. init() called with context
/// 3. update(dt) called each frame (if enabled)
/// 4. render() called each frame (if visible)
/// 5. shutdown() called before destruction
///
/// Thread-safety: Widgets are NOT thread-safe. All operations must occur
/// on the main thread.
class Widget {
public:
    virtual ~Widget() = default;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// Initialize the widget
    ///
    /// Called once after creation and context setup.
    /// @param ctx Widget context with ECS access and configuration
    /// @return Ok on success, Error on failure
    [[nodiscard]] virtual void_core::Result<void> init(WidgetContext& ctx) = 0;

    /// Update the widget
    ///
    /// Called each frame if the widget is active.
    /// @param ctx Widget context
    /// @param dt Delta time in seconds
    virtual void update(WidgetContext& ctx, float dt) = 0;

    /// Render the widget
    ///
    /// Called each frame if the widget is visible.
    /// @param ctx Widget context
    virtual void render(WidgetContext& ctx) = 0;

    /// Shutdown the widget
    ///
    /// Called before destruction. Clean up resources here.
    /// @param ctx Widget context
    virtual void shutdown(WidgetContext& ctx) = 0;

    // =========================================================================
    // State
    // =========================================================================

    /// Get widget ID
    [[nodiscard]] virtual const std::string& id() const = 0;

    /// Get widget type
    [[nodiscard]] virtual const std::string& type() const = 0;

    /// Check if widget is enabled
    [[nodiscard]] bool is_enabled() const noexcept { return m_enabled; }

    /// Enable/disable the widget
    void set_enabled(bool enabled) noexcept { m_enabled = enabled; }

    /// Check if widget is visible
    [[nodiscard]] bool is_visible() const noexcept { return m_visible; }

    /// Show/hide the widget
    void set_visible(bool visible) noexcept { m_visible = visible; }

    /// Toggle visibility
    void toggle_visible() noexcept { m_visible = !m_visible; }

protected:
    Widget() = default;

    // Non-copyable
    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;

private:
    bool m_enabled = true;
    bool m_visible = true;
};

// =============================================================================
// WidgetFactory
// =============================================================================

/// Function type for creating widget instances
///
/// Factories are registered by type name and invoked when a widget
/// of that type is declared in a manifest.
using WidgetFactory = std::function<std::unique_ptr<Widget>(const nlohmann::json& config)>;

// =============================================================================
// Built-in Widget Types
// =============================================================================

/// Debug HUD widget displaying performance metrics
///
/// Shows: FPS, frame time, entity count, memory usage
class DebugHudWidget : public Widget {
public:
    DebugHudWidget();
    explicit DebugHudWidget(const nlohmann::json& config);

    [[nodiscard]] void_core::Result<void> init(WidgetContext& ctx) override;
    void update(WidgetContext& ctx, float dt) override;
    void render(WidgetContext& ctx) override;
    void shutdown(WidgetContext& ctx) override;

    [[nodiscard]] const std::string& id() const override { return m_id; }
    [[nodiscard]] const std::string& type() const override { return s_type; }

    // Configuration
    void set_show_fps(bool show) noexcept { m_show_fps = show; }
    void set_show_frame_time(bool show) noexcept { m_show_frame_time = show; }
    void set_show_entity_count(bool show) noexcept { m_show_entity_count = show; }
    void set_show_memory(bool show) noexcept { m_show_memory = show; }

private:
    static inline const std::string s_type = "debug_hud";
    std::string m_id = "debug_hud";

    // Configuration
    bool m_show_fps = true;
    bool m_show_frame_time = true;
    bool m_show_entity_count = true;
    bool m_show_memory = false;

    // Metrics
    float m_fps = 0.0f;
    float m_frame_time_ms = 0.0f;
    std::size_t m_entity_count = 0;
    std::size_t m_memory_used_mb = 0;

    // Averaging
    float m_fps_accumulator = 0.0f;
    int m_fps_sample_count = 0;
    static constexpr int kFpsSampleWindow = 60;
};

/// Console widget for command input and log output
class ConsoleWidget : public Widget {
public:
    ConsoleWidget();
    explicit ConsoleWidget(const nlohmann::json& config);

    [[nodiscard]] void_core::Result<void> init(WidgetContext& ctx) override;
    void update(WidgetContext& ctx, float dt) override;
    void render(WidgetContext& ctx) override;
    void shutdown(WidgetContext& ctx) override;

    [[nodiscard]] const std::string& id() const override { return m_id; }
    [[nodiscard]] const std::string& type() const override { return s_type; }

    /// Add a log message
    void log(const std::string& message);

    /// Execute a command
    void_core::Result<void> execute_command(const std::string& command);

    /// Register a command handler
    void register_command(const std::string& name,
                          std::function<void_core::Result<void>(const std::vector<std::string>&)> handler);

    /// Get history size
    [[nodiscard]] std::size_t history_size() const { return m_history.size(); }

private:
    static inline const std::string s_type = "console";
    std::string m_id = "console";

    // Configuration
    std::size_t m_max_history = 100;
    std::string m_log_filter;

    // State
    std::vector<std::string> m_log_messages;
    std::vector<std::string> m_history;
    std::string m_input_buffer;
    std::size_t m_history_index = 0;
    bool m_scroll_to_bottom = false;

    // Commands
    std::map<std::string, std::function<void_core::Result<void>(const std::vector<std::string>&)>> m_commands;

    void setup_default_commands();
};

/// Entity inspector widget for viewing/editing entity components
class InspectorWidget : public Widget {
public:
    InspectorWidget();
    explicit InspectorWidget(const nlohmann::json& config);

    [[nodiscard]] void_core::Result<void> init(WidgetContext& ctx) override;
    void update(WidgetContext& ctx, float dt) override;
    void render(WidgetContext& ctx) override;
    void shutdown(WidgetContext& ctx) override;

    [[nodiscard]] const std::string& id() const override { return m_id; }
    [[nodiscard]] const std::string& type() const override { return s_type; }

    /// Select an entity for inspection
    void select_entity(void_ecs::Entity entity);

    /// Clear selection
    void clear_selection();

    /// Get selected entity
    [[nodiscard]] std::optional<void_ecs::Entity> selected_entity() const;

private:
    static inline const std::string s_type = "inspector";
    std::string m_id = "inspector";

    // Configuration
    bool m_allow_edit = true;
    bool m_show_hidden = false;

    // State
    std::optional<void_ecs::Entity> m_selected_entity;
};

// =============================================================================
// Widget Type Registry
// =============================================================================

/// Registry for widget factories
///
/// Allows runtime registration of new widget types from plugins.
/// Built-in types are registered automatically.
class WidgetTypeRegistry {
public:
    WidgetTypeRegistry();

    /// Register a widget factory
    ///
    /// @param type_name Widget type name (e.g., "debug_hud", "custom_hud")
    /// @param factory Factory function to create widget instances
    void register_type(const std::string& type_name, WidgetFactory factory);

    /// Check if a type is registered
    [[nodiscard]] bool has_type(const std::string& type_name) const;

    /// Create a widget of given type
    ///
    /// @param type_name Widget type name
    /// @param config Configuration JSON
    /// @return Created widget or nullptr if type not found
    [[nodiscard]] std::unique_ptr<Widget> create(
        const std::string& type_name,
        const nlohmann::json& config) const;

    /// Get all registered type names
    [[nodiscard]] std::vector<std::string> registered_types() const;

    /// Register built-in types
    void register_builtins();

private:
    std::map<std::string, WidgetFactory> m_factories;
};

} // namespace void_package
