#pragma once

/// @file namespace.hpp
/// @brief Namespace isolation and permissions for void_ir

#include "fwd.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <optional>
#include <functional>

namespace void_ir {

// =============================================================================
// NamespaceId
// =============================================================================

/// Unique namespace identifier
struct NamespaceId {
    std::uint32_t value = UINT32_MAX;

    /// Default constructor creates invalid ID
    constexpr NamespaceId() noexcept = default;

    /// Explicit construction
    constexpr explicit NamespaceId(std::uint32_t v) noexcept : value(v) {}

    /// Check if valid
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value != UINT32_MAX;
    }

    /// Create invalid ID
    [[nodiscard]] static constexpr NamespaceId invalid() noexcept {
        return NamespaceId{};
    }

    /// Comparison operators
    constexpr bool operator==(const NamespaceId& other) const noexcept = default;
    constexpr bool operator!=(const NamespaceId& other) const noexcept = default;
    constexpr bool operator<(const NamespaceId& other) const noexcept {
        return value < other.value;
    }
};

// =============================================================================
// EntityRef
// =============================================================================

/// Reference to an entity within a namespace
struct EntityRef {
    NamespaceId namespace_id;
    std::uint64_t entity_id = 0;

    /// Default constructor
    constexpr EntityRef() noexcept = default;

    /// Construct with namespace and entity
    constexpr EntityRef(NamespaceId ns, std::uint64_t id) noexcept
        : namespace_id(ns), entity_id(id) {}

    /// Check if valid
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return namespace_id.is_valid();
    }

    /// Comparison operators
    constexpr bool operator==(const EntityRef& other) const noexcept = default;
    constexpr bool operator!=(const EntityRef& other) const noexcept = default;
    constexpr bool operator<(const EntityRef& other) const noexcept {
        if (namespace_id != other.namespace_id) {
            return namespace_id < other.namespace_id;
        }
        return entity_id < other.entity_id;
    }
};

// =============================================================================
// LayerId
// =============================================================================

/// Layer identifier
struct LayerId {
    std::uint32_t value = UINT32_MAX;

    constexpr LayerId() noexcept = default;
    constexpr explicit LayerId(std::uint32_t v) noexcept : value(v) {}

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value != UINT32_MAX;
    }

    [[nodiscard]] static constexpr LayerId invalid() noexcept {
        return LayerId{};
    }

    constexpr bool operator==(const LayerId& other) const noexcept = default;
    constexpr bool operator<(const LayerId& other) const noexcept {
        return value < other.value;
    }
};

// =============================================================================
// AssetRef
// =============================================================================

/// Asset reference (path or UUID)
struct AssetRef {
    std::string path;
    std::uint64_t uuid = 0;

    /// Default constructor
    AssetRef() = default;

    /// Construct from path
    explicit AssetRef(std::string p) : path(std::move(p)) {}

    /// Construct from UUID
    explicit AssetRef(std::uint64_t id) : uuid(id) {}

    /// Check if valid
    [[nodiscard]] bool is_valid() const noexcept {
        return !path.empty() || uuid != 0;
    }

    /// Check if path-based
    [[nodiscard]] bool is_path() const noexcept {
        return !path.empty();
    }

    /// Check if UUID-based
    [[nodiscard]] bool is_uuid() const noexcept {
        return uuid != 0;
    }

    bool operator==(const AssetRef& other) const noexcept {
        if (is_uuid() && other.is_uuid()) {
            return uuid == other.uuid;
        }
        return path == other.path;
    }
};

// =============================================================================
// NamespacePermissions
// =============================================================================

/// Permissions for a namespace
struct NamespacePermissions {
    /// Can create entities
    bool can_create_entities = true;

    /// Can delete entities
    bool can_delete_entities = true;

    /// Can modify components
    bool can_modify_components = true;

    /// Can access other namespaces
    bool can_cross_namespace = false;

    /// Can create snapshots
    bool can_snapshot = true;

    /// Can modify layers
    bool can_modify_layers = true;

    /// Can modify hierarchy
    bool can_modify_hierarchy = true;

