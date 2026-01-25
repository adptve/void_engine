#pragma once

/// @file id.hpp
/// @brief ID types and generators for void_core

#include "fwd.hpp"
#include "error.hpp"
#include <cstdint>
#include <string>
#include <atomic>
#include <functional>
#include <compare>
#include <ostream>
#include <vector>

namespace void_core {

// =============================================================================
// FNV-1a Hash (for string-based IDs)
// =============================================================================

namespace detail {

/// FNV-1a hash constants
constexpr std::uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325ULL;
constexpr std::uint64_t FNV_PRIME = 0x100000001b3ULL;

/// Compute FNV-1a hash of string
[[nodiscard]] constexpr std::uint64_t fnv1a_hash(const char* str, std::size_t len) noexcept {
    std::uint64_t hash = FNV_OFFSET_BASIS;
    for (std::size_t i = 0; i < len; ++i) {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(str[i]));
        hash *= FNV_PRIME;
    }
    return hash;
}

[[nodiscard]] inline std::uint64_t fnv1a_hash(const std::string& str) noexcept {
    return fnv1a_hash(str.data(), str.size());
}

} // namespace detail

// =============================================================================
// Id
// =============================================================================

/// Generational index identifier (64-bit)
/// Layout: [Generation(32 bits) | Index(32 bits)]
struct Id {
    std::uint64_t bits = UINT64_MAX;  // Null by default

    /// Default constructor (null ID)
    constexpr Id() noexcept = default;

    /// Construct from raw bits
    constexpr explicit Id(std::uint64_t raw) noexcept : bits(raw) {}

    /// Construct from index and generation
    [[nodiscard]] static constexpr Id create(std::uint32_t index, std::uint32_t generation) noexcept {
        return Id((static_cast<std::uint64_t>(generation) << 32) | static_cast<std::uint64_t>(index));
    }

    /// Create null ID
    [[nodiscard]] static constexpr Id null() noexcept {
        return Id{UINT64_MAX};
    }

    /// Create ID from name (using FNV-1a hash)
    [[nodiscard]] static Id from_name(const std::string& name) noexcept {
        return Id(detail::fnv1a_hash(name));
    }

    /// Check if null
    [[nodiscard]] constexpr bool is_null() const noexcept {
        return bits == UINT64_MAX;
    }

    /// Check if valid (not null)
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return bits != UINT64_MAX;
    }

    /// Get index component
    [[nodiscard]] constexpr std::uint32_t index() const noexcept {
        return static_cast<std::uint32_t>(bits & 0xFFFFFFFF);
    }

    /// Get generation component
    [[nodiscard]] constexpr std::uint32_t generation() const noexcept {
        return static_cast<std::uint32_t>(bits >> 32);
    }

    /// Get raw bits
    [[nodiscard]] constexpr std::uint64_t to_bits() const noexcept {
        return bits;
    }

    /// Create from raw bits
    [[nodiscard]] static constexpr Id from_bits(std::uint64_t raw) noexcept {
        return Id(raw);
    }

    /// Comparison operators
    constexpr auto operator<=>(const Id&) const noexcept = default;
    constexpr bool operator==(const Id&) const noexcept = default;

    /// Explicit bool conversion
    explicit constexpr operator bool() const noexcept {
        return is_valid();
    }
};

// =============================================================================
// IdGenerator
// =============================================================================

/// Thread-safe ID generator
class IdGenerator {
public:
    /// Constructor
    IdGenerator() noexcept : m_next(0) {}

    /// Generate next ID (thread-safe)
    [[nodiscard]] Id next() noexcept {
        std::uint64_t index = m_next.fetch_add(1, std::memory_order_relaxed);
        return Id::create(static_cast<std::uint32_t>(index), 0);
    }

    /// Generate batch of IDs
    /// Returns starting ID; subsequent IDs are [start, start + count)
    [[nodiscard]] Id next_batch(std::uint32_t count) noexcept {
        std::uint64_t start = m_next.fetch_add(count, std::memory_order_relaxed);
        return Id::create(static_cast<std::uint32_t>(start), 0);
    }

    /// Get current count (approximate, for debugging)
    [[nodiscard]] std::uint64_t current() const noexcept {
        return m_next.load(std::memory_order_relaxed);
    }

