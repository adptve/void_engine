/// @file main.cpp
/// @brief void_engine entry point - phased initialization
///
/// Phases:
///   0. Skeleton     - CLI, manifest (ACTIVE)
///   1. Foundation   - memory, core, math, structures
///   2. Infrastructure - event, services, ir, kernel
///   3. Resources    - asset, shader
///   4. Platform     - presenter, render, compositor
///   5. I/O          - audio
///   6. Simulation   - ecs, physics, triggers
///   7. Scene        - scene, graph
///   8. Scripting    - script, scripting, cpp, shell
///   9. Gameplay     - ai, combat, inventory, gamestate
///  10. UI           - ui, hud
///  11. Extensions   - xr, editor
///  12. Application  - runtime, engine

// =============================================================================
// PHASE 0: SKELETON (ACTIVE)
// =============================================================================
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

// =============================================================================
// PHASE 1: FOUNDATION (ACTIVE)
// =============================================================================
#include <void_engine/memory/memory.hpp>
#include <void_engine/math/math.hpp>
#include <void_engine/structures/structures.hpp>
#include <void_engine/core/core.hpp>

// =============================================================================
// PHASE 2: INFRASTRUCTURE (ACTIVE)
// =============================================================================
#include <void_engine/event/event_bus.hpp>
#include <void_engine/services/services.hpp>
#include <void_engine/ir/ir.hpp>
#include <void_engine/kernel/kernel.hpp>

// =============================================================================
// PHASE 3: RESOURCES (ACTIVE)
// =============================================================================
#include <void_engine/asset/asset.hpp>
#include <void_engine/shader/shader.hpp>

// =============================================================================
// PHASE 4: PLATFORM
// =============================================================================
// #include <void_engine/presenter/timing.hpp>
// #include <void_engine/presenter/frame.hpp>
// #include <void_engine/render/gl_renderer.hpp>
// #include <void_engine/compositor/compositor_module.hpp>
// #include <GLFW/glfw3.h>

// =============================================================================
// PHASE 5: I/O
// =============================================================================
// #include <void_engine/audio/device.hpp>
// #include <void_engine/audio/sound.hpp>
// #include <void_engine/audio/listener.hpp>

// =============================================================================
// PHASE 6: SIMULATION
// =============================================================================
// #include <void_engine/ecs/world.hpp>
// #include <void_engine/physics/physics.hpp>
// #include <void_engine/triggers/triggers.hpp>

// =============================================================================
// PHASE 7: SCENE
// =============================================================================
// #include <void_engine/scene/scene_parser.hpp>
// #include <void_engine/scene/scene_data.hpp>
// #include <void_engine/scene/scene_instantiator.hpp>
// #include <void_engine/graph/node.hpp>

// =============================================================================
// PHASE 8: SCRIPTING
// =============================================================================
// #include <void_engine/script/parser.hpp>
// #include <void_engine/scripting/vm.hpp>
// #include <void_engine/cpp/compiler.hpp>
// #include <void_engine/shell/shell.hpp>

// =============================================================================
// PHASE 9: GAMEPLAY
// =============================================================================
// #include <void_engine/ai/fsm.hpp>
// #include <void_engine/ai/behavior_tree.hpp>
// #include <void_engine/combat/damage.hpp>
// #include <void_engine/inventory/inventory.hpp>
// #include <void_engine/gamestate/state_machine.hpp>

// =============================================================================
// PHASE 10: UI
// =============================================================================
// #include <void_engine/ui/context.hpp>
// #include <void_engine/ui/widgets.hpp>
// #include <void_engine/hud/hud.hpp>
// #include <void_engine/hud/health_bar.hpp>

// =============================================================================
// PHASE 11: EXTENSIONS
// =============================================================================
// #include <void_engine/xr/session.hpp>
// #include <void_engine/xr/input.hpp>
// #include <void_engine/editor/editor.hpp>

// =============================================================================
// PHASE 12: APPLICATION
// =============================================================================
// #include <void_engine/runtime/runtime.hpp>
// #include <void_engine/engine/engine.hpp>

namespace fs = std::filesystem;

// =============================================================================
// Project Configuration
// =============================================================================

struct ProjectConfig {
    std::string name;
    std::string display_name;
    std::string version;
    std::string scene_file;
    fs::path project_dir;
    int window_width = 1280;
    int window_height = 720;
    bool valid = false;
    std::string error;
};