    /// Allowed component types (empty = all allowed)
    std::vector<std::string> allowed_components;

    /// Blocked component types
    std::vector<std::string> blocked_components;

    /// Create full permissions
    [[nodiscard]] static NamespacePermissions full() noexcept {
        return NamespacePermissions{};
    }

    /// Create read-only permissions
    [[nodiscard]] static NamespacePermissions read_only() noexcept {
        NamespacePermissions perms;
        perms.can_create_entities = false;
        perms.can_delete_entities = false;
        perms.can_modify_components = false;
        perms.can_modify_layers = false;
        perms.can_modify_hierarchy = false;
        return perms;
    }

    /// Check if component type is allowed
    [[nodiscard]] bool is_component_allowed(std::string_view component) const {
        // Check blocked first
        for (const auto& blocked : blocked_components) {
            if (blocked == component) {
                return false;
            }
        }

        // If allowed list is empty, all are allowed
        if (allowed_components.empty()) {
            return true;
        }

        // Check allowed list
        for (const auto& allowed : allowed_components) {
            if (allowed == component) {
                return true;
            }
        }

        return false;
    }
};

// =============================================================================
// ResourceLimits
// =============================================================================

/// Resource limits for a namespace
struct ResourceLimits {
    /// Maximum number of entities (0 = unlimited)
    std::size_t max_entities = 0;

    /// Maximum number of components per entity (0 = unlimited)
    std::size_t max_components_per_entity = 0;

    /// Maximum total memory usage in bytes (0 = unlimited)
    std::size_t max_memory_bytes = 0;

    /// Maximum pending transactions (0 = unlimited)
    std::size_t max_pending_transactions = 0;

    /// Maximum snapshots retained (0 = unlimited)
    std::size_t max_snapshots = 0;

    /// Create unlimited limits
    [[nodiscard]] static ResourceLimits unlimited() noexcept {
        return ResourceLimits{};
    }

    /// Create strict limits for sandboxed execution
    [[nodiscard]] static ResourceLimits sandboxed() noexcept {
        ResourceLimits limits;
        limits.max_entities = 10000;
        limits.max_components_per_entity = 32;
        limits.max_memory_bytes = 64 * 1024 * 1024;  // 64 MB
        limits.max_pending_transactions = 100;
        limits.max_snapshots = 10;
        return limits;
    }
};

// =============================================================================
// ResourceUsage
// =============================================================================

/// Current resource usage for a namespace
struct ResourceUsage {
    std::size_t entity_count = 0;
    std::size_t component_count = 0;
    std::size_t memory_bytes = 0;
    std::size_t pending_transactions = 0;
    std::size_t snapshot_count = 0;

    /// Check if within limits
    [[nodiscard]] bool within_limits(const ResourceLimits& limits) const noexcept {
        if (limits.max_entities > 0 && entity_count >= limits.max_entities) {
            return false;
        }
        if (limits.max_memory_bytes > 0 && memory_bytes >= limits.max_memory_bytes) {
            return false;
        }
        if (limits.max_pending_transactions > 0 &&
            pending_transactions >= limits.max_pending_transactions) {
            return false;
        }
        if (limits.max_snapshots > 0 && snapshot_count >= limits.max_snapshots) {
            return false;
        }
        return true;
    }
};

// =============================================================================
// Namespace
// =============================================================================

/// Namespace with isolation and permissions
class Namespace {
public:
    /// Construct with ID and name
    explicit Namespace(NamespaceId id, std::string name)
        : m_id(id)
        , m_name(std::move(name))
        , m_permissions(NamespacePermissions::full())
        , m_limits(ResourceLimits::unlimited()) {}

    /// Construct with all options
    Namespace(NamespaceId id, std::string name,
              NamespacePermissions permissions, ResourceLimits limits)
        : m_id(id)
        , m_name(std::move(name))
        , m_permissions(std::move(permissions))
        , m_limits(limits) {}

    /// Get namespace ID
    [[nodiscard]] NamespaceId id() const noexcept { return m_id; }

    /// Get namespace name
    [[nodiscard]] const std::string& name() const noexcept { return m_name; }

