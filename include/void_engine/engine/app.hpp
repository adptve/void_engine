/// @file app.hpp
/// @brief Application interface for void_engine
///
/// The IApp interface defines the contract for game/application code.
/// Games implement this interface to integrate with the engine lifecycle.

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <void_engine/core/error.hpp>
#include <void_engine/core/hot_reload.hpp>

#include <functional>
#include <memory>
#include <string>

namespace void_engine {

// =============================================================================
// App Configuration
// =============================================================================

/// Application configuration
struct AppConfig {
    std::string name = "app";
    std::string version = "0.1.0";
    std::string organization;

    // Optional engine config overrides
    EngineFeature required_features = EngineFeature::Minimal;

    // Lifecycle options
    bool pause_on_focus_lost = false;
    bool allow_background_update = true;

    // Hot-reload
    bool supports_hot_reload = false;
    std::vector<std::string> hot_reload_paths;

    /// Create from name
    [[nodiscard]] static AppConfig from_name(const std::string& name);
};

// =============================================================================
// Application Interface
// =============================================================================

/// Application interface - implement this for your game
class IApp {
public:
    virtual ~IApp() = default;

    // =========================================================================
    // Information
    // =========================================================================

    /// Get app configuration
    [[nodiscard]] virtual const AppConfig& config() const = 0;

    /// Get app name
    [[nodiscard]] virtual std::string name() const { return config().name; }

    /// Get app version
    [[nodiscard]] virtual std::string version() const { return config().version; }

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// Called once when the application starts
    /// @param engine The engine instance
    /// @return Success or error
    [[nodiscard]] virtual void_core::Result<void> on_init(Engine& engine) = 0;

    /// Called when the application is ready to run
    /// This is after all subsystems are initialized
    [[nodiscard]] virtual void_core::Result<void> on_ready(Engine& engine) {
        (void)engine;
        return void_core::Ok();
    }

    /// Called every frame to update game state
    /// @param engine The engine instance
    /// @param dt Delta time in seconds
    virtual void on_update(Engine& engine, float dt) = 0;

    /// Called at fixed intervals for physics/simulation
    /// @param engine The engine instance
    /// @param dt Fixed timestep in seconds
    virtual void on_fixed_update(Engine& engine, float dt) {
        (void)engine;
        (void)dt;
    }

    /// Called after update to handle late-update tasks
    virtual void on_late_update(Engine& engine, float dt) {
        (void)engine;
        (void)dt;
    }

    /// Called every frame to render
    /// @param engine The engine instance
    virtual void on_render(Engine& engine) = 0;

    /// Called when the application is shutting down
    virtual void on_shutdown(Engine& engine) = 0;

    // =========================================================================
    // Events
    // =========================================================================

    /// Called when the window gains focus
    virtual void on_focus_gained(Engine& engine) {
        (void)engine;
    }

    /// Called when the window loses focus
    virtual void on_focus_lost(Engine& engine) {
        (void)engine;
    }

    /// Called when the window is resized
    virtual void on_resize(Engine& engine, std::uint32_t width, std::uint32_t height) {
        (void)engine;
        (void)width;
        (void)height;
    }

    /// Called when the application should quit
    /// @return true to allow quit, false to prevent
    [[nodiscard]] virtual bool on_quit_request(Engine& engine) {
        (void)engine;
        return true;
    }

    // =========================================================================
    // Hot-Reload
    // =========================================================================

    /// Check if app supports hot-reload
    [[nodiscard]] virtual bool supports_hot_reload() const {
        return config().supports_hot_reload;
    }

    /// Prepare for hot-reload (save state)
    [[nodiscard]] virtual void_core::Result<void_core::HotReloadSnapshot> prepare_reload(Engine& engine) {
        (void)engine;
        return void_core::HotReloadSnapshot::empty();
    }

    /// Complete hot-reload (restore state)
    [[nodiscard]] virtual void_core::Result<void> complete_reload(
        Engine& engine, void_core::HotReloadSnapshot snapshot)
    {
        (void)engine;
        (void)snapshot;
        return void_core::Ok();
    }
};

// =============================================================================
// Application Base Class
// =============================================================================

/// Convenient base class for applications
class AppBase : public IApp {
public:
    explicit AppBase(AppConfig config) : m_config(std::move(config)) {}
    ~AppBase() override = default;

    [[nodiscard]] const AppConfig& config() const override { return m_config; }

    // Default implementations that do nothing
    void on_fixed_update(Engine& engine, float dt) override {
        (void)engine;
        (void)dt;
    }

    void on_late_update(Engine& engine, float dt) override {
        (void)engine;
        (void)dt;
    }

protected:
    AppConfig m_config;
};

// =============================================================================
// Application Builder
// =============================================================================

/// Builder for application configuration
class AppBuilder {
public:
    AppBuilder() = default;

    /// Set app name
    AppBuilder& name(const std::string& n) {
        m_config.name = n;
        return *this;
    }

    /// Set app version
    AppBuilder& version(const std::string& v) {
        m_config.version = v;
        return *this;
    }

