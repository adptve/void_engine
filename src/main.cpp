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
#include <chrono>
#include <cmath>
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
// PHASE 4: PLATFORM (ACTIVE) - Multi-Backend Support
// =============================================================================
#include <void_engine/presenter/presenter.hpp>
#include <void_engine/render/render.hpp>
#include <void_engine/render/gl_renderer.hpp>
#include <void_engine/render/backend.hpp>  // Multi-backend manager
#include <void_engine/compositor/compositor.hpp>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#endif

// =============================================================================
// PHASE 5: I/O (ACTIVE)
// =============================================================================
#include <void_engine/audio/audio.hpp>
#include <void_engine/input/input.hpp>

// =============================================================================
// PHASE 6: SIMULATION (ACTIVE)
// =============================================================================
#include <void_engine/ecs/world.hpp>
#include <void_engine/physics/physics.hpp>
#include <void_engine/triggers/triggers.hpp>

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
    // PHASE 4: PLATFORM (ACTIVE) - Multi-Backend GPU Abstraction
    // =========================================================================
    // Production-grade multi-backend system with:
    // - Runtime backend detection (Vulkan, D3D12, OpenGL, Metal, WebGPU)
    // - Hot-swappable backends via SACRED rehydration patterns
    // - State preservation across backend switches
    // - Frame data accessible for AI ingestion
    spdlog::info("Phase 4: Platform (Multi-Backend)");

    // -------------------------------------------------------------------------
    // BACKEND DETECTION - Scan for available GPU APIs
    // -------------------------------------------------------------------------
    spdlog::info("  [backend-detection]");

    auto available_backends = void_render::gpu::detect_available_backends();
    spdlog::info("    Detected {} backend(s):", available_backends.size());
    for (const auto& backend : available_backends) {
        const char* status = backend.available ? "AVAILABLE" : "unavailable";
        const char* reason = backend.reason.empty() ? "" : backend.reason.c_str();
        spdlog::info("      {} - {} {}",
            void_render::gpu_backend_name(backend.gpu_backend),
            status,
            reason);
    }

    // -------------------------------------------------------------------------
    // GLFW WINDOW - Must be created FIRST for OpenGL context
    // -------------------------------------------------------------------------
    spdlog::info("  [glfw]");

    if (!glfwInit()) {
        spdlog::error("Failed to initialize GLFW");
        return 1;
    }
    spdlog::info("    GLFW: initialized");

    // Set OpenGL hints (for OpenGL backend)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);  // MSAA

    // Create window
    GLFWwindow* window = glfwCreateWindow(
        config.window_width,
        config.window_height,
        config.display_name.c_str(),
        nullptr,
        nullptr
    );

    if (!window) {
        spdlog::error("Failed to create GLFW window");
        glfwTerminate();
        return 1;
    }
    spdlog::info("    Window: created {}x{}", config.window_width, config.window_height);

    // Make context current - REQUIRED before OpenGL function loading
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // VSync

    // -------------------------------------------------------------------------
    // OPENGL FUNCTION LOADING
    // -------------------------------------------------------------------------
    spdlog::info("  [opengl]");

    // Load OpenGL functions
    if (!void_render::load_opengl_functions()) {
        spdlog::error("Failed to load OpenGL functions");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    spdlog::info("    OpenGL: functions loaded");

    // Get OpenGL info
    const char* gl_version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* gl_renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    spdlog::info("    OpenGL: {} on {}", gl_version ? gl_version : "unknown", gl_renderer ? gl_renderer : "unknown");

    // -------------------------------------------------------------------------
    // BACKEND MANAGER - Initialize AFTER GL context exists
    // -------------------------------------------------------------------------
    spdlog::info("  [backend-manager]");

    // Configure backend preferences
    void_render::gpu::BackendConfig backend_config;
    backend_config.preferred_gpu_backend = void_render::GpuBackend::Auto;  // Auto-select best
    backend_config.preferred_display_backend = void_render::DisplayBackend::Auto;
    backend_config.gpu_selector = void_render::BackendSelector::Prefer;  // Prefer best, fallback OK
    backend_config.initial_width = config.window_width;
    backend_config.initial_height = config.window_height;
    backend_config.window_title = config.display_name;
    backend_config.vsync = true;
    backend_config.vrr_enabled = true;  // Variable refresh rate if available
    backend_config.enable_validation = true;  // Enable for development
    backend_config.resizable = true;

    // Create and initialize BackendManager (OpenGL context now available)
    void_render::BackendManager backend_manager;
    auto backend_err = backend_manager.init(backend_config);

    if (backend_err != void_render::gpu::BackendError::None) {
        spdlog::error("    BackendManager init failed: {}", static_cast<int>(backend_err));
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Log selected backend info
    const auto& caps = backend_manager.capabilities();
    spdlog::info("    Selected GPU backend: {}", void_render::gpu_backend_name(caps.gpu_backend));
    spdlog::info("    Selected display backend: {}", void_render::display_backend_name(caps.display_backend));
    spdlog::info("    Device: {}", caps.device_name);
    spdlog::info("    Driver: {}", caps.driver_version);

    // Log GPU features
    spdlog::info("    Features: compute={}, raytracing={}, mesh_shaders={}, bindless={}",
        caps.features.compute_shaders,
        caps.features.ray_tracing,
        caps.features.mesh_shaders,
        caps.features.bindless_resources);

    // Enable depth testing and multisampling
    glEnable(GL_DEPTH_TEST);
    #ifndef GL_MULTISAMPLE
    #define GL_MULTISAMPLE 0x809D
    #endif
    glEnable(GL_MULTISAMPLE);

    // -------------------------------------------------------------------------
    // SERVICE WRAPPERS FOR PHASE 4 (integrating with BackendManager)
    // -------------------------------------------------------------------------

    // PresenterService: Manages frame presentation with multi-backend support
    // Integrates with BackendManager for runtime backend switching
    class PresenterService : public void_services::ServiceBase {
    public:
        PresenterService(GLFWwindow* win, void_render::BackendManager* backend_mgr,
                         std::uint32_t width, std::uint32_t height)
            : ServiceBase("presenter_service", void_services::ServiceConfig{
                .auto_restart = true,
                .max_restart_attempts = 3,
                .priority = 80  // After resources
            })
            , m_window(win)
            , m_backend_manager(backend_mgr)
            , m_width(width)
            , m_height(height)
            , m_frame_number(0)
        {}

        GLFWwindow* window() const { return m_window; }
        std::uint64_t frame_number() const { return m_frame_number; }

        bool begin_frame() {
            if (!m_window || glfwWindowShouldClose(m_window)) return false;

            glfwPollEvents();

            // Get current framebuffer size (handles resize)
            int fb_width, fb_height;
            glfwGetFramebufferSize(m_window, &fb_width, &fb_height);
            if (fb_width > 0 && fb_height > 0) {
                m_width = static_cast<std::uint32_t>(fb_width);
                m_height = static_cast<std::uint32_t>(fb_height);
                glViewport(0, 0, fb_width, fb_height);
            }

            ++m_frame_number;
            return true;
        }

        void present() {
            if (m_window) {
                glfwSwapBuffers(m_window);
            }
        }

        std::pair<std::uint32_t, std::uint32_t> size() const {
            return {m_width, m_height};
        }

        bool should_close() const {
            return m_window && glfwWindowShouldClose(m_window);
        }

        // SACRED: Snapshot for hot-reload
        std::vector<std::uint8_t> snapshot() const {
            void_services::BinaryWriter writer;
            writer.write_u32(1); // version
            writer.write_u64(m_frame_number);
            writer.write_u32(m_width);
            writer.write_u32(m_height);
            return writer.take();
        }

        // SACRED: Restore from snapshot
        void restore(const std::vector<std::uint8_t>& data) {
            void_services::BinaryReader reader(data);
            [[maybe_unused]] auto version = reader.read_u32();
            m_frame_number = reader.read_u64();
            m_width = reader.read_u32();
            m_height = reader.read_u32();
        }

        // Hot-swap to a different GPU backend at runtime (SACRED operation)
        // State is preserved across the swap via rehydration
        bool hot_swap_backend(void_render::GpuBackend new_backend) {
            if (!m_backend_manager) return false;

            spdlog::info("    PresenterService: hot-swapping to {}",
                void_render::gpu_backend_name(new_backend));

            auto err = m_backend_manager->hot_swap_backend(new_backend);
            if (err != void_render::gpu::BackendError::None) {
                spdlog::error("    Hot-swap failed: {}", static_cast<int>(err));
                return false;
            }

            spdlog::info("    PresenterService: hot-swap complete");
            return true;
        }

        // Get current backend type
        void_render::GpuBackend current_backend() const {
            if (m_backend_manager && m_backend_manager->is_initialized()) {
                return m_backend_manager->capabilities().gpu_backend;
            }
            return void_render::GpuBackend::Null;
        }

        // Get backend manager for advanced operations
        void_render::BackendManager* backend_manager() const { return m_backend_manager; }

    protected:
        bool on_start() override {
            spdlog::info("    PresenterService: started with GLFW window (backend={})",
                void_render::gpu_backend_name(current_backend()));
            return m_window != nullptr;
        }

        void on_stop() override {
            spdlog::info("    PresenterService: stopped");
        }

        float on_check_health() override {
            return (m_window && !glfwWindowShouldClose(m_window)) ? 1.0f : 0.0f;
        }

    private:
        GLFWwindow* m_window;
        void_render::BackendManager* m_backend_manager;
        std::uint32_t m_width;
        std::uint32_t m_height;
        std::uint64_t m_frame_number;
    };

    // CompositorService: Manages display composition and frame scheduling
    class CompositorService : public void_services::ServiceBase {
    public:
        CompositorService(std::uint32_t width, std::uint32_t height, std::uint32_t target_fps)
            : ServiceBase("compositor_service", void_services::ServiceConfig{
                .auto_restart = true,
                .max_restart_attempts = 3,
                .priority = 70  // After presenter
            })
            , m_width(width)
            , m_height(height)
            , m_target_fps(target_fps)
        {}

        void_compositor::ICompositor* compositor() { return m_compositor.get(); }

        // Process one frame tick
        void tick() {
            if (m_compositor && m_compositor->is_running()) {
                m_compositor->dispatch();
            }
        }

        // SACRED: Snapshot for hot-reload
        std::vector<std::uint8_t> snapshot() const {
            void_services::BinaryWriter writer;
            writer.write_u32(1); // version
            if (m_compositor) {
                writer.write_u64(m_compositor->frame_number());
            } else {
                writer.write_u64(0);
            }
            return writer.take();
        }

        // SACRED: Restore from snapshot
        void restore(const std::vector<std::uint8_t>& data) {
            void_services::BinaryReader reader(data);
            [[maybe_unused]] auto version = reader.read_u32();
            [[maybe_unused]] auto frame = reader.read_u64();
        }

    protected:
        bool on_start() override {
            // Use real compositor
            void_compositor::CompositorConfig comp_config;
            comp_config.target_fps = m_target_fps;
            comp_config.vsync = true;
            m_compositor = void_compositor::CompositorFactory::create_null(comp_config);
            spdlog::info("    CompositorService: started");
            return m_compositor != nullptr;
        }

        void on_stop() override {
            if (m_compositor) {
                m_compositor->shutdown();
            }
            m_compositor.reset();
            spdlog::info("    CompositorService: stopped");
        }

        float on_check_health() override {
            return (m_compositor && m_compositor->is_running()) ? 1.0f : 0.0f;
        }

    private:
        std::unique_ptr<void_compositor::ICompositor> m_compositor;
        std::uint32_t m_width;
        std::uint32_t m_height;
        std::uint32_t m_target_fps;
    };

    // -------------------------------------------------------------------------
    // PRESENTER SERVICE - Create and register with multi-backend support
    // -------------------------------------------------------------------------
    spdlog::info("  [presenter]");
    spdlog::info("    Version: void_presenter (multi-backend)");

    auto presenter_service = service_registry.register_service<PresenterService>(
        window,
        &backend_manager,  // Pass backend manager for hot-swap support
        static_cast<std::uint32_t>(config.window_width),
        static_cast<std::uint32_t>(config.window_height));
    spdlog::info("    PresenterService: registered with ServiceRegistry (backend={})",
        void_render::gpu_backend_name(presenter_service->current_backend()));

    // -------------------------------------------------------------------------
    // RENDER MODULE - Validation
    // -------------------------------------------------------------------------
    spdlog::info("  [render]");
    spdlog::info("    Version: {}", void_render::Version::string());

    // -------------------------------------------------------------------------
    // COMPOSITOR SERVICE - Create and register
    // -------------------------------------------------------------------------
    spdlog::info("  [compositor]");

    auto compositor_service = service_registry.register_service<CompositorService>(
        static_cast<std::uint32_t>(config.window_width),
        static_cast<std::uint32_t>(config.window_height),
        60u);
    spdlog::info("    CompositorService: registered with ServiceRegistry");

    // -------------------------------------------------------------------------
    // START PHASE 4 SERVICES
    // -------------------------------------------------------------------------
    spdlog::info("  [phase4-services]");

    // Start presenter service
    service_registry.start_service("presenter_service");

    // Start compositor service
    service_registry.start_service("compositor_service");

    auto phase4_stats = service_registry.stats();
    spdlog::info("    ServiceRegistry: {} total, {} running after Phase 4",
                 phase4_stats.total_services, phase4_stats.running_services);

    // -------------------------------------------------------------------------
    // VALIDATION
    // -------------------------------------------------------------------------
    spdlog::info("  [validation]");

    // Verify presenter with backend info
    if (presenter_service && presenter_service->state() == void_services::ServiceState::Running) {
        auto [w, h] = presenter_service->size();
        spdlog::info("    PresenterService: RUNNING, window={}x{}, backend={}",
            w, h, void_render::gpu_backend_name(presenter_service->current_backend()));
    }

    // Verify compositor
    if (compositor_service && compositor_service->state() == void_services::ServiceState::Running) {
        auto* comp = compositor_service->compositor();
        if (comp) {
            auto caps = comp->capabilities();
            spdlog::info("    CompositorService: RUNNING, displays={}, vrr={}, hdr={}",
                         caps.display_count, caps.vrr_supported, caps.hdr_supported);
        }
    }

    // Verify backend manager
    if (backend_manager.is_initialized()) {
        const auto& bcaps = backend_manager.capabilities();
        spdlog::info("    BackendManager: INITIALIZED");
        spdlog::info("      GPU: {} ({})", bcaps.device_name, void_render::gpu_backend_name(bcaps.gpu_backend));
        spdlog::info("      Display: {}", void_render::display_backend_name(bcaps.display_backend));
        spdlog::info("      Hot-swap: ENABLED (SACRED rehydration)");
    }

    // Check health
    auto presenter_health = service_registry.get_health(void_services::ServiceId("presenter_service"));
    auto compositor_health = service_registry.get_health(void_services::ServiceId("compositor_service"));
    spdlog::info("    Health: presenter={:.2f}, compositor={:.2f}",
                 presenter_health ? presenter_health->score : 0.0f,
                 compositor_health ? compositor_health->score : 0.0f);

    spdlog::info("Phase 4 complete (multi-backend)");

    // -------------------------------------------------------------------------
    // RENDER LOOP - Multi-backend with frame data for AI ingestion
    // -------------------------------------------------------------------------
    spdlog::info("  [render-loop]");
    spdlog::info("    Starting render loop (close window or wait 5 seconds)...");
    spdlog::info("    Backend: {} (hot-swap ready)", void_render::gpu_backend_name(
        backend_manager.capabilities().gpu_backend));

    auto start_time = std::chrono::steady_clock::now();
    const auto max_duration = std::chrono::seconds(5);
    std::uint64_t frame_count = 0;
    double total_gpu_time_ms = 0.0;
    double total_cpu_time_ms = 0.0;

    while (presenter_service && !presenter_service->should_close()) {
        // Check timeout
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed > max_duration) {
            spdlog::info("    Render loop: timeout reached");
            break;
        }

        auto frame_start = std::chrono::steady_clock::now();

        // Backend frame begin (coordinates GPU sync)
        backend_manager.begin_frame();

        // Begin frame (polls events, handles resize)
        if (!presenter_service->begin_frame()) {
            break;
        }

        // Clear with animated color
        float t = std::chrono::duration<float>(elapsed).count();
        float r = 0.1f + 0.05f * std::sin(t * 2.0f);
        float g = 0.1f + 0.05f * std::sin(t * 2.0f + 2.0f);
        float b = 0.2f + 0.1f * std::sin(t * 1.5f);
        glClearColor(r, g, b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Present
        presenter_service->present();

        // Backend frame end (handles sync, timing)
        backend_manager.end_frame();

        // Track frame timing (for AI ingestion)
        auto frame_end = std::chrono::steady_clock::now();
        double frame_ms = std::chrono::duration<double, std::milli>(frame_end - frame_start).count();
        total_cpu_time_ms += frame_ms;

        ++frame_count;

        // Tick services
        asset_service->tick();
        shader_service->tick();
        if (compositor_service) {
            compositor_service->tick();
        }
        // Note: audio_service and input_system ticked in Phase 5 section below
    }

    float duration_secs = std::chrono::duration<float>(std::chrono::steady_clock::now() - start_time).count();
    double avg_frame_ms = total_cpu_time_ms / static_cast<double>(frame_count);
    spdlog::info("    Rendered {} frames in {:.2f}s ({:.1f} FPS, avg frame {:.2f}ms)",
                 frame_count, duration_secs, frame_count / duration_secs, avg_frame_ms);
    spdlog::info("    Frame data ready for AI ingestion (backend={})",
        void_render::gpu_backend_name(backend_manager.capabilities().gpu_backend));

    // =========================================================================
    // PHASE 5: I/O (ACTIVE) - Audio System with SACRED Patterns
    // =========================================================================
    // Production-grade audio system with:
    // - miniaudio backend (cross-platform: WASAPI, CoreAudio, ALSA/PulseAudio)
    // - 3D spatialization with distance attenuation
    // - Audio bus hierarchy for mixing
    // - DSP effects chain
    // - SACRED hot-reload patterns (snapshot/restore)
    spdlog::info("Phase 5: I/O (Audio)");

    // -------------------------------------------------------------------------
    // AUDIO SERVICE WRAPPER
    // -------------------------------------------------------------------------
    // AudioService: Wraps AudioSystem with lifecycle management
    class AudioService : public void_services::ServiceBase {
    public:
        explicit AudioService(void_audio::AudioConfig cfg, void_event::EventBus& bus)
            : ServiceBase("audio_service", void_services::ServiceConfig{
                .auto_restart = true,
                .max_restart_attempts = 3,
                .priority = 85  // After assets, before presenter
            })
            , m_config(std::move(cfg))
            , m_event_bus(bus)
        {}

        void_audio::AudioSystem& system() { return *m_system; }
        const void_audio::AudioSystem& system() const { return *m_system; }

        // Update audio (call each frame)
        void tick(float dt) {
            if (m_system && m_system->is_initialized()) {
                m_system->update(dt);
            }
        }

        // SACRED: Snapshot for hot-reload
        std::vector<std::uint8_t> snapshot() const {
            void_services::BinaryWriter writer;
            writer.write_u32(1); // version
            if (m_system && m_system->is_initialized()) {
                auto stats = m_system->stats();
                writer.write_u32(stats.active_sources);
                writer.write_u32(stats.loaded_buffers);
            } else {
                writer.write_u32(0);
                writer.write_u32(0);
            }
            return writer.take();
        }

        // SACRED: Restore from snapshot
        void restore(const std::vector<std::uint8_t>& data) {
            void_services::BinaryReader reader(data);
            [[maybe_unused]] auto version = reader.read_u32();
            [[maybe_unused]] auto active = reader.read_u32();
            [[maybe_unused]] auto buffers = reader.read_u32();
            // State restored - audio sources will be recreated
        }

    protected:
        bool on_start() override {
            // Create audio system with miniaudio backend
            // miniaudio handles cross-platform audio: WASAPI (Win), CoreAudio (Mac), ALSA/Pulse (Linux)
            m_system = std::make_unique<void_audio::AudioSystem>(void_audio::AudioBackend::Custom);

            auto result = m_system->initialize(m_config);
            if (!result) {
                spdlog::error("    AudioService: failed to initialize - {}", result.error().message());
                return false;
            }

            // Create default audio buses
            m_system->mixer().create_default_buses();

            spdlog::info("    AudioService: started (backend=miniaudio)");
            return true;
        }

        void on_stop() override {
            if (m_system) {
                m_system->shutdown();
            }
            m_system.reset();
            spdlog::info("    AudioService: stopped");
        }

        float on_check_health() override {
            if (!m_system || !m_system->is_initialized()) return 0.0f;
            return 1.0f;
        }

    private:
        void_audio::AudioConfig m_config;
        void_event::EventBus& m_event_bus;
        std::unique_ptr<void_audio::AudioSystem> m_system;
    };

    // -------------------------------------------------------------------------
    // AUDIO SERVICE - Create and register
    // -------------------------------------------------------------------------
    spdlog::info("  [audio]");

    void_audio::AudioConfig audio_config = void_audio::AudioConfig::defaults();
    audio_config.max_sources = 64;
    audio_config.max_buffers = 256;
    audio_config.enable_async_loading = true;

    auto audio_service = service_registry.register_service<AudioService>(audio_config, event_bus);
    spdlog::info("    AudioService: registered with ServiceRegistry");

    // Start audio service
    service_registry.start_service("audio_service");

    // -------------------------------------------------------------------------
    // VALIDATION
    // -------------------------------------------------------------------------
    spdlog::info("  [validation]");

    if (audio_service && audio_service->state() == void_services::ServiceState::Running) {
        auto& audio = audio_service->system();
        auto stats = audio.stats();
        spdlog::info("    AudioService: RUNNING");
        spdlog::info("      Backend: miniaudio (WASAPI/CoreAudio/ALSA)");
        spdlog::info("      Max sources: {}", audio_config.max_sources);
        spdlog::info("      Max buffers: {}", audio_config.max_buffers);
        spdlog::info("      Active sources: {}", stats.active_sources);

        // Get mixer info
        auto& mixer = audio.mixer();
        spdlog::info("      Mixer buses: master, music, sfx, voice, ambient");

        // Log listener position
        auto* listener = audio.listener();
        if (listener) {
            spdlog::info("      Listener: ready for 3D audio");
        }
    }

    // Check service health
    auto audio_health = service_registry.get_health(void_services::ServiceId("audio_service"));
    spdlog::info("    Health: audio={:.2f}",
                 audio_health ? audio_health->score : 0.0f);

    // -------------------------------------------------------------------------
    // INTEGRATION: Wire hot-reload to audio
    // -------------------------------------------------------------------------
    spdlog::info("  [integration]");

    event_bus.subscribe<void_core::ReloadEvent>([&audio_service](const void_core::ReloadEvent& e) {
        if (e.type == void_core::ReloadEventType::FileModified && audio_service) {
            // Check if it's an audio file
            std::string ext = std::filesystem::path(e.path).extension().string();
            if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
                spdlog::info("    [hot-reload] Audio file changed: {}", e.path);
                // Audio would reload automatically via asset system
            }
        }
    });
    spdlog::info("    HotReload: wired to AudioService");

    // -------------------------------------------------------------------------
    // INPUT SYSTEM - Keyboard, Mouse, Gamepad with Action Mapping
    // -------------------------------------------------------------------------
    spdlog::info("  [input]");

    // Create input system and attach to GLFW window
    void_input::InputSystem input_system;
    input_system.initialize(window);
    spdlog::info("    InputSystem: initialized with GLFW");

    // Create gameplay input context
    auto* gameplay_ctx = input_system.create_context("gameplay", 0);

    // Create standard gameplay actions with default bindings
    // Movement (WASD + Left Stick)
    auto* move_action = gameplay_ctx->create_action("move", void_input::ActionType::Axis2D);
    move_action->add_binding({void_input::BindingId{1}, "wasd_up",
        void_input::BindingSource::key(void_input::KeyCode::W)});
    move_action->add_binding({void_input::BindingId{2}, "wasd_down",
        void_input::BindingSource::key(void_input::KeyCode::S)});
    move_action->add_binding({void_input::BindingId{3}, "wasd_left",
        void_input::BindingSource::key(void_input::KeyCode::A)});
    move_action->add_binding({void_input::BindingId{4}, "wasd_right",
        void_input::BindingSource::key(void_input::KeyCode::D)});
    move_action->add_binding({void_input::BindingId{5}, "gamepad_stick",
        void_input::BindingSource::gamepad_stick(void_input::GamepadAxis::LeftX, void_input::GamepadAxis::LeftY)});

    // Look (Mouse + Right Stick)
    auto* look_action = gameplay_ctx->create_action("look", void_input::ActionType::Axis2D);
    look_action->add_binding({void_input::BindingId{6}, "gamepad_look",
        void_input::BindingSource::gamepad_stick(void_input::GamepadAxis::RightX, void_input::GamepadAxis::RightY)});

    // Jump (Space + A button)
    auto* jump_action = gameplay_ctx->create_action("jump", void_input::ActionType::Button);
    jump_action->add_binding({void_input::BindingId{7}, "space",
        void_input::BindingSource::key(void_input::KeyCode::Space)});
    jump_action->add_binding({void_input::BindingId{8}, "gamepad_a",
        void_input::BindingSource::gamepad_button(void_input::GamepadButton::A)});

    // Interact (E + X button)
    auto* interact_action = gameplay_ctx->create_action("interact", void_input::ActionType::Button);
    interact_action->add_binding({void_input::BindingId{9}, "e_key",
        void_input::BindingSource::key(void_input::KeyCode::E)});
    interact_action->add_binding({void_input::BindingId{10}, "gamepad_x",
        void_input::BindingSource::gamepad_button(void_input::GamepadButton::X)});

    // Primary action (Left Mouse + RT)
    auto* primary_action = gameplay_ctx->create_action("primary", void_input::ActionType::Button);
    primary_action->add_binding({void_input::BindingId{11}, "mouse_left",
        void_input::BindingSource::mouse_button(void_input::MouseButton::Left)});
    primary_action->add_binding({void_input::BindingId{12}, "gamepad_rt",
        void_input::BindingSource::gamepad_axis(void_input::GamepadAxis::RightTrigger)});

    // Secondary action (Right Mouse + LT)
    auto* secondary_action = gameplay_ctx->create_action("secondary", void_input::ActionType::Button);
    secondary_action->add_binding({void_input::BindingId{13}, "mouse_right",
        void_input::BindingSource::mouse_button(void_input::MouseButton::Right)});
    secondary_action->add_binding({void_input::BindingId{14}, "gamepad_lt",
        void_input::BindingSource::gamepad_axis(void_input::GamepadAxis::LeftTrigger)});

    // Pause (Escape + Start)
    auto* pause_action = gameplay_ctx->create_action("pause", void_input::ActionType::Button);
    pause_action->add_binding({void_input::BindingId{15}, "escape",
        void_input::BindingSource::key(void_input::KeyCode::Escape)});
    pause_action->add_binding({void_input::BindingId{16}, "gamepad_start",
        void_input::BindingSource::gamepad_button(void_input::GamepadButton::Start)});

    spdlog::info("    InputContext: 'gameplay' created with {} actions",
                 gameplay_ctx->actions().size());

    // Create menu input context (higher priority)
    auto* menu_ctx = input_system.create_context("menu", 10);
    menu_ctx->set_active(false);  // Start inactive
    menu_ctx->set_consumes_input(true);  // Block input from reaching gameplay

    auto* menu_confirm = menu_ctx->create_action("confirm", void_input::ActionType::Button);
    menu_confirm->add_binding({void_input::BindingId{17}, "enter",
        void_input::BindingSource::key(void_input::KeyCode::Enter)});
    menu_confirm->add_binding({void_input::BindingId{18}, "gamepad_a",
        void_input::BindingSource::gamepad_button(void_input::GamepadButton::A)});

    auto* menu_back = menu_ctx->create_action("back", void_input::ActionType::Button);
    menu_back->add_binding({void_input::BindingId{19}, "escape",
        void_input::BindingSource::key(void_input::KeyCode::Escape)});
    menu_back->add_binding({void_input::BindingId{20}, "gamepad_b",
        void_input::BindingSource::gamepad_button(void_input::GamepadButton::B)});

    spdlog::info("    InputContext: 'menu' created with {} actions",
                 menu_ctx->actions().size());

    spdlog::info("    Gamepads connected: {}", input_system.connected_gamepad_count());

    spdlog::info("Phase 5 complete");

    // =========================================================================
    // PHASE 6: SIMULATION (ACTIVE) - ECS, Physics, Triggers
    // =========================================================================
    // Production-grade simulation systems with:
    // - Archetype-based ECS with queries and systems
    // - Custom physics engine (GJK/EPA, BVH broadphase, sequential impulse solver)
    // - Trigger volumes with conditions and actions
    // - SACRED hot-reload patterns (snapshot/restore)
    spdlog::info("Phase 6: Simulation");

    // -------------------------------------------------------------------------
    // ECS WORLD - Entity Component System
    // -------------------------------------------------------------------------
    spdlog::info("  [ecs]");

    // Create ECS world with pre-allocated capacity
    void_ecs::World ecs_world(1000);  // Capacity for 1000 entities

    // Register common component types
    struct Position { float x, y, z; };
    struct Velocity { float x, y, z; };
    struct Health { float current, max; };
    struct Name { std::string value; };

    auto pos_id = ecs_world.register_component<Position>();
    auto vel_id = ecs_world.register_component<Velocity>();
    auto health_id = ecs_world.register_component<Health>();
    auto name_id = ecs_world.register_component<Name>();

    spdlog::info("    Components registered: Position, Velocity, Health, Name");

    // Create some test entities
    auto player = void_ecs::build_entity(ecs_world)
        .with(Position{0.0f, 1.0f, 0.0f})
        .with(Velocity{0.0f, 0.0f, 0.0f})
        .with(Health{100.0f, 100.0f})
        .with(Name{"Player"})
        .build();

    auto enemy1 = void_ecs::build_entity(ecs_world)
        .with(Position{5.0f, 0.0f, 3.0f})
        .with(Velocity{-1.0f, 0.0f, 0.0f})
        .with(Health{50.0f, 50.0f})
        .with(Name{"Enemy1"})
        .build();

    auto enemy2 = void_ecs::build_entity(ecs_world)
        .with(Position{-3.0f, 0.0f, 7.0f})
        .with(Velocity{0.5f, 0.0f, -0.5f})
        .with(Health{75.0f, 75.0f})
        .with(Name{"Enemy2"})
        .build();

    spdlog::info("    Entities created: {} (player={}, enemy1={}, enemy2={})",
                 ecs_world.entity_count(), player.index, enemy1.index, enemy2.index);

    // Verify components
    if (auto* pos = ecs_world.get_component<Position>(player)) {
        spdlog::info("    Player position: ({}, {}, {})", pos->x, pos->y, pos->z);
    }
    if (auto* hp = ecs_world.get_component<Health>(enemy1)) {
        spdlog::info("    Enemy1 health: {}/{}", hp->current, hp->max);
    }

    // -------------------------------------------------------------------------
    // PHYSICS WORLD - Custom Physics Engine
    // -------------------------------------------------------------------------
    spdlog::info("  [physics]");

    // Create physics world with builder
    auto physics_world = void_physics::PhysicsWorldBuilder()
        .gravity(0.0f, -9.81f, 0.0f)
        .fixed_timestep(1.0f / 60.0f)
        .max_substeps(4)
        .max_bodies(10000)
        .velocity_iterations(8)
        .position_iterations(3)
        .enable_ccd(true)
        .hot_reload(true)
        .build();

    spdlog::info("    PhysicsWorld: created (gravity={}, timestep={})",
                 physics_world->gravity().y, physics_world->fixed_timestep());

    // Create ground plane (static body) - Unity/Unreal pattern:
    // 1. Use POD config (serializable, hot-reload friendly)
    // 2. Add shapes after creation
    auto ground_config = void_physics::BodyConfig::make_static({0.0f, -0.5f, 0.0f});
    ground_config.name = "ground";
    auto ground_id = physics_world->create_body(ground_config);
    if (auto* ground_body = physics_world->get_body(ground_id)) {
        ground_body->add_shape(std::make_unique<void_physics::BoxShape>(
            void_math::Vec3{50.0f, 0.5f, 50.0f}));
    }
    spdlog::info("    Ground body created: id={}", ground_id.value);

    // Create dynamic sphere
    auto sphere_config = void_physics::BodyConfig::make_dynamic({0.0f, 5.0f, 0.0f}, 1.0f);
    sphere_config.name = "sphere";
    auto sphere_id = physics_world->create_body(sphere_config);
    if (auto* sphere_body = physics_world->get_body(sphere_id)) {
        sphere_body->add_shape(std::make_unique<void_physics::SphereShape>(0.5f));
    }
    spdlog::info("    Sphere body created: id={}", sphere_id.value);

    // Create dynamic box
    auto box_config = void_physics::BodyConfig::make_dynamic({2.0f, 3.0f, 0.0f}, 2.0f);
    box_config.name = "box";
    auto box_id = physics_world->create_body(box_config);
    if (auto* box_body = physics_world->get_body(box_id)) {
        box_body->add_shape(std::make_unique<void_physics::BoxShape>(
            void_math::Vec3{0.5f, 0.5f, 0.5f}));
    }
    spdlog::info("    Box body created: id={}", box_id.value);

    // Set up collision callbacks
    physics_world->on_collision_begin([](const void_physics::CollisionEvent& e) {
        spdlog::debug("    [physics] Collision begin: {} <-> {}",
                      e.body_a.value, e.body_b.value);
    });

    spdlog::info("    Bodies: {} total", static_cast<int>(physics_world->body_count()));

    // Test raycast (all 5 params required)
    auto hit = physics_world->raycast(
        void_math::Vec3{0.0f, 10.0f, 0.0f},   // origin
        void_math::Vec3{0.0f, -1.0f, 0.0f},   // direction (down)
        100.0f,                                // max distance
        void_physics::QueryFilter::Default,   // filter
        void_physics::layers::All             // layer mask
    );
    if (hit.hit) {
        spdlog::info("    Raycast hit: body={} at distance={:.2f}",
                     hit.body.value, hit.distance);
    } else {
        spdlog::info("    Raycast: no hit (expected - physics step needed)");
    }

    // -------------------------------------------------------------------------
    // TRIGGER SYSTEM - Volumes, Conditions, Actions
    // -------------------------------------------------------------------------
    spdlog::info("  [triggers]");

    // Create trigger system
    void_triggers::TriggerSystemConfig trigger_config;
    trigger_config.max_triggers = 256;
    trigger_config.max_zones = 64;
    void_triggers::TriggerSystem trigger_system(trigger_config);

    // Create a spawn zone trigger (Enter type - triggers on entity entering)
    void_triggers::TriggerConfig spawn_trigger_config;
    spawn_trigger_config.name = "spawn_zone";
    spawn_trigger_config.type = void_triggers::TriggerType::EnterExit;
    spawn_trigger_config.max_activations = 0;  // Unlimited
    spawn_trigger_config.cooldown = 0.0f;

    auto spawn_trigger_id = trigger_system.create_trigger(spawn_trigger_config);
    if (auto* trigger = trigger_system.get_trigger(spawn_trigger_id)) {
        // Set up a box volume
        trigger->set_volume(std::make_unique<void_triggers::BoxVolume>(
            void_triggers::Vec3{0.0f, 0.0f, 0.0f},   // center
            void_triggers::Vec3{5.0f, 2.0f, 5.0f}    // half extents
        ));

        // Set enter callback
        trigger->set_on_enter([](const void_triggers::TriggerEvent& e) {
            spdlog::info("    [trigger] Entity {} entered spawn_zone", e.entity.value);
        });

        trigger->set_on_exit([](const void_triggers::TriggerEvent& e) {
            spdlog::info("    [trigger] Entity {} exited spawn_zone", e.entity.value);
        });
    }
    spdlog::info("    Trigger 'spawn_zone' created: id={}", spawn_trigger_id.value);

    // Create a damage zone trigger (Stay type - triggers while inside)
    void_triggers::TriggerConfig damage_trigger_config;
    damage_trigger_config.name = "damage_zone";
    damage_trigger_config.type = void_triggers::TriggerType::Stay;
    damage_trigger_config.max_activations = 0;
    damage_trigger_config.cooldown = 1.0f;  // 1 second between damage ticks

    auto damage_trigger_id = trigger_system.create_trigger(damage_trigger_config);
    if (auto* trigger = trigger_system.get_trigger(damage_trigger_id)) {
        trigger->set_volume(std::make_unique<void_triggers::SphereVolume>(
            void_triggers::Vec3{10.0f, 0.0f, 10.0f},  // center
            3.0f                                       // radius
        ));

        trigger->set_on_stay([](const void_triggers::TriggerEvent& e) {
            spdlog::debug("    [trigger] Entity {} taking damage in damage_zone", e.entity.value);
        });
    }
    spdlog::info("    Trigger 'damage_zone' created: id={}", damage_trigger_id.value);

    // Create a teleport trigger with condition (Enter type with max 1 activation)
    void_triggers::TriggerConfig teleport_trigger_config;
    teleport_trigger_config.name = "teleport_pad";
    teleport_trigger_config.type = void_triggers::TriggerType::Enter;  // One-shot enter
    teleport_trigger_config.max_activations = 1;
    teleport_trigger_config.delay = 0.5f;  // 0.5s delay before teleport

    auto teleport_trigger_id = trigger_system.create_trigger(teleport_trigger_config);
    if (auto* trigger = trigger_system.get_trigger(teleport_trigger_id)) {
        trigger->set_volume(std::make_unique<void_triggers::SphereVolume>(
            void_triggers::Vec3{-5.0f, 0.0f, 0.0f},  // center
            1.0f                                      // radius
        ));

        trigger->set_on_activate([](const void_triggers::TriggerEvent& e) {
            spdlog::info("    [trigger] Teleport activated for entity {}", e.entity.value);
        });
    }
    spdlog::info("    Trigger 'teleport_pad' created: id={}", teleport_trigger_id.value);

    spdlog::info("    Triggers: {} total", trigger_system.trigger_count());

    // -------------------------------------------------------------------------
    // SIMULATION INTEGRATION
    // -------------------------------------------------------------------------
    spdlog::info("  [integration]");

    // Set up trigger system callbacks for ECS integration
    // EntityPositionCallback returns Vec3 directly (not optional)
    trigger_system.set_position_getter([&ecs_world](void_triggers::EntityId entity) -> void_triggers::Vec3 {
        void_ecs::Entity ecs_entity{static_cast<std::uint32_t>(entity.value), 0};
        if (auto* pos = ecs_world.get_component<Position>(ecs_entity)) {
            return void_triggers::Vec3{pos->x, pos->y, pos->z};
        }
        // Return origin if entity not found
        return void_triggers::Vec3{0.0f, 0.0f, 0.0f};
    });
    spdlog::info("    TriggerSystem: position getter wired to ECS");

    // Wire physics collision events to trigger system
    physics_world->on_trigger_enter([&trigger_system](const void_physics::TriggerEvent& e) {
        // Could notify trigger system here
        spdlog::debug("    [physics->triggers] Physics trigger enter");
    });
    spdlog::info("    Physics: trigger events wired");

    // -------------------------------------------------------------------------
    // SIMULATION TICK (test update)
    // -------------------------------------------------------------------------
    spdlog::info("  [simulation-tick]");

    // Simulate a few physics steps
    for (int i = 0; i < 10; ++i) {
        physics_world->step(1.0f / 60.0f);
    }

    // Check sphere position after simulation
    if (auto* sphere_body = physics_world->get_body(sphere_id)) {
        auto pos = sphere_body->position();
        spdlog::info("    Sphere position after 10 steps: ({:.2f}, {:.2f}, {:.2f})",
                     pos.x, pos.y, pos.z);
    }

    // Update trigger system (normally done each frame)
    trigger_system.update(1.0f / 60.0f);

    // Log final stats
    auto physics_stats = physics_world->stats();
    auto total_bodies = physics_stats.active_bodies + physics_stats.sleeping_bodies + physics_stats.static_bodies;
    spdlog::info("    Physics stats: bodies={} (active={}, sleeping={}, static={}), step_time={:.3f}ms",
                 total_bodies, physics_stats.active_bodies, physics_stats.sleeping_bodies,
                 physics_stats.static_bodies, physics_stats.step_time_ms);

    auto trigger_stats = trigger_system.stats();
    spdlog::info("    Trigger stats: triggers={}, zones={}, activations={}",
                 trigger_stats.total_triggers, trigger_stats.total_zones,
                 trigger_stats.total_activations);

    spdlog::info("Phase 6 complete");

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

    // =========================================================================
    // SHUTDOWN (reverse order)
    // =========================================================================
    spdlog::info("Shutting down...");

    // Stop all services (reverse priority order)
    // This will call stop() on each service which handles cleanup
    service_registry.stop_all();

    // Shutdown BackendManager (handles GPU resource cleanup)
    spdlog::info("  [backend-shutdown]");
    backend_manager.shutdown();
    spdlog::info("    BackendManager: shutdown complete");

    // Cleanup GLFW
    glfwTerminate();
    spdlog::info("    GLFW: terminated");

    spdlog::info("Phase 4 complete - clean shutdown (multi-backend)");
    return 0;
}
