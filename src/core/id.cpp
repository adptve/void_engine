/// @file id.cpp
/// @brief ID system implementation for void_core
///
/// The ID system is primarily constexpr/inline header-only.
/// This file provides:
/// - Global ID generator instances
/// - ID debugging and validation utilities
/// - ID serialization support

#include <void_engine/core/id.hpp>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace void_core {

// =============================================================================
// Global ID Generators
// =============================================================================

namespace {

/// Global entity ID generator
IdGenerator s_entity_id_generator;

/// Global resource ID generator
IdGenerator s_resource_id_generator;

/// Global component ID generator
IdGenerator s_component_id_generator;

/// Global system ID generator
IdGenerator s_system_id_generator;

/// Mutex for generator access (for reset operations)
std::mutex s_generator_mutex;

} // anonymous namespace

/// Get the global entity ID generator
IdGenerator& entity_id_generator() {
    return s_entity_id_generator;
}

/// Get the global resource ID generator
IdGenerator& resource_id_generator() {
    return s_resource_id_generator;
}

/// Get the global component ID generator
IdGenerator& component_id_generator() {
    return s_component_id_generator;
}

/// Get the global system ID generator
IdGenerator& system_id_generator() {
    return s_system_id_generator;
}

/// Generate a new entity ID
Id next_entity_id() {
    return s_entity_id_generator.next();
}

/// Generate a new resource ID
Id next_resource_id() {
    return s_resource_id_generator.next();
}

/// Generate a new component ID
Id next_component_id() {
    return s_component_id_generator.next();
}

/// Generate a new system ID
Id next_system_id() {
    return s_system_id_generator.next();
}

/// Reset all global ID generators (DANGEROUS - only for testing/shutdown)
void reset_all_id_generators() {
    std::lock_guard<std::mutex> lock(s_generator_mutex);
    s_entity_id_generator.reset();
    s_resource_id_generator.reset();
    s_component_id_generator.reset();
    s_system_id_generator.reset();
}

// =============================================================================
// ID Debugging Utilities
// =============================================================================

namespace debug {

/// Format an ID for debugging
std::string format_id(Id id) {
    if (id.is_null()) {
        return "Id(null)";
    }

    std::ostringstream oss;
    oss << "Id(idx=" << id.index()
        << ", gen=" << id.generation()
        << ", bits=0x" << std::hex << std::setfill('0') << std::setw(16) << id.bits
        << ")";
    return oss.str();
}

/// Format a NamedId for debugging
std::string format_named_id(const NamedId& id) {
    std::ostringstream oss;
    oss << "NamedId(\"" << id.name
        << "\", hash=0x" << std::hex << std::setfill('0') << std::setw(16) << id.hash
        << ")";
    return oss.str();
}

/// Validate ID consistency
struct IdValidation {
    bool is_valid = false;
    bool not_null = false;
    bool index_reasonable = false;
    std::string error_message;
};

/// Validate an ID
IdValidation validate_id(Id id) {
    IdValidation result;

    if (id.is_null()) {
        result.error_message = "ID is null";
        return result;
    }
    result.not_null = true;

    // Check if index is reasonable (not suspiciously large)
    constexpr std::uint32_t MAX_REASONABLE_INDEX = 0x00FFFFFF;  // 16 million
    if (id.index() > MAX_REASONABLE_INDEX) {
        result.error_message = "ID index suspiciously large: " + std::to_string(id.index());
        return result;
    }
    result.index_reasonable = true;
    result.is_valid = true;

    return result;
}

/// Get generator statistics
struct GeneratorStats {
    std::uint64_t entity_count;
    std::uint64_t resource_count;
    std::uint64_t component_count;
    std::uint64_t system_count;
};

GeneratorStats get_generator_stats() {
    return GeneratorStats{
        s_entity_id_generator.current(),
        s_resource_id_generator.current(),
        s_component_id_generator.current(),
        s_system_id_generator.current()
    };
}

/// Format generator statistics
std::string format_generator_stats() {
    auto stats = get_generator_stats();
    std::ostringstream oss;
    oss << "ID Generator Statistics:\n"
        << "  Entities: " << stats.entity_count << "\n"
        << "  Resources: " << stats.resource_count << "\n"
        << "  Components: " << stats.component_count << "\n"
        << "  Systems: " << stats.system_count << "\n";
    return oss.str();
}

} // namespace debug

// =============================================================================
// ID Serialization Support
// =============================================================================