    /// Set organization
    AppBuilder& organization(const std::string& org) {
        m_config.organization = org;
        return *this;
    }

    /// Set required features
    AppBuilder& require_features(EngineFeature features) {
        m_config.required_features = features;
        return *this;
    }

    /// Enable hot-reload support
    AppBuilder& hot_reload(bool enable = true) {
        m_config.supports_hot_reload = enable;
        return *this;
    }

    /// Add hot-reload watch path
    AppBuilder& watch_path(const std::string& path) {
        m_config.hot_reload_paths.push_back(path);
        return *this;
    }

    /// Pause when focus lost
    AppBuilder& pause_on_focus_lost(bool pause = true) {
        m_config.pause_on_focus_lost = pause;
        return *this;
    }

    /// Build the config
    [[nodiscard]] AppConfig build() const {
        return m_config;
    }

private:
    AppConfig m_config;
};

// =============================================================================
// Simple App (Lambda-based)
// =============================================================================

/// Callbacks for SimpleApp
struct SimpleAppCallbacks {
    std::function<void_core::Result<void>(Engine&)> on_init;
    std::function<void(Engine&, float)> on_update;
    std::function<void(Engine&)> on_render;
    std::function<void(Engine&)> on_shutdown;

    // Optional
    std::function<void_core::Result<void>(Engine&)> on_ready;
    std::function<void(Engine&, float)> on_fixed_update;
    std::function<void(Engine&, float)> on_late_update;
    std::function<void(Engine&)> on_focus_gained;
    std::function<void(Engine&)> on_focus_lost;
    std::function<void(Engine&, std::uint32_t, std::uint32_t)> on_resize;
    std::function<bool(Engine&)> on_quit_request;
};

/// Simple application using callbacks
class SimpleApp : public AppBase {
public:
    SimpleApp(AppConfig config, SimpleAppCallbacks callbacks)
        : AppBase(std::move(config))
        , m_callbacks(std::move(callbacks)) {}

    [[nodiscard]] void_core::Result<void> on_init(Engine& engine) override {
        if (m_callbacks.on_init) {
            return m_callbacks.on_init(engine);
        }
        return void_core::Ok();
    }

    [[nodiscard]] void_core::Result<void> on_ready(Engine& engine) override {
        if (m_callbacks.on_ready) {
            return m_callbacks.on_ready(engine);
        }
        return void_core::Ok();
    }

    void on_update(Engine& engine, float dt) override {
        if (m_callbacks.on_update) {
            m_callbacks.on_update(engine, dt);
        }
    }

    void on_fixed_update(Engine& engine, float dt) override {
        if (m_callbacks.on_fixed_update) {
            m_callbacks.on_fixed_update(engine, dt);
        }
    }

    void on_late_update(Engine& engine, float dt) override {
        if (m_callbacks.on_late_update) {
            m_callbacks.on_late_update(engine, dt);
        }
    }

    void on_render(Engine& engine) override {
        if (m_callbacks.on_render) {
            m_callbacks.on_render(engine);
        }
    }

    void on_shutdown(Engine& engine) override {
        if (m_callbacks.on_shutdown) {
            m_callbacks.on_shutdown(engine);
        }
    }

    void on_focus_gained(Engine& engine) override {
        if (m_callbacks.on_focus_gained) {
            m_callbacks.on_focus_gained(engine);
        }
    }

    void on_focus_lost(Engine& engine) override {
        if (m_callbacks.on_focus_lost) {
            m_callbacks.on_focus_lost(engine);
        }
    }

    void on_resize(Engine& engine, std::uint32_t width, std::uint32_t height) override {
        if (m_callbacks.on_resize) {
            m_callbacks.on_resize(engine, width, height);
        }
    }

    [[nodiscard]] bool on_quit_request(Engine& engine) override {
        if (m_callbacks.on_quit_request) {
            return m_callbacks.on_quit_request(engine);
        }
        return true;
    }

private:
    SimpleAppCallbacks m_callbacks;
};

// =============================================================================
// Factory Functions
// =============================================================================

/// Create a simple app with callbacks
[[nodiscard]] inline std::unique_ptr<SimpleApp> make_simple_app(
    const std::string& name,
    SimpleAppCallbacks callbacks)
{
    auto config = AppConfig::from_name(name);
    return std::make_unique<SimpleApp>(std::move(config), std::move(callbacks));
}

/// Create app with init/update/render/shutdown callbacks
[[nodiscard]] inline std::unique_ptr<SimpleApp> make_app(
    const std::string& name,
    std::function<void_core::Result<void>(Engine&)> init,
    std::function<void(Engine&, float)> update,
    std::function<void(Engine&)> render,
    std::function<void(Engine&)> shutdown)
{
    SimpleAppCallbacks callbacks;
    callbacks.on_init = std::move(init);
    callbacks.on_update = std::move(update);
    callbacks.on_render = std::move(render);
    callbacks.on_shutdown = std::move(shutdown);
    return make_simple_app(name, std::move(callbacks));
}

} // namespace void_engine
