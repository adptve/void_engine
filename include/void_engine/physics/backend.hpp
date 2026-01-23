/// @file backend.hpp
/// @brief Physics backend abstraction for void_physics
///
/// This file provides the abstraction layer for different physics engines:
/// - Jolt Physics (recommended)
/// - NVIDIA PhysX
/// - Bullet Physics
/// - Custom implementations

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "world.hpp"

#include <void_engine/core/error.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace void_physics {

// =============================================================================
// Backend Capabilities
// =============================================================================

/// Backend feature flags
enum class BackendCapability : std::uint32_t {
    None                = 0,
    Raycast             = 1 << 0,
    ShapeCast           = 1 << 1,
    Overlap             = 1 << 2,
    ContinuousDetection = 1 << 3,
    Joints              = 1 << 4,
    CharacterController = 1 << 5,
    SoftBodies          = 1 << 6,
    Cloth               = 1 << 7,
    Fluids              = 1 << 8,
    Destruction         = 1 << 9,
    VehiclePhysics      = 1 << 10,
    Multithreading      = 1 << 11,
    Deterministic       = 1 << 12,
    HotReload           = 1 << 13,
    DebugRendering      = 1 << 14,

    Standard = Raycast | ShapeCast | Overlap | ContinuousDetection | Joints | CharacterController,
    Full = 0xFFFFFFFF,
};