    /// Get permissions
    [[nodiscard]] const NamespacePermissions& permissions() const noexcept {
        return m_permissions;
    }

    /// Set permissions
    void set_permissions(NamespacePermissions perms) {
        m_permissions = std::move(perms);
    }

    /// Get resource limits
    [[nodiscard]] const ResourceLimits& limits() const noexcept {
        return m_limits;
    }

    /// Set resource limits
    void set_limits(ResourceLimits limits) noexcept {
        m_limits = limits;
    }

    /// Get current resource usage
    [[nodiscard]] const ResourceUsage& usage() const noexcept {
        return m_usage;
    }

    /// Update resource usage
    void update_usage(ResourceUsage usage) noexcept {
        m_usage = usage;
    }

    /// Check if within resource limits
    [[nodiscard]] bool within_limits() const noexcept {
        return m_usage.within_limits(m_limits);
    }

    /// Allocate new entity ID
    [[nodiscard]] std::uint64_t allocate_entity() {
        return m_next_entity_id++;
    }

    /// Get next entity ID (without allocating)
    [[nodiscard]] std::uint64_t peek_next_entity_id() const noexcept {
        return m_next_entity_id;
    }

private:
    NamespaceId m_id;
    std::string m_name;
    NamespacePermissions m_permissions;
    ResourceLimits m_limits;
    ResourceUsage m_usage;
    std::uint64_t m_next_entity_id = 1;
};

// =============================================================================
// NamespaceRegistry
// =============================================================================

/// Registry of namespaces
class NamespaceRegistry {
public:
    /// Create a new namespace
    [[nodiscard]] NamespaceId create(std::string name) {
        NamespaceId id(static_cast<std::uint32_t>(m_namespaces.size()));
        m_namespaces.emplace_back(id, std::move(name));
        m_name_to_id[m_namespaces.back().name()] = id;
        return id;
    }

    /// Create with custom permissions and limits
    [[nodiscard]] NamespaceId create(std::string name,
                                      NamespacePermissions permissions,
                                      ResourceLimits limits) {
        NamespaceId id(static_cast<std::uint32_t>(m_namespaces.size()));
        m_namespaces.emplace_back(id, std::move(name),
                                   std::move(permissions), limits);
        m_name_to_id[m_namespaces.back().name()] = id;
        return id;
    }

    /// Get namespace by ID
    [[nodiscard]] Namespace* get(NamespaceId id) {
        if (!id.is_valid() || id.value >= m_namespaces.size()) {
            return nullptr;
        }
        return &m_namespaces[id.value];
    }

    /// Get namespace by ID (const)
    [[nodiscard]] const Namespace* get(NamespaceId id) const {
        if (!id.is_valid() || id.value >= m_namespaces.size()) {
            return nullptr;
        }
        return &m_namespaces[id.value];
    }

    /// Find namespace by name
    [[nodiscard]] std::optional<NamespaceId> find_by_name(std::string_view name) const {
        auto it = m_name_to_id.find(std::string(name));
        if (it == m_name_to_id.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    /// Get total namespace count
    [[nodiscard]] std::size_t size() const noexcept {
        return m_namespaces.size();
    }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept {
        return m_namespaces.empty();
    }

    /// Clear all namespaces
    void clear() {
        m_namespaces.clear();
        m_name_to_id.clear();
    }

private:
    std::vector<Namespace> m_namespaces;
    std::unordered_map<std::string, NamespaceId> m_name_to_id;
};

} // namespace void_ir

// Hash specializations
template<>
struct std::hash<void_ir::NamespaceId> {
    std::size_t operator()(const void_ir::NamespaceId& id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};

template<>
struct std::hash<void_ir::EntityRef> {
    std::size_t operator()(const void_ir::EntityRef& ref) const noexcept {
        std::size_t h1 = std::hash<std::uint32_t>{}(ref.namespace_id.value);
        std::size_t h2 = std::hash<std::uint64_t>{}(ref.entity_id);
        return h1 ^ (h2 << 1);
    }
};

template<>
struct std::hash<void_ir::LayerId> {
    std::size_t operator()(const void_ir::LayerId& id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};