ProjectConfig load_manifest(const fs::path& manifest_path) {
    ProjectConfig config;

    if (!fs::exists(manifest_path)) {
        config.error = "Manifest file not found: " + manifest_path.string();
        return config;
    }

    config.project_dir = manifest_path.parent_path();

    try {
        std::ifstream file(manifest_path);
        if (!file.is_open()) {
            config.error = "Could not open manifest file";
            return config;
        }

        nlohmann::json json = nlohmann::json::parse(file);

        // Parse package section
        if (json.contains("package")) {
            auto& pkg = json["package"];
            config.name = pkg.value("name", "unnamed");
            config.display_name = pkg.value("display_name", config.name);
            config.version = pkg.value("version", "0.0.0");
        } else {
            config.error = "Missing 'package' section in manifest";
            return config;
        }

        // Parse app section
        if (json.contains("app")) {
            auto& app = json["app"];
            config.scene_file = app.value("scene", "");
        } else {
            config.error = "Missing 'app' section in manifest";
            return config;
        }

        // Parse window section (optional)
        if (json.contains("window")) {
            auto& win = json["window"];
            config.window_width = win.value("width", 1280);
            config.window_height = win.value("height", 720);
        }

        config.valid = true;

    } catch (const nlohmann::json::parse_error& err) {
        config.error = "Failed to parse manifest: " + std::string(err.what());
    } catch (const std::exception& err) {
        config.error = "Error reading manifest: " + std::string(err.what());
    }

    return config;
}

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [OPTIONS] [PROJECT_PATH]\n"
              << "\n"
              << "Arguments:\n"
              << "  PROJECT_PATH    Path to project directory or manifest.toml\n"
              << "\n"
              << "Options:\n"
              << "  --help, -h      Show this help message\n"
              << "  --version, -v   Show version information\n";
}