    /// Reset generator (NOT thread-safe, use only during initialization)
    void reset() noexcept {
        m_next.store(0, std::memory_order_relaxed);
    }

private:
    std::atomic<std::uint64_t> m_next;
};

// =============================================================================
// NamedId
// =============================================================================

/// String-based identifier with precomputed hash
struct NamedId {
    std::string name;
    std::uint64_t hash = 0;

    /// Default constructor
    NamedId() = default;

    /// Construct from string
    explicit NamedId(const std::string& n)
        : name(n), hash(detail::fnv1a_hash(n)) {}

    explicit NamedId(std::string&& n)
        : name(std::move(n)), hash(detail::fnv1a_hash(name)) {}

    /// Construct from C-string
    explicit NamedId(const char* n)
        : name(n), hash(detail::fnv1a_hash(name)) {}

    /// Get name
    [[nodiscard]] const std::string& get_name() const noexcept { return name; }

    /// Get hash
    [[nodiscard]] std::uint64_t hash_value() const noexcept { return hash; }

    /// Convert to Id
    [[nodiscard]] Id to_id() const noexcept {
        return Id(hash);
    }

    /// Comparison (by hash for speed, then by name for collision handling)
    bool operator==(const NamedId& other) const noexcept {
        return hash == other.hash && name == other.name;
    }

    bool operator!=(const NamedId& other) const noexcept {
        return !(*this == other);
    }

    bool operator<(const NamedId& other) const noexcept {
        if (hash != other.hash) return hash < other.hash;
        return name < other.name;
    }

    /// Explicit bool conversion
    explicit operator bool() const noexcept {
        return !name.empty();
    }
};

/// Output stream operators
inline std::ostream& operator<<(std::ostream& os, const Id& id) {
    if (id.is_null()) {
        return os << "Id(null)";
    }
    return os << "Id(" << id.index() << "v" << id.generation() << ")";
}

inline std::ostream& operator<<(std::ostream& os, const NamedId& id) {
    return os << "NamedId(\"" << id.name << "\")";
}

// =============================================================================
// Global ID Generators (Implemented in id.cpp)
// =============================================================================

/// Get the global entity ID generator
IdGenerator& entity_id_generator();

/// Get the global resource ID generator
IdGenerator& resource_id_generator();

/// Get the global component ID generator
IdGenerator& component_id_generator();

/// Get the global system ID generator
IdGenerator& system_id_generator();

/// Generate a new entity ID
Id next_entity_id();

/// Generate a new resource ID
Id next_resource_id();

/// Generate a new component ID
Id next_component_id();

/// Generate a new system ID
Id next_system_id();

/// Reset all global ID generators (DANGEROUS - only for testing/shutdown)
void reset_all_id_generators();

// =============================================================================
// ID Serialization (Implemented in id.cpp)
// =============================================================================

namespace serialization {

/// Serialize an Id to binary
std::vector<std::uint8_t> serialize_id(Id id);

/// Deserialize an Id from binary
Result<Id> deserialize_id(const std::vector<std::uint8_t>& data);

/// Serialize a NamedId to binary
std::vector<std::uint8_t> serialize_named_id(const NamedId& id);

/// Deserialize a NamedId from binary
Result<NamedId> deserialize_named_id(const std::vector<std::uint8_t>& data);

} // namespace serialization

// =============================================================================
// Debug Utilities (Implemented in id.cpp)
// =============================================================================

namespace debug {

/// Format an ID for debugging
std::string format_id(Id id);

/// Format a NamedId for debugging
std::string format_named_id(const NamedId& id);

/// Get generator statistics as formatted string
std::string format_generator_stats();

} // namespace debug

// =============================================================================
// Hash Verification (Implemented in id.cpp)
// =============================================================================

namespace hash {

/// Verify FNV-1a hash implementation against known test vectors
bool verify_fnv1a_implementation();

} // namespace hash

} // namespace void_core

/// Hash specializations
template<>
struct std::hash<void_core::Id> {
    std::size_t operator()(const void_core::Id& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.bits);
    }
};

template<>
struct std::hash<void_core::NamedId> {
    std::size_t operator()(const void_core::NamedId& id) const noexcept {
        return static_cast<std::size_t>(id.hash);
    }
};