namespace serialization {

/// Binary serialization constants for IDs
namespace id_binary {
    constexpr std::uint32_t MAGIC_ID = 0x564F4944;       // "VOID"
    constexpr std::uint32_t MAGIC_NAMED_ID = 0x4E414D45; // "NAME"
    constexpr std::uint32_t VERSION = 1;
}

/// Serialize an Id to binary
std::vector<std::uint8_t> serialize_id(Id id) {
    std::vector<std::uint8_t> data(sizeof(std::uint32_t) * 2 + sizeof(std::uint64_t));

    auto* ptr = data.data();

    // Magic
    std::memcpy(ptr, &id_binary::MAGIC_ID, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    // Version
    std::memcpy(ptr, &id_binary::VERSION, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    // ID bits
    std::uint64_t bits = id.to_bits();
    std::memcpy(ptr, &bits, sizeof(std::uint64_t));

    return data;
}

/// Deserialize an Id from binary
Result<Id> deserialize_id(const std::vector<std::uint8_t>& data) {
    if (data.size() < sizeof(std::uint32_t) * 2 + sizeof(std::uint64_t)) {
        return Err<Id>(Error(ErrorCode::ParseError, "ID data too short"));
    }

    const auto* ptr = data.data();

    // Verify magic
    std::uint32_t magic;
    std::memcpy(&magic, ptr, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    if (magic != id_binary::MAGIC_ID) {
        return Err<Id>(Error(ErrorCode::ParseError, "Invalid ID magic"));
    }

    // Verify version
    std::uint32_t version;
    std::memcpy(&version, ptr, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    if (version != id_binary::VERSION) {
        return Err<Id>(Error(ErrorCode::IncompatibleVersion, "Unsupported ID version"));
    }

    // Read ID bits
    std::uint64_t bits;
    std::memcpy(&bits, ptr, sizeof(std::uint64_t));

    return Ok(Id::from_bits(bits));
}

/// Serialize a NamedId to binary
std::vector<std::uint8_t> serialize_named_id(const NamedId& id) {
    // Calculate size: magic + version + hash + name_length + name
    std::size_t name_len = id.name.size();
    std::vector<std::uint8_t> data(
        sizeof(std::uint32_t) * 3 + sizeof(std::uint64_t) + name_len);

    auto* ptr = data.data();

    // Magic
    std::memcpy(ptr, &id_binary::MAGIC_NAMED_ID, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    // Version
    std::memcpy(ptr, &id_binary::VERSION, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    // Hash
    std::memcpy(ptr, &id.hash, sizeof(std::uint64_t));
    ptr += sizeof(std::uint64_t);

    // Name length
    std::uint32_t len = static_cast<std::uint32_t>(name_len);
    std::memcpy(ptr, &len, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    // Name data
    std::memcpy(ptr, id.name.data(), name_len);

    return data;
}

/// Deserialize a NamedId from binary
Result<NamedId> deserialize_named_id(const std::vector<std::uint8_t>& data) {
    constexpr std::size_t MIN_SIZE = sizeof(std::uint32_t) * 3 + sizeof(std::uint64_t);
    if (data.size() < MIN_SIZE) {
        return Err<NamedId>(Error(ErrorCode::ParseError, "NamedId data too short"));
    }

    const auto* ptr = data.data();

    // Verify magic
    std::uint32_t magic;
    std::memcpy(&magic, ptr, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    if (magic != id_binary::MAGIC_NAMED_ID) {
        return Err<NamedId>(Error(ErrorCode::ParseError, "Invalid NamedId magic"));
    }

    // Verify version
    std::uint32_t version;
    std::memcpy(&version, ptr, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    if (version != id_binary::VERSION) {
        return Err<NamedId>(Error(ErrorCode::IncompatibleVersion, "Unsupported NamedId version"));
    }

    // Read hash
    std::uint64_t hash;
    std::memcpy(&hash, ptr, sizeof(std::uint64_t));
    ptr += sizeof(std::uint64_t);

    // Read name length
    std::uint32_t name_len;
    std::memcpy(&name_len, ptr, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    // Validate remaining data
    if (data.size() < MIN_SIZE + name_len) {
        return Err<NamedId>(Error(ErrorCode::ParseError, "NamedId data truncated"));
    }

    // Read name
    std::string name(reinterpret_cast<const char*>(ptr), name_len);

    // Verify hash matches
    NamedId result(std::move(name));
    if (result.hash != hash) {
        return Err<NamedId>(Error(ErrorCode::ValidationError, "NamedId hash mismatch"));
    }

    return Ok(std::move(result));
}

} // namespace serialization

// =============================================================================
// FNV-1a Hash Verification
// =============================================================================

namespace hash {

/// Verify FNV-1a hash implementation against known test vectors
bool verify_fnv1a_implementation() {
    // Test vectors from FNV specification
    struct TestCase {
        const char* input;
        std::uint64_t expected;
    };

    // These are FNV-1a 64-bit test vectors
    constexpr TestCase tests[] = {
        {"", 0xcbf29ce484222325ULL},
        {"a", 0xaf63dc4c8601ec8cULL},
        {"foobar", 0x85944171f73967e8ULL},
    };

    for (const auto& test : tests) {
        std::uint64_t computed = detail::fnv1a_hash(test.input, std::strlen(test.input));
        if (computed != test.expected) {
            return false;
        }
    }

    return true;
}

/// Self-test on module load (debug builds only)
#ifndef NDEBUG
namespace {
    struct FnvVerifier {
        FnvVerifier() {
            if (!verify_fnv1a_implementation()) {
                // In debug builds, assert on hash implementation errors
                assert(false && "FNV-1a hash implementation verification failed");
            }
        }
    };
    static FnvVerifier s_fnv_verifier;
}
#endif

} // namespace hash

} // namespace void_core