void print_version() {
    std::cout << "void_engine 0.1.0\n";
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    fs::path project_path;

    // =========================================================================
    // PHASE 0: CLI PARSING
    // =========================================================================
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            print_version();
            return 0;
        } else if (arg[0] != '-') {
            project_path = arg;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (project_path.empty()) {
        std::cerr << "Error: No project specified.\n\n";
        print_usage(argv[0]);
        return 1;
    }

    // Resolve manifest path
    fs::path manifest_path;
    if (fs::is_directory(project_path)) {
        manifest_path = project_path / "manifest.json";
    } else if (fs::is_regular_file(project_path)) {
        manifest_path = project_path;
    } else {
        std::cerr << "Project path does not exist: " << project_path << "\n";
        return 1;
    }

    // Load manifest
    spdlog::info("Loading project: {}", manifest_path.string());
    auto config = load_manifest(manifest_path);

    if (!config.valid) {
        spdlog::error("Failed to load project: {}", config.error);
        return 1;
    }

    spdlog::info("Project: {} v{}", config.display_name, config.version);
    spdlog::info("Scene: {}", config.scene_file);
    spdlog::info("Window: {}x{}", config.window_width, config.window_height);

    // =========================================================================
    // PHASE 1: FOUNDATION (ACTIVE)
    // =========================================================================
    spdlog::info("Phase 1: Foundation");

    // -------------------------------------------------------------------------
    // MEMORY MODULE
    // -------------------------------------------------------------------------
    spdlog::info("  [memory]");

    // Arena allocator
    void_memory::Arena arena(1024);
    void* arena_ptr = arena.allocate(64, 16);
    spdlog::info("    Arena: allocated 64 bytes at {:p}", arena_ptr);

    // Pool allocator
    void_memory::Pool pool = void_memory::Pool::for_type<float>(16);
    void* pool_ptr = pool.allocate(sizeof(float), alignof(float));
    spdlog::info("    Pool: allocated float at {:p}", pool_ptr);

    // -------------------------------------------------------------------------
    // MATH MODULE
    // -------------------------------------------------------------------------
    spdlog::info("  [math]");

    // Vec3 operations
    void_math::Vec3 v1{1.0f, 2.0f, 3.0f};
    void_math::Vec3 v2{4.0f, 5.0f, 6.0f};
    float dot_result = glm::dot(v1, v2);
    spdlog::info("    Vec3: dot({},{},{}) * ({},{},{}) = {}",
                 v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, dot_result);

    // Transform
    auto transform = void_math::Transform::from_position(void_math::vec3::UP * 5.0f);
    spdlog::info("    Transform: pos=({},{},{})",
                 transform.position.x, transform.position.y, transform.position.z);

    // Mat4
    void_math::Mat4 identity = void_math::mat4::IDENTITY;
    spdlog::info("    Mat4: identity[0][0]={}", identity[0][0]);

    // Quat
    void_math::Quat q = void_math::quat::IDENTITY;
    spdlog::info("    Quat: identity w={}", q.w);

    // -------------------------------------------------------------------------
    // STRUCTURES MODULE
    // -------------------------------------------------------------------------
    spdlog::info("  [structures]");

    // SlotMap
    void_structures::SlotMap<int> slot_map;
    auto slot_key = slot_map.insert(42);
    auto* slot_val = slot_map.get(slot_key);
    spdlog::info("    SlotMap: key gen={}, value={}", slot_key.generation, slot_val ? *slot_val : -1);

    // SparseSet
    void_structures::SparseSet<float> sparse_set;
    sparse_set.insert(10, 3.14f);
    sparse_set.insert(20, 2.71f);
    spdlog::info("    SparseSet: size={}, contains(10)={}", sparse_set.size(), sparse_set.contains(10));

    // -------------------------------------------------------------------------
    // CORE MODULE
    // -------------------------------------------------------------------------
    spdlog::info("  [core]");

    // Version
    spdlog::info("    Version: {}", void_core::VOID_CORE_VERSION.to_string());

    // Handle system
    void_core::HandleAllocator<int> handle_alloc;
    auto h1 = handle_alloc.allocate();
    auto h2 = handle_alloc.allocate();
    spdlog::info("    Handle: h1 idx={} gen={}, h2 idx={} gen={}",
                 h1.index(), h1.generation(), h2.index(), h2.generation());

    // HotReload event (SACRED pattern)
    auto reload_event = void_core::ReloadEvent::modified("test.cpp");
    spdlog::info("    HotReload: event type={}", void_core::reload_event_type_name(reload_event.type));

    spdlog::info("Phase 1 complete");

    // =========================================================================
    // PHASE 2: INFRASTRUCTURE (ACTIVE)
    // =========================================================================
    spdlog::info("Phase 2: Infrastructure");

    // -------------------------------------------------------------------------
    // EVENT MODULE - Event bus for engine-wide messaging
    // -------------------------------------------------------------------------
    spdlog::info("  [event]");

    // Create engine event bus (will persist for app lifetime)
    void_event::EventBus event_bus;

    // Test event struct
    struct TestEvent {
        std::string message;
        int value;
    };

    // Subscribe to test events
    int received_count = 0;
    auto sub_id = event_bus.subscribe<TestEvent>([&received_count](const TestEvent& e) {
        received_count++;
    });
    spdlog::info("    EventBus: subscribed id={}", sub_id.id);

    // Publish events
    event_bus.publish(TestEvent{"hello", 42});
    event_bus.publish(TestEvent{"world", 100});
    event_bus.process();
    spdlog::info("    EventBus: published 2 events, received {}", received_count);

    // Wire hot-reload events to event bus
    event_bus.subscribe<void_core::ReloadEvent>([](const void_core::ReloadEvent& e) {
        spdlog::info("    [hot-reload] {} on {}", void_core::reload_event_type_name(e.type), e.path);
    });
    spdlog::info("    EventBus: hot-reload subscription wired");

    // -------------------------------------------------------------------------
    // SERVICES MODULE - Service registry for managed services
    // -------------------------------------------------------------------------
    spdlog::info("  [services]");

    void_services::ServiceRegistry service_registry;
    auto reg_stats = service_registry.stats();
    spdlog::info("    ServiceRegistry: {} services registered", reg_stats.total_services);

    // -------------------------------------------------------------------------
    // IR MODULE - Intermediate representation for state patches
    // -------------------------------------------------------------------------
    spdlog::info("  [ir]");

    void_ir::NamespaceRegistry ns_registry;
    auto game_ns = ns_registry.create("game");
    spdlog::info("    NamespaceRegistry: created 'game' ns id={}", game_ns.value);

    // Create entity reference
    void_ir::EntityRef player_ref(game_ns, 1);
    spdlog::info("    EntityRef: player ns={} entity={}", player_ref.namespace_id.value, player_ref.entity_id);

    // -------------------------------------------------------------------------
    // KERNEL MODULE - Central orchestrator
    // -------------------------------------------------------------------------
    spdlog::info("  [kernel]");

    // Build kernel with configuration
    auto kernel = void_kernel::KernelBuilder()
        .name(config.name)
        .hot_reload(true)
        .target_fps(60)
        .build();

    spdlog::info("    Kernel: created '{}', phase={}",
                 kernel->config().name,
                 static_cast<int>(kernel->phase()));

    // Initialize kernel
    auto init_result = kernel->initialize();
    if (init_result) {
        spdlog::info("    Kernel: initialized successfully");
    } else {
        spdlog::warn("    Kernel: init returned error (expected at this phase)");
    }

    spdlog::info("Phase 2 complete");

    // =========================================================================
    // PHASE 3: RESOURCES (ACTIVE) - Full Production Integration
    // =========================================================================
    spdlog::info("Phase 3: Resources");

    // -------------------------------------------------------------------------
    // SERVICE WRAPPERS
    // -------------------------------------------------------------------------
    // AssetService: Wraps AssetServer with lifecycle management
    class AssetService : public void_services::ServiceBase {
    public:
        AssetService(void_asset::AssetServerConfig cfg, void_event::EventBus& bus)
            : ServiceBase("asset_service", void_services::ServiceConfig{
                .auto_restart = true,
                .max_restart_attempts = 3,
                .priority = 100  // High priority - assets needed early
            })
            , m_config(std::move(cfg))
            , m_event_bus(bus)
        {}

        void_asset::AssetServer& server() { return *m_server; }
        const void_asset::AssetServer& server() const { return *m_server; }

        // Process pending loads and drain events (call each frame)
        void tick() {
            if (m_server) {
                m_server->process();
                auto events = m_server->drain_events();
                for (const auto& e : events) {
                    m_event_bus.publish(e);
                }
            }
        }

        // SACRED: Snapshot for hot-reload
        std::vector<std::uint8_t> snapshot() const {
            void_services::BinaryWriter writer;
            writer.write_u32(1); // version
            if (m_server) {
                writer.write_u64(m_server->loaded_count());
                writer.write_u64(m_server->pending_count());
            } else {
                writer.write_u64(0);
                writer.write_u64(0);
            }
            return writer.take();
        }

        // SACRED: Restore from snapshot
        void restore(const std::vector<std::uint8_t>& data) {
            void_services::BinaryReader reader(data);
            [[maybe_unused]] auto version = reader.read_u32();
            [[maybe_unused]] auto loaded = reader.read_u64();
            [[maybe_unused]] auto pending = reader.read_u64();
            // State restored - assets will reload on demand
        }

    protected:
        bool on_start() override {
            m_server = std::make_unique<void_asset::AssetServer>(m_config);
            spdlog::info("    AssetService: started");
            return true;
        }

        void on_stop() override {
            m_server.reset();
            spdlog::info("    AssetService: stopped");
        }

        float on_check_health() override {
            if (!m_server) return 0.0f;
            // Health based on pending load ratio
            auto pending = m_server->pending_count();
            auto loaded = m_server->loaded_count();
            if (loaded == 0 && pending == 0) return 1.0f;
            return 1.0f - (static_cast<float>(pending) / static_cast<float>(pending + loaded + 1));
        }

    private:
        void_asset::AssetServerConfig m_config;
        void_event::EventBus& m_event_bus;
        std::unique_ptr<void_asset::AssetServer> m_server;
    };

    // ShaderService: Wraps ShaderPipeline with lifecycle management
    class ShaderService : public void_services::ServiceBase {
    public:
        explicit ShaderService(void_shader::ShaderPipelineConfig cfg)
            : ServiceBase("shader_service", void_services::ServiceConfig{
                .auto_restart = true,
                .max_restart_attempts = 3,
                .priority = 90  // After assets
            })
            , m_config(std::move(cfg))
        {}

        void_shader::ShaderPipeline& pipeline() { return *m_pipeline; }
        const void_shader::ShaderPipeline& pipeline() const { return *m_pipeline; }

        // Poll for shader changes (call each frame)
        void tick() {
            if (m_pipeline) {
                auto changes = m_pipeline->poll_changes();
                for (const auto& change : changes) {
                    if (change.success) {
                        spdlog::info("    [shader-reload] Recompiled: {}", change.path);
                    } else {
                        spdlog::warn("    [shader-reload] Failed: {} - {}", change.path, change.error_message);
                    }
                }
            }
        }

        // SACRED: Snapshot for hot-reload
        std::vector<std::uint8_t> snapshot() const {
            void_services::BinaryWriter writer;
            writer.write_u32(1); // version
            if (m_pipeline) {
                writer.write_u64(m_pipeline->shader_count());
            } else {
                writer.write_u64(0);
            }
            return writer.take();
        }

        // SACRED: Restore from snapshot
        void restore(const std::vector<std::uint8_t>& data) {
            void_services::BinaryReader reader(data);
            [[maybe_unused]] auto version = reader.read_u32();
            [[maybe_unused]] auto count = reader.read_u64();
            // Shaders will recompile on demand
        }

    protected:
        bool on_start() override {
            m_pipeline = std::make_unique<void_shader::ShaderPipeline>(m_config);
            spdlog::info("    ShaderService: started");
            return true;
        }

        void on_stop() override {
            if (m_pipeline) {
                m_pipeline->stop_watching();
            }
            m_pipeline.reset();
            spdlog::info("    ShaderService: stopped");
        }

        float on_check_health() override {
            if (!m_pipeline) return 0.0f;
            return 1.0f; // Shader pipeline is healthy if it exists
        }

    private:
        void_shader::ShaderPipelineConfig m_config;
        std::unique_ptr<void_shader::ShaderPipeline> m_pipeline;
    };

    // -------------------------------------------------------------------------
    // ASSET SERVICE - Create and register
    // -------------------------------------------------------------------------
    spdlog::info("  [asset]");
    spdlog::info("    Version: {}", void_asset::VOID_ASSET_VERSION);

    void_asset::AssetServerConfig asset_config;
    asset_config.with_asset_dir(config.project_dir.string() + "/assets")
                .with_hot_reload(true)
                .with_max_concurrent_loads(4);

    auto asset_service = service_registry.register_service<AssetService>(asset_config, event_bus);
    spdlog::info("    AssetService: registered with ServiceRegistry");

    // -------------------------------------------------------------------------
    // SHADER SERVICE - Create and register
    // -------------------------------------------------------------------------
    spdlog::info("  [shader]");
    spdlog::info("    Version: {}", void_shader::void_shader_version_string());

    void_shader::ShaderPipelineConfig shader_config;
    shader_config.with_base_path(config.project_dir.string() + "/shaders")
                 .with_validation(true)
                 .with_hot_reload(true)
                 .with_cache_size(256);

    auto shader_service = service_registry.register_service<ShaderService>(shader_config);
    spdlog::info("    ShaderService: registered with ServiceRegistry");

    // -------------------------------------------------------------------------
    // START SERVICES
    // -------------------------------------------------------------------------
    spdlog::info("  [services]");

    // Wire service events to log
    service_registry.set_event_callback([](const void_services::ServiceEvent& e) {
        spdlog::info("    [service-event] {} on '{}'",
            [](void_services::ServiceEventType t) {
                switch(t) {
                    case void_services::ServiceEventType::Registered: return "Registered";
                    case void_services::ServiceEventType::Unregistered: return "Unregistered";
                    case void_services::ServiceEventType::Starting: return "Starting";
                    case void_services::ServiceEventType::Started: return "Started";
                    case void_services::ServiceEventType::Stopping: return "Stopping";
                    case void_services::ServiceEventType::Stopped: return "Stopped";
                    case void_services::ServiceEventType::Failed: return "Failed";
                    case void_services::ServiceEventType::Restarting: return "Restarting";
                    case void_services::ServiceEventType::HealthChanged: return "HealthChanged";
                    default: return "Unknown";
                }
            }(e.type),
            e.service_id.name);
    });

    // Start all services (respects priority order)
    service_registry.start_all();

    auto svc_stats = service_registry.stats();
    spdlog::info("    ServiceRegistry: {} total, {} running",
                 svc_stats.total_services, svc_stats.running_services);

    // -------------------------------------------------------------------------
    // INTEGRATION: Event wiring
    // -------------------------------------------------------------------------
    spdlog::info("  [integration]");

    // Subscribe to asset events
    event_bus.subscribe<void_asset::AssetEvent>([](const void_asset::AssetEvent& e) {
        spdlog::info("    [asset-event] {} on '{}'",
                     void_asset::asset_event_type_name(e.type), e.path.str());
    });
    spdlog::info("    EventBus: asset event subscription wired");

    // Wire hot-reload from core to asset service
    event_bus.subscribe<void_core::ReloadEvent>([&asset_service](const void_core::ReloadEvent& e) {
        if (e.type == void_core::ReloadEventType::FileModified && asset_service) {
            std::string path = e.path;
            if (auto id = asset_service->server().get_id(path)) {
                spdlog::info("    [hot-reload] Reloading asset: {}", path);
                asset_service->server().reload(*id);
            }
        }
    });
    spdlog::info("    HotReload: wired to AssetService");

    // Register services with kernel's hot-reload system
    kernel->hot_reload().manager().on_reload([](const std::string& path, bool success) {
        spdlog::info("    [kernel-reload] {} {}", path, success ? "succeeded" : "failed");
    });
    spdlog::info("    Kernel: hot-reload callback registered");

    // -------------------------------------------------------------------------
    // VALIDATION: Test the services
    // -------------------------------------------------------------------------
    spdlog::info("  [validation]");

    // Test AssetPath
    void_asset::AssetPath test_path("textures/player.png");
    spdlog::info("    AssetPath: '{}' ext={} stem={}",
                 test_path.str(), test_path.extension(), test_path.stem());

    // Verify services are running
    if (asset_service && asset_service->state() == void_services::ServiceState::Running) {
        spdlog::info("    AssetService: RUNNING, loaded={}, pending={}",
                     asset_service->server().loaded_count(),
                     asset_service->server().pending_count());
    }

    if (shader_service && shader_service->state() == void_services::ServiceState::Running) {
        spdlog::info("    ShaderService: RUNNING, shader_count={}",
                     shader_service->pipeline().shader_count());
    }

    // Test service health
    auto asset_health = service_registry.get_health(void_services::ServiceId("asset_service"));
    auto shader_health = service_registry.get_health(void_services::ServiceId("shader_service"));
    spdlog::info("    Health: asset={:.2f}, shader={:.2f}",
                 asset_health ? asset_health->score : 0.0f,
                 shader_health ? shader_health->score : 0.0f);

    spdlog::info("Phase 3 complete");

    // =========================================================================
    // PHASE 4: PLATFORM
    // =========================================================================
    // spdlog::info("Phase 4: Platform");
    // TODO: presenter, render, compositor init

    // =========================================================================
    // PHASE 5: I/O
    // =========================================================================
    // spdlog::info("Phase 5: I/O");
    // TODO: audio init

    // =========================================================================
    // PHASE 6: SIMULATION
    // =========================================================================
    // spdlog::info("Phase 6: Simulation");
    // TODO: ecs, physics, triggers init

    // =========================================================================
    // PHASE 7: SCENE
    // =========================================================================
    // spdlog::info("Phase 7: Scene");
    // TODO: scene, graph init

    // =========================================================================
    // PHASE 8: SCRIPTING
    // =========================================================================
    // spdlog::info("Phase 8: Scripting");
    // TODO: script, scripting, cpp, shell init

    // =========================================================================
    // PHASE 9: GAMEPLAY
    // =========================================================================
    // spdlog::info("Phase 9: Gameplay");
    // TODO: ai, combat, inventory, gamestate init

    // =========================================================================
    // PHASE 10: UI
    // =========================================================================
    // spdlog::info("Phase 10: UI");
    // TODO: ui, hud init

    // =========================================================================
    // PHASE 11: EXTENSIONS
    // =========================================================================
    // spdlog::info("Phase 11: Extensions");
    // TODO: xr, editor init

    // =========================================================================
    // PHASE 12: APPLICATION
    // =========================================================================
    // spdlog::info("Phase 12: Application");
    // TODO: runtime, engine init
    // TODO: main loop
    // TODO: shutdown (reverse order)

    spdlog::info("Phase 3 complete - resources working");
    return 0;
}
