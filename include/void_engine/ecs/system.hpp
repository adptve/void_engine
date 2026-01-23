#pragma once

/// @file system.hpp
/// @brief System definition and scheduling for void_ecs
///
/// Systems are functions that operate on entities with specific components.
/// The scheduler organizes systems into stages and manages execution order.

#include "fwd.hpp"
#include "query.hpp"

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <typeindex>
#include <array>

namespace void_ecs {

// Forward declaration
class World;

// =============================================================================
// SystemId
// =============================================================================

/// Unique identifier for a system
struct SystemId {
    std::size_t id;

    constexpr explicit SystemId(std::size_t i = 0) noexcept : id(i) {}

    /// Create from string (hash-based)
    [[nodiscard]] static SystemId from_name(const std::string& name) {
        return SystemId{std::hash<std::string>{}(name)};
    }

    [[nodiscard]] constexpr bool operator==(const SystemId& other) const noexcept {
        return id == other.id;
    }
    [[nodiscard]] constexpr bool operator!=(const SystemId& other) const noexcept {
        return id != other.id;
    }
};

} // namespace void_ecs

template<>
struct std::hash<void_ecs::SystemId> {
    [[nodiscard]] std::size_t operator()(const void_ecs::SystemId& s) const noexcept {
        return s.id;
    }
};

namespace void_ecs {

// =============================================================================
// SystemStage
// =============================================================================

/// Execution stage for systems
enum class SystemStage : std::uint8_t {
    First = 0,       // Initialization
    PreUpdate,       // Before main game logic
    Update,          // Main game logic (default)
    PostUpdate,      // After game logic
    PreRender,       // Before rendering
    Render,          // Actual rendering
    PostRender,      // After rendering
    Last,            // Cleanup

    COUNT            // Number of stages
};

/// Number of system stages
constexpr std::size_t SYSTEM_STAGE_COUNT = static_cast<std::size_t>(SystemStage::COUNT);

// =============================================================================
// ResourceAccess
// =============================================================================

/// Resource access declaration for conflict detection
struct ResourceAccess {
    std::type_index type_id;
    Access access;

    ResourceAccess(std::type_index t, Access a) : type_id(t), access(a) {}

    template<typename R>
    [[nodiscard]] static ResourceAccess read() {
        return ResourceAccess{std::type_index(typeid(R)), Access::Read};
    }

    template<typename R>
    [[nodiscard]] static ResourceAccess write() {
        return ResourceAccess{std::type_index(typeid(R)), Access::Write};
    }
};

// =============================================================================
// SystemDescriptor
// =============================================================================

/// Metadata for a system
class SystemDescriptor {
public:
    std::string name;
    SystemStage stage{SystemStage::Update};
    std::vector<QueryDescriptor> queries;
    std::vector<ResourceAccess> resources;
    std::vector<SystemId> run_after;
    std::vector<SystemId> run_before;
    bool exclusive{false};  // Can't run in parallel

    // =========================================================================
    // Builder Methods
    // =========================================================================

    SystemDescriptor() = default;

    explicit SystemDescriptor(std::string n) : name(std::move(n)) {}

    /// Set execution stage
    SystemDescriptor& set_stage(SystemStage s) {
        stage = s;
        return *this;
    }

    /// Add a query requirement
    SystemDescriptor& add_query(QueryDescriptor query) {
        queries.push_back(std::move(query));
        return *this;
    }

    /// Add read resource requirement
    template<typename R>
    SystemDescriptor& read_resource() {
        resources.push_back(ResourceAccess::read<R>());
        return *this;
    }

    /// Add write resource requirement
    template<typename R>
    SystemDescriptor& write_resource() {
        resources.push_back(ResourceAccess::write<R>());
        return *this;
    }

    /// Add ordering constraint (run after another system)
    SystemDescriptor& after(SystemId system) {
        run_after.push_back(system);
        return *this;
    }

    /// Add ordering constraint (run before another system)
    SystemDescriptor& before(SystemId system) {
        run_before.push_back(system);
        return *this;
    }

    /// Mark as exclusive (can't run in parallel)
    SystemDescriptor& set_exclusive() {
        exclusive = true;
        return *this;
    }

    // =========================================================================
    // Conflict Detection
    // =========================================================================

    /// Check if this system conflicts with another
    [[nodiscard]] bool conflicts_with(const SystemDescriptor& other) const {
        // Exclusive systems conflict with everything
        if (exclusive || other.exclusive) {
            return true;
        }

        // Check resource conflicts
        for (const auto& res : resources) {
            if (res.access == Access::Write) {
                // Our write conflicts with any access
                for (const auto& other_res : other.resources) {
                    if (res.type_id == other_res.type_id) {
                        return true;
                    }
                }
            }
        }

        // Check for their writes conflicting with our access
        for (const auto& other_res : other.resources) {
            if (other_res.access == Access::Write) {
                for (const auto& res : resources) {
                    if (res.type_id == other_res.type_id) {
                        return true;
                    }
                }
            }
        }

        // Check query conflicts
        for (const auto& query : queries) {
            for (const auto& other_query : other.queries) {
                if (query.conflicts_with(other_query)) {
                    return true;
                }
            }
        }

        return false;
    }

