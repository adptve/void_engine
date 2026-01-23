#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_ir module

#include <cstdint>

namespace void_ir {

// =============================================================================
// ID Types
// =============================================================================

/// Unique namespace identifier
struct NamespaceId;

/// Reference to an entity within a namespace
struct EntityRef;

/// Unique transaction identifier
struct TransactionId;

/// Unique snapshot identifier
struct SnapshotId {
    std::uint64_t value = UINT64_MAX;

    constexpr SnapshotId() noexcept = default;
    constexpr explicit SnapshotId(std::uint64_t v) noexcept : value(v) {}

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value != UINT64_MAX;
    }

    [[nodiscard]] static constexpr SnapshotId invalid() noexcept {
        return SnapshotId{};
    }

    constexpr bool operator==(const SnapshotId& other) const noexcept = default;
    constexpr bool operator<(const SnapshotId& other) const noexcept {
        return value < other.value;
    }
};

/// Layer identifier
struct LayerId;

/// Asset reference
struct AssetRef;

// =============================================================================
// Core Types
// =============================================================================

/// Namespace with isolation and permissions
class Namespace;

/// Namespace permissions
struct NamespacePermissions;

/// Resource limits for a namespace
struct ResourceLimits;

/// Namespace registry
class NamespaceRegistry;

// =============================================================================
// Value Types
// =============================================================================

/// Dynamic value type
class Value;

/// Value type discriminator
enum class ValueType : std::uint8_t;

// =============================================================================
// Patch Types
// =============================================================================

/// Patch kind discriminator
enum class PatchKind : std::uint8_t;

/// Base patch structure
struct Patch;

/// Entity creation/deletion patch
struct EntityPatch;

/// Component modification patch
struct ComponentPatch;

/// Layer modification patch
struct LayerPatch;

/// Asset reference patch
struct AssetPatch;

/// Hierarchy modification patch
struct HierarchyPatch;

/// Camera modification patch
struct CameraPatch;

/// Transform modification patch
struct TransformPatch;

/// Patch batch for optimization
class PatchBatch;

// =============================================================================
// Transaction Types
// =============================================================================

/// Transaction for atomic operations
class Transaction;

/// Transaction builder (fluent API)
class TransactionBuilder;

/// Transaction state
enum class TransactionState : std::uint8_t;

// =============================================================================
// Bus Types
// =============================================================================

/// Inter-thread patch bus
class PatchBus;

/// Subscription handle
struct SubscriptionId;

// =============================================================================
// Validation Types
// =============================================================================

/// Component schema for validation
class ComponentSchema;

/// Field descriptor
struct FieldDescriptor;

/// Field type discriminator
enum class FieldType : std::uint8_t;

/// Validation result
struct ValidationResult;

/// Validation error
struct ValidationError;

/// Schema registry
class SchemaRegistry;

// =============================================================================
// Snapshot Types
// =============================================================================

/// Namespace snapshot
class Snapshot;

/// Snapshot delta
class SnapshotDelta;

/// Snapshot manager
class SnapshotManager;

// =============================================================================
// Batch Optimization Types
// =============================================================================

/// Batch optimizer
class BatchOptimizer;

/// Optimization statistics
struct OptimizationStats;

} // namespace void_ir
