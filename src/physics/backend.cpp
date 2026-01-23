/// @file backend.cpp
/// @brief Physics backend implementations for void_physics

#include <void_engine/physics/backend.hpp>
#include <void_engine/physics/world.hpp>

#include <algorithm>

namespace void_physics {

// =============================================================================
// NullBackend Implementation
// =============================================================================

BackendInfo NullBackend::info() const {
    BackendInfo info;
    info.type = PhysicsBackend::Null;
    info.name = "Null Physics";
    info.version = "1.0.0";
    info.vendor = "void_engine";
    info.capabilities = BackendCapability::Standard;

    info.performance.multithreaded = false;
    info.performance.simd_optimized = false;
    info.performance.gpu_accelerated = false;
    info.performance.recommended_max_bodies = 1000;
    info.performance.recommended_max_joints = 500;

    info.limits.max_bodies = 10000;
    info.limits.max_shapes_per_body = 16;
    info.limits.max_joints = 5000;
    info.limits.max_contact_points = 10000;

    return info;
}

void_core::Result<void> NullBackend::initialize(const PhysicsConfig& /*config*/) {
    m_initialized = true;
    return {};
}

void NullBackend::shutdown() {
    m_initialized = false;
}

std::unique_ptr<IPhysicsWorld> NullBackend::create_world(const PhysicsConfig& config) {
    return std::make_unique<PhysicsWorld>(config);
}

std::unique_ptr<ICharacterController> NullBackend::create_character_controller(
    IPhysicsWorld& world,
    const CharacterControllerConfig& config) {
    return std::make_unique<CharacterController>(world, config);
}

// =============================================================================
// JoltBackend Implementation (Stub)
// =============================================================================

struct JoltBackend::Impl {
    bool initialized = false;
    // Jolt-specific data would go here
    // JPH::PhysicsSystem* physics_system;
    // JPH::TempAllocator* temp_allocator;
    // etc.
};

BackendInfo JoltBackend::info() const {
    BackendInfo info;
    info.type = PhysicsBackend::Jolt;
    info.name = "Jolt Physics";
    info.version = "4.0.0";
    info.vendor = "Jorrit Rouwe";
    info.capabilities = BackendCapability::Standard |
                        BackendCapability::SoftBodies |
                        BackendCapability::VehiclePhysics |
                        BackendCapability::Multithreading |
                        BackendCapability::Deterministic |
                        BackendCapability::DebugRendering;

    info.performance.multithreaded = true;
    info.performance.simd_optimized = true;
    info.performance.gpu_accelerated = false;
    info.performance.recommended_max_bodies = 65536;
    info.performance.recommended_max_joints = 10000;

    info.limits.max_bodies = 1 << 20;
    info.limits.max_shapes_per_body = 256;
    info.limits.max_joints = 1 << 16;
    info.limits.max_contact_points = 1 << 20;

    return info;
}

void_core::Result<void> JoltBackend::initialize(const PhysicsConfig& /*config*/) {
    // In a real implementation:
    // 1. Initialize Jolt's allocator
    // 2. Register default Jolt types
    // 3. Create physics system
    // 4. Create job system for multithreading

    /*
    JPH::RegisterDefaultAllocator();

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    m_impl = std::make_unique<Impl>();
    m_impl->temp_allocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
    m_impl->job_system = new JPH::JobSystemThreadPool(
        JPH::cMaxPhysicsJobs,
        JPH::cMaxPhysicsBarriers,
        std::thread::hardware_concurrency() - 1
    );

    // Create physics system
    const uint cMaxBodies = config.max_bodies;
    const uint cNumBodyMutexes = 0; // Auto-detect
    const uint cMaxBodyPairs = 65536;
    const uint cMaxContactConstraints = 10240;

    m_impl->physics_system = new JPH::PhysicsSystem();
    m_impl->physics_system->Init(
        cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
        broad_phase_layer_interface,
        object_vs_broadphase_layer_filter,
        object_vs_object_layer_filter
    );

    m_impl->initialized = true;
    */

    m_impl = std::make_unique<Impl>();
    m_impl->initialized = true;
    return {};
}

void JoltBackend::shutdown() {
    if (m_impl) {
        // In a real implementation:
        // delete m_impl->physics_system;
        // delete m_impl->job_system;
        // delete m_impl->temp_allocator;
        // JPH::UnregisterTypes();
        // delete JPH::Factory::sInstance;

        m_impl->initialized = false;
        m_impl.reset();
    }
}

bool JoltBackend::is_initialized() const {
    return m_impl && m_impl->initialized;
}

std::unique_ptr<IPhysicsWorld> JoltBackend::create_world(const PhysicsConfig& config) {
    // In a real implementation, return a JoltPhysicsWorld
    // For now, return the generic PhysicsWorld
    return std::make_unique<PhysicsWorld>(config);
}

std::unique_ptr<ICharacterController> JoltBackend::create_character_controller(
    IPhysicsWorld& world,
    const CharacterControllerConfig& config) {
    // In a real implementation, return a JoltCharacterController
    return std::make_unique<CharacterController>(world, config);
}

// =============================================================================
// PhysXBackend Implementation (Stub)
// =============================================================================

struct PhysXBackend::Impl {
    bool initialized = false;
    // PhysX-specific data would go here
    // PxFoundation* foundation;
    // PxPhysics* physics;
    // PxDefaultCpuDispatcher* dispatcher;
    // etc.
};

BackendInfo PhysXBackend::info() const {
    BackendInfo info;
    info.type = PhysicsBackend::PhysX;
    info.name = "NVIDIA PhysX";
    info.version = "5.3.0";
    info.vendor = "NVIDIA";
    info.capabilities = BackendCapability::Standard |
                        BackendCapability::Cloth |
                        BackendCapability::Fluids |
                        BackendCapability::Destruction |
                        BackendCapability::VehiclePhysics |
                        BackendCapability::Multithreading |
                        BackendCapability::DebugRendering;

    info.performance.multithreaded = true;
    info.performance.simd_optimized = true;
    info.performance.gpu_accelerated = true;
    info.performance.recommended_max_bodies = 100000;
    info.performance.recommended_max_joints = 50000;

    info.limits.max_bodies = 1 << 22;
    info.limits.max_shapes_per_body = 128;
    info.limits.max_joints = 1 << 18;
    info.limits.max_contact_points = 1 << 22;

    return info;
}

void_core::Result<void> PhysXBackend::initialize(const PhysicsConfig& /*config*/) {
    // In a real implementation:
    /*
    m_impl = std::make_unique<Impl>();

    m_impl->foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_impl->allocator, m_impl->error_callback);
    if (!m_impl->foundation) {
        return void_core::Error{void_core::ErrorCode::InitializationFailed, "Failed to create PhysX foundation"};
    }

    m_impl->pvd = PxCreatePvd(*m_impl->foundation);
    PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
    m_impl->pvd->connect(*transport, PxPvdInstrumentationFlag::eALL);

    m_impl->physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_impl->foundation, PxTolerancesScale(), true, m_impl->pvd);
    if (!m_impl->physics) {
        return void_core::Error{void_core::ErrorCode::InitializationFailed, "Failed to create PhysX physics"};
    }

    m_impl->dispatcher = PxDefaultCpuDispatcherCreate(std::thread::hardware_concurrency());
    if (!m_impl->dispatcher) {
        return void_core::Error{void_core::ErrorCode::InitializationFailed, "Failed to create PhysX dispatcher"};
    }

    PxInitExtensions(*m_impl->physics, m_impl->pvd);
    m_impl->initialized = true;
    */

    m_impl = std::make_unique<Impl>();
    m_impl->initialized = true;
    return {};
}

void PhysXBackend::shutdown() {
    if (m_impl) {
        // In a real implementation:
        /*
        PxCloseExtensions();
        m_impl->dispatcher->release();
        m_impl->physics->release();
        if (m_impl->pvd) {
            PxPvdTransport* transport = m_impl->pvd->getTransport();
            m_impl->pvd->release();
            transport->release();
        }
        m_impl->foundation->release();
        */

        m_impl->initialized = false;
        m_impl.reset();
    }
}

bool PhysXBackend::is_initialized() const {
    return m_impl && m_impl->initialized;
}

std::unique_ptr<IPhysicsWorld> PhysXBackend::create_world(const PhysicsConfig& config) {
    // In a real implementation, return a PhysXWorld
    return std::make_unique<PhysicsWorld>(config);
}

std::unique_ptr<ICharacterController> PhysXBackend::create_character_controller(
    IPhysicsWorld& world,
    const CharacterControllerConfig& config) {
    // In a real implementation, return a PhysXCharacterController
    return std::make_unique<CharacterController>(world, config);
}

// =============================================================================
// BulletBackend Implementation (Stub)
// =============================================================================

struct BulletBackend::Impl {
    bool initialized = false;
    // Bullet-specific data would go here
    // btBroadphaseInterface* broadphase;
    // btDefaultCollisionConfiguration* collision_config;
    // btCollisionDispatcher* dispatcher;
    // btSequentialImpulseConstraintSolver* solver;
    // etc.
};

BackendInfo BulletBackend::info() const {
    BackendInfo info;
    info.type = PhysicsBackend::Bullet;
    info.name = "Bullet Physics";
    info.version = "3.25";
    info.vendor = "Bullet Physics Library";
    info.capabilities = BackendCapability::Standard |
                        BackendCapability::SoftBodies |
                        BackendCapability::VehiclePhysics |
                        BackendCapability::Multithreading |
                        BackendCapability::DebugRendering;

    info.performance.multithreaded = true;
    info.performance.simd_optimized = true;
    info.performance.gpu_accelerated = false;
    info.performance.recommended_max_bodies = 50000;
    info.performance.recommended_max_joints = 10000;

    info.limits.max_bodies = 1 << 18;
    info.limits.max_shapes_per_body = 64;
    info.limits.max_joints = 1 << 16;
    info.limits.max_contact_points = 1 << 18;

    return info;
}

void_core::Result<void> BulletBackend::initialize(const PhysicsConfig& /*config*/) {
    // In a real implementation:
    /*
    m_impl = std::make_unique<Impl>();

    m_impl->collision_config = new btDefaultCollisionConfiguration();
    m_impl->dispatcher = new btCollisionDispatcher(m_impl->collision_config);
    m_impl->broadphase = new btDbvtBroadphase();
    m_impl->solver = new btSequentialImpulseConstraintSolver();

    m_impl->initialized = true;
    */

    m_impl = std::make_unique<Impl>();
    m_impl->initialized = true;
    return {};
}

void BulletBackend::shutdown() {
    if (m_impl) {
        // In a real implementation:
        /*
        delete m_impl->solver;
        delete m_impl->broadphase;
        delete m_impl->dispatcher;
        delete m_impl->collision_config;
        */

        m_impl->initialized = false;
        m_impl.reset();
    }
}

bool BulletBackend::is_initialized() const {
    return m_impl && m_impl->initialized;
}

std::unique_ptr<IPhysicsWorld> BulletBackend::create_world(const PhysicsConfig& config) {
    // In a real implementation, return a BulletWorld
    return std::make_unique<PhysicsWorld>(config);
}

std::unique_ptr<ICharacterController> BulletBackend::create_character_controller(
    IPhysicsWorld& world,
    const CharacterControllerConfig& config) {
    // In a real implementation, return a BulletCharacterController
    return std::make_unique<CharacterController>(world, config);
}

// =============================================================================
// PhysicsBackendFactory Implementation
// =============================================================================

PhysicsBackendFactory& PhysicsBackendFactory::instance() {
    static PhysicsBackendFactory factory;
    return factory;
}

void PhysicsBackendFactory::register_backend(PhysicsBackend type, CreatorFunc creator) {
    auto backend = creator();
    RegisteredBackend entry;
    entry.creator = std::move(creator);
    entry.info = backend->info();
    m_backends[type] = std::move(entry);
}

void PhysicsBackendFactory::unregister_backend(PhysicsBackend type) {
    m_backends.erase(type);
}

bool PhysicsBackendFactory::is_available(PhysicsBackend type) const {
    return m_backends.find(type) != m_backends.end();
}

std::vector<PhysicsBackend> PhysicsBackendFactory::available_backends() const {
    std::vector<PhysicsBackend> result;
    result.reserve(m_backends.size());
    for (const auto& [type, entry] : m_backends) {
        result.push_back(type);
    }
    return result;
}

std::unique_ptr<IPhysicsBackend> PhysicsBackendFactory::create(PhysicsBackend type) const {
    auto it = m_backends.find(type);
    if (it == m_backends.end()) {
        return nullptr;
    }
    return it->second.creator();
}

std::unique_ptr<IPhysicsBackend> PhysicsBackendFactory::create_best() const {
    // Priority order: Jolt > PhysX > Bullet > Null
    static const PhysicsBackend priority[] = {
        PhysicsBackend::Jolt,
        PhysicsBackend::PhysX,
        PhysicsBackend::Bullet,
        PhysicsBackend::Null
    };

    for (auto type : priority) {
        if (is_available(type)) {
            return create(type);
        }
    }

    // Fallback to NullBackend
    return std::make_unique<NullBackend>();
}

const BackendInfo* PhysicsBackendFactory::get_info(PhysicsBackend type) const {
    auto it = m_backends.find(type);
    if (it == m_backends.end()) {
        return nullptr;
    }
    return &it->second.info;
}

void PhysicsBackendFactory::register_builtins() {
    // Always register NullBackend
    register_backend(PhysicsBackend::Null, []() {
        return std::make_unique<NullBackend>();
    });

    // Register JoltBackend (always available as we have the stub)
    register_backend(PhysicsBackend::Jolt, []() {
        return std::make_unique<JoltBackend>();
    });

    // Conditionally register PhysX if available
    #ifdef VOID_HAS_PHYSX
    register_backend(PhysicsBackend::PhysX, []() {
        return std::make_unique<PhysXBackend>();
    });
    #endif

    // Conditionally register Bullet if available
    #ifdef VOID_HAS_BULLET
    register_backend(PhysicsBackend::Bullet, []() {
        return std::make_unique<BulletBackend>();
    });
    #endif
}

// =============================================================================
// PhysicsSystem Implementation
// =============================================================================

PhysicsSystem::PhysicsSystem(PhysicsBackend backend) {
    // Ensure builtins are registered
    PhysicsBackendFactory::instance().register_builtins();

    // Create the requested backend
    m_backend = PhysicsBackendFactory::instance().create(backend);
    if (!m_backend) {
        // Fallback to null backend
        m_backend = std::make_unique<NullBackend>();
    }
}

PhysicsSystem::~PhysicsSystem() {
    shutdown();
}

void_core::Result<void> PhysicsSystem::initialize(const PhysicsConfig& config) {
    if (m_initialized) {
        return void_core::Error{void_core::ErrorCode::AlreadyExists, "Physics system already initialized"};
    }

    m_config = config;

    // Initialize backend
    auto result = m_backend->initialize(config);
    if (!result) {
        return result;
    }

    // Create main world
    m_main_world = m_backend->create_world(config);
    if (!m_main_world) {
        m_backend->shutdown();
        return void_core::Error{void_core::ErrorCode::InvalidState, "Failed to create main physics world"};
    }

    m_initialized = true;
    return {};
}

void PhysicsSystem::shutdown() {
    if (!m_initialized) {
        return;
    }

    m_main_world.reset();

    if (m_backend) {
        m_backend->shutdown();
    }

    m_initialized = false;
}

std::unique_ptr<IPhysicsWorld> PhysicsSystem::create_world(const PhysicsConfig& config) {
    if (!m_backend || !m_initialized) {
        return nullptr;
    }
    return m_backend->create_world(config);
}

void PhysicsSystem::step(float dt) {
    if (m_main_world) {
        m_main_world->step(dt);
    }
}

PhysicsStats PhysicsSystem::stats() const {
    if (m_main_world) {
        return m_main_world->stats();
    }
    return PhysicsStats{};
}

void_core::Result<void_core::HotReloadSnapshot> PhysicsSystem::snapshot() const {
    if (!m_main_world) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "No main world"};
    }
    return m_main_world->snapshot();
}

void_core::Result<void> PhysicsSystem::restore(void_core::HotReloadSnapshot snapshot) {
    if (!m_main_world) {
        return void_core::Error{void_core::ErrorCode::InvalidState, "No main world"};
    }
    return m_main_world->restore(std::move(snapshot));
}

} // namespace void_physics