    /// Get system ID
    [[nodiscard]] SystemId id() const {
        return SystemId::from_name(name);
    }
};

// =============================================================================
// System Interface
// =============================================================================

/// Base interface for systems
class System {
public:
    virtual ~System() = default;

    /// Get system descriptor
    [[nodiscard]] virtual const SystemDescriptor& descriptor() const = 0;

    /// Run the system
    virtual void run(World& world) = 0;

    /// Called when system is added to world
    virtual void on_add(World& /*world*/) {}

    /// Called when system is removed from world
    virtual void on_remove(World& /*world*/) {}
};

// =============================================================================
// FunctionSystem
// =============================================================================

/// System implemented as a function/lambda
template<typename F>
class FunctionSystem : public System {
private:
    SystemDescriptor descriptor_;
    F func_;

public:
    FunctionSystem(SystemDescriptor desc, F func)
        : descriptor_(std::move(desc))
        , func_(std::move(func)) {}

    [[nodiscard]] const SystemDescriptor& descriptor() const override {
        return descriptor_;
    }

    void run(World& world) override {
        func_(world);
    }
};

/// Create a function system
template<typename F>
[[nodiscard]] std::unique_ptr<System> make_system(SystemDescriptor desc, F&& func) {
    return std::make_unique<FunctionSystem<std::decay_t<F>>>(
        std::move(desc), std::forward<F>(func));
}

/// Create a simple system with just a name and function
template<typename F>
[[nodiscard]] std::unique_ptr<System> make_system(const std::string& name, F&& func) {
    return make_system(SystemDescriptor(name), std::forward<F>(func));
}

// =============================================================================
// SystemBatch
// =============================================================================

/// Batch of systems that can run in parallel
struct SystemBatch {
    std::vector<std::size_t> system_indices;

    SystemBatch() = default;

    void add(std::size_t index) {
        system_indices.push_back(index);
    }

    [[nodiscard]] const std::vector<std::size_t>& systems() const noexcept {
        return system_indices;
    }

    [[nodiscard]] bool empty() const noexcept {
        return system_indices.empty();
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return system_indices.size();
    }
};

// =============================================================================
// SystemScheduler
// =============================================================================

/// Manages system execution order
class SystemScheduler {
public:
    using size_type = std::size_t;

private:
    std::array<std::vector<std::unique_ptr<System>>, SYSTEM_STAGE_COUNT> stages_;
    bool dirty_{false};

public:
    // =========================================================================
    // System Management
    // =========================================================================

    /// Add a system
    void add_system(std::unique_ptr<System> system) {
        const auto& desc = system->descriptor();
        std::size_t stage_idx = static_cast<std::size_t>(desc.stage);
        stages_[stage_idx].push_back(std::move(system));
        dirty_ = true;
    }

    /// Add a function system
    template<typename F>
    void add_system(SystemDescriptor desc, F&& func) {
        add_system(make_system(std::move(desc), std::forward<F>(func)));
    }

    /// Add a simple named system
    template<typename F>
    void add_system(const std::string& name, F&& func) {
        add_system(make_system(name, std::forward<F>(func)));
    }

    // =========================================================================
    // Execution
    // =========================================================================

    /// Run all systems in order
    void run(World& world) {
        for (auto& stage : stages_) {
            for (auto& system : stage) {
                system->run(world);
            }
        }
    }

    /// Run systems in a specific stage
    void run_stage(World& world, SystemStage stage) {
        std::size_t stage_idx = static_cast<std::size_t>(stage);
        for (auto& system : stages_[stage_idx]) {
            system->run(world);
        }
    }

    // =========================================================================
    // Query
    // =========================================================================

    /// Get systems in a stage
    [[nodiscard]] const std::vector<std::unique_ptr<System>>& systems_in_stage(
            SystemStage stage) const {
        return stages_[static_cast<std::size_t>(stage)];
    }

    /// Total number of systems
    [[nodiscard]] size_type size() const noexcept {
        size_type total = 0;
        for (const auto& stage : stages_) {
            total += stage.size();
        }
        return total;
    }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }

    // =========================================================================
    // Parallel Batching
    // =========================================================================

    /// Create batches of non-conflicting systems for parallel execution
    [[nodiscard]] std::vector<SystemBatch> create_parallel_batches(
            SystemStage stage) const {
        std::vector<SystemBatch> batches;
        const auto& systems = stages_[static_cast<std::size_t>(stage)];

        if (systems.empty()) {
            return batches;
        }

        std::vector<bool> scheduled(systems.size(), false);

        while (true) {
            SystemBatch batch;

            for (size_type i = 0; i < systems.size(); ++i) {
                if (scheduled[i]) continue;

                const auto& desc = systems[i]->descriptor();
                bool conflicts = false;

                // Check conflicts with systems already in this batch
                for (size_type j : batch.system_indices) {
                    if (desc.conflicts_with(systems[j]->descriptor())) {
                        conflicts = true;
                        break;
                    }
                }

                if (!conflicts) {
                    batch.add(i);
                    scheduled[i] = true;
                }
            }

            if (batch.empty()) {
                break;  // All systems scheduled
            }

            batches.push_back(std::move(batch));
        }

        return batches;
    }
};

} // namespace void_ecs
