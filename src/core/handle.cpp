/// @file handle.cpp
/// @brief Handle system implementation for void_core
///
/// The handle system is template-based and primarily header-only.
/// This file provides:
/// - Compilation verification for handle types
/// - Explicit template instantiations for common handle types
/// - Handle debugging and validation utilities

#include <void_engine/core/handle.hpp>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace void_core {

// =============================================================================
// Handle Debugging Utilities
// =============================================================================

namespace debug {

/// Format a raw handle value for debugging
std::string format_handle_bits(std::uint32_t bits) {
    if (bits == handle_constants::NULL_BITS) {
        return "Handle(null)";
    }

    std::uint32_t index = bits & handle_constants::MAX_INDEX;
    std::uint8_t generation = static_cast<std::uint8_t>(bits >> 24);

    std::ostringstream oss;
    oss << "Handle(idx=" << index << ", gen=" << static_cast<int>(generation)
        << ", bits=0x" << std::hex << std::setfill('0') << std::setw(8) << bits << ")";
    return oss.str();
}

/// Validate a handle against allocator state
template<typename T>
HandleValidation validate_handle(
    Handle<T> handle,
    const HandleAllocator<T>& allocator)
{
    HandleValidation result;

    if (handle.is_null()) {
        result.error_message = "Handle is null";
        return result;
    }

    std::uint32_t index = handle.index();
    if (index >= allocator.capacity()) {
        result.error_message = "Handle index out of bounds: " + std::to_string(index)
            + " >= " + std::to_string(allocator.capacity());
        return result;
    }
    result.index_in_range = true;

    std::uint8_t expected_gen = allocator.generation_at(index);
    std::uint8_t handle_gen = handle.generation();
    if (expected_gen != handle_gen) {
        result.error_message = "Generation mismatch: handle has "
            + std::to_string(static_cast<int>(handle_gen))
            + ", allocator has " + std::to_string(static_cast<int>(expected_gen));
        return result;
    }
    result.generation_matches = true;
    result.is_valid = true;

    return result;
}

} // namespace debug

// =============================================================================
// Handle Pool Statistics
// =============================================================================

/// Compute statistics for a handle allocator
template<typename T>
HandlePoolStats compute_pool_stats(const HandleAllocator<T>& allocator) {
    HandlePoolStats stats;
    stats.total_allocated = allocator.capacity();
    stats.active_count = allocator.len();
    stats.free_count = allocator.free_count();

    if (stats.total_allocated > 0) {
        stats.fragmentation_ratio = static_cast<float>(stats.free_count)
            / static_cast<float>(stats.total_allocated);
    }

    return stats;
}

// =============================================================================
// Common Handle Type Definitions
// =============================================================================

// Forward declarations for common engine types that use handles
struct Entity;
struct Component;
struct Resource;
struct Texture;
struct Mesh;
struct Material;
struct Shader;
struct Buffer;
struct RenderTarget;

// =============================================================================
// Explicit Template Instantiations
// =============================================================================

// Handle<T> - Struct, no methods to instantiate beyond header

// HandleAllocator<T> for common types
template class HandleAllocator<void>;  // Generic handle allocator

// HandleMap<T> for common types
template class HandleMap<int>;
template class HandleMap<float>;
template class HandleMap<std::string>;
template class HandleMap<std::vector<std::uint8_t>>;

// Debug utilities
template debug::HandleValidation debug::validate_handle<void>(
    Handle<void>, const HandleAllocator<void>&);

template HandlePoolStats compute_pool_stats<void>(const HandleAllocator<void>&);

// =============================================================================
// Handle Serialization Support
// =============================================================================

namespace serialization {

/// Binary serialization constants for handles
namespace handle_binary {
    constexpr std::uint32_t MAGIC = 0x484E444C;  // "HNDL"
    constexpr std::uint32_t VERSION = 1;
}

/// Serialize a handle to binary
template<typename T>
std::vector<std::uint8_t> serialize_handle(Handle<T> handle) {
    std::vector<std::uint8_t> data(sizeof(std::uint32_t) * 3);

    auto* ptr = data.data();

    // Magic
    std::memcpy(ptr, &handle_binary::MAGIC, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    // Version
    std::memcpy(ptr, &handle_binary::VERSION, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    // Handle bits
    std::uint32_t bits = handle.to_bits();
    std::memcpy(ptr, &bits, sizeof(std::uint32_t));

    return data;
}

/// Deserialize a handle from binary
template<typename T>
Result<Handle<T>> deserialize_handle(const std::vector<std::uint8_t>& data) {
    if (data.size() < sizeof(std::uint32_t) * 3) {
        return Err<Handle<T>>(Error(ErrorCode::ParseError, "Handle data too short"));
    }

    const auto* ptr = data.data();

    // Verify magic
    std::uint32_t magic;
    std::memcpy(&magic, ptr, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    if (magic != handle_binary::MAGIC) {
        return Err<Handle<T>>(Error(ErrorCode::ParseError, "Invalid handle magic"));
    }

    // Verify version
    std::uint32_t version;
    std::memcpy(&version, ptr, sizeof(std::uint32_t));
    ptr += sizeof(std::uint32_t);

    if (version != handle_binary::VERSION) {
        return Err<Handle<T>>(Error(ErrorCode::IncompatibleVersion, "Unsupported handle version"));
    }

    // Read handle bits
    std::uint32_t bits;
    std::memcpy(&bits, ptr, sizeof(std::uint32_t));

    return Ok(Handle<T>::from_bits(bits));
}

// Explicit instantiations for serialization
template std::vector<std::uint8_t> serialize_handle<void>(Handle<void>);
template Result<Handle<void>> deserialize_handle<void>(const std::vector<std::uint8_t>&);

} // namespace serialization

// =============================================================================
// Handle Allocator Compaction
// =============================================================================

/// Compact a HandleMap by removing gaps in storage
/// NOTE: This invalidates all handles! Only use during controlled shutdown/reload.
template<typename T>
CompactionResult compact_handle_map(HandleMap<T>& map) {
    CompactionResult result;

    // HandleMap doesn't support compaction without invalidating handles
    // This is intentional - handles must remain stable
    // Compaction would require a remapping table

    result.success = false;
    return result;
}

// Explicit instantiation
template CompactionResult compact_handle_map<void>(HandleMap<void>&);

} // namespace void_core