/// Bitwise operations
inline BackendCapability operator|(BackendCapability a, BackendCapability b) {
    return static_cast<BackendCapability>(
        static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline BackendCapability operator&(BackendCapability a, BackendCapability b) {
    return static_cast<BackendCapability>(
        static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

inline bool has_capability(BackendCapability caps, BackendCapability check) {
    return (caps & check) == check;
}

// =============================================================================
// Backend Information
// =============================================================================

/// Information about a physics backend
struct BackendInfo {
    PhysicsBackend type = PhysicsBackend::Null;
    std::string name;
    std::string version;
    std::string vendor;
    BackendCapability capabilities = BackendCapability::None;

    /// Performance characteristics
    struct Performance {
        bool multithreaded = false;
        bool simd_optimized = false;
        bool gpu_accelerated = false;
        std::uint32_t recommended_max_bodies = 10000;
        std::uint32_t recommended_max_joints = 5000;
    } performance;

    /// Memory limits
    struct Limits {
        std::uint32_t max_bodies = 65536;
        std::uint32_t max_shapes_per_body = 64;
        std::uint32_t max_joints = 65536;
        std::uint32_t max_contact_points = 262144;
    } limits;
};

// =============================================================================
// Backend Interface
// =============================================================================

/// Physics backend interface
class IPhysicsBackend {
public:
    virtual ~IPhysicsBackend() = default;

    /// Get backend information
    [[nodiscard]] virtual BackendInfo info() const = 0;

    /// Get backend type
    [[nodiscard]] virtual PhysicsBackend type() const = 0;

    /// Initialize the backend
    [[nodiscard]] virtual void_core::Result<void> initialize(const PhysicsConfig& config) = 0;

    /// Shutdown the backend
    virtual void shutdown() = 0;

    /// Check if backend is initialized
    [[nodiscard]] virtual bool is_initialized() const = 0;

    /// Create a physics world
    [[nodiscard]] virtual std::unique_ptr<IPhysicsWorld> create_world(const PhysicsConfig& config) = 0;

    /// Create a character controller
    [[nodiscard]] virtual std::unique_ptr<ICharacterController> create_character_controller(
        IPhysicsWorld& world,
        const CharacterControllerConfig& config) = 0;

    /// Check if capability is supported
    [[nodiscard]] virtual bool supports(BackendCapability cap) const {
        return has_capability(info().capabilities, cap);
    }
};

// =============================================================================
// Null Backend (Testing)
// =============================================================================

/// Null physics backend for testing
class NullBackend : public IPhysicsBackend {
public:
    [[nodiscard]] BackendInfo info() const override;
    [[nodiscard]] PhysicsBackend type() const override { return PhysicsBackend::Null; }
    [[nodiscard]] void_core::Result<void> initialize(const PhysicsConfig& config) override;
    void shutdown() override;
    [[nodiscard]] bool is_initialized() const override { return m_initialized; }
    [[nodiscard]] std::unique_ptr<IPhysicsWorld> create_world(const PhysicsConfig& config) override;
    [[nodiscard]] std::unique_ptr<ICharacterController> create_character_controller(
        IPhysicsWorld& world,
        const CharacterControllerConfig& config) override;

private:
    bool m_initialized = false;
};

// =============================================================================
// Backend Factory
// =============================================================================

/// Factory for creating physics backends
class PhysicsBackendFactory {
public:
    /// Backend creator function type
    using CreatorFunc = std::function<std::unique_ptr<IPhysicsBackend>()>;

    /// Get singleton instance
    [[nodiscard]] static PhysicsBackendFactory& instance();

    /// Register a backend creator
    void register_backend(PhysicsBackend type, CreatorFunc creator);

    /// Unregister a backend
    void unregister_backend(PhysicsBackend type);

    /// Check if backend is available
    [[nodiscard]] bool is_available(PhysicsBackend type) const;

    /// Get available backends
    [[nodiscard]] std::vector<PhysicsBackend> available_backends() const;

    /// Create backend instance
    [[nodiscard]] std::unique_ptr<IPhysicsBackend> create(PhysicsBackend type) const;

    /// Create best available backend
    [[nodiscard]] std::unique_ptr<IPhysicsBackend> create_best() const;

    /// Get backend info without creating
    [[nodiscard]] const BackendInfo* get_info(PhysicsBackend type) const;

    /// Register all built-in backends
    void register_builtins();

private:
    PhysicsBackendFactory() = default;

    struct RegisteredBackend {
        CreatorFunc creator;
        BackendInfo info;
    };
    std::unordered_map<PhysicsBackend, RegisteredBackend> m_backends;
};

// =============================================================================
// Backend Registration Macros
// =============================================================================

/// Register a physics backend
#define VOID_REGISTER_PHYSICS_BACKEND(BackendType, BackendClass) \
    namespace { \
        struct BackendClass##Registrar { \
            BackendClass##Registrar() { \
                void_physics::PhysicsBackendFactory::instance().register_backend( \
                    BackendType, \
                    []() { return std::make_unique<BackendClass>(); } \
                ); \
            } \
        }; \
        static BackendClass##Registrar g_##BackendClass##Registrar; \
    }

// =============================================================================
// Jolt Backend (Interface Only)
// =============================================================================

/// Jolt Physics backend interface
/// Implementation would be in a separate file using Jolt headers
class JoltBackend : public IPhysicsBackend {
public:
    [[nodiscard]] BackendInfo info() const override;
    [[nodiscard]] PhysicsBackend type() const override { return PhysicsBackend::Jolt; }
    [[nodiscard]] void_core::Result<void> initialize(const PhysicsConfig& config) override;
    void shutdown() override;
    [[nodiscard]] bool is_initialized() const override;
    [[nodiscard]] std::unique_ptr<IPhysicsWorld> create_world(const PhysicsConfig& config) override;
    [[nodiscard]] std::unique_ptr<ICharacterController> create_character_controller(
        IPhysicsWorld& world,
        const CharacterControllerConfig& config) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// =============================================================================
// PhysX Backend (Interface Only)
// =============================================================================

/// NVIDIA PhysX backend interface
class PhysXBackend : public IPhysicsBackend {
public:
    [[nodiscard]] BackendInfo info() const override;
    [[nodiscard]] PhysicsBackend type() const override { return PhysicsBackend::PhysX; }
    [[nodiscard]] void_core::Result<void> initialize(const PhysicsConfig& config) override;
    void shutdown() override;
    [[nodiscard]] bool is_initialized() const override;
    [[nodiscard]] std::unique_ptr<IPhysicsWorld> create_world(const PhysicsConfig& config) override;
    [[nodiscard]] std::unique_ptr<ICharacterController> create_character_controller(
        IPhysicsWorld& world,
        const CharacterControllerConfig& config) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// =============================================================================
// Bullet Backend (Interface Only)
// =============================================================================

/// Bullet Physics backend interface
class BulletBackend : public IPhysicsBackend {
public:
    [[nodiscard]] BackendInfo info() const override;
    [[nodiscard]] PhysicsBackend type() const override { return PhysicsBackend::Bullet; }
    [[nodiscard]] void_core::Result<void> initialize(const PhysicsConfig& config) override;
    void shutdown() override;
    [[nodiscard]] bool is_initialized() const override;
    [[nodiscard]] std::unique_ptr<IPhysicsWorld> create_world(const PhysicsConfig& config) override;
    [[nodiscard]] std::unique_ptr<ICharacterController> create_character_controller(
        IPhysicsWorld& world,
        const CharacterControllerConfig& config) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// =============================================================================
// Physics System
// =============================================================================

/// High-level physics system managing backend and worlds
class PhysicsSystem {
public:
    /// Create physics system with specified backend
    explicit PhysicsSystem(PhysicsBackend backend = PhysicsBackend::Null);
    ~PhysicsSystem();

    // Non-copyable
    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;

    /// Initialize the system
    [[nodiscard]] void_core::Result<void> initialize(const PhysicsConfig& config);

    /// Shutdown the system
    void shutdown();

    /// Check if initialized
    [[nodiscard]] bool is_initialized() const { return m_initialized; }

    /// Get backend
    [[nodiscard]] IPhysicsBackend* backend() { return m_backend.get(); }
    [[nodiscard]] const IPhysicsBackend* backend() const { return m_backend.get(); }

    /// Create a world
    [[nodiscard]] std::unique_ptr<IPhysicsWorld> create_world(const PhysicsConfig& config);

    /// Get main world
    [[nodiscard]] IPhysicsWorld* main_world() { return m_main_world.get(); }
    [[nodiscard]] const IPhysicsWorld* main_world() const { return m_main_world.get(); }

    /// Step simulation on main world
    void step(float dt);

    /// Get statistics
    [[nodiscard]] PhysicsStats stats() const;

    /// Hot-reload support
    [[nodiscard]] void_core::Result<void_core::HotReloadSnapshot> snapshot() const;
    [[nodiscard]] void_core::Result<void> restore(void_core::HotReloadSnapshot snapshot);

private:
    std::unique_ptr<IPhysicsBackend> m_backend;
    std::unique_ptr<IPhysicsWorld> m_main_world;
    PhysicsConfig m_config;
    bool m_initialized = false;
};

} // namespace void_physics
