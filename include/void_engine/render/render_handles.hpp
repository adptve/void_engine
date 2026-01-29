/// @file render_handles.hpp
/// @brief Lightweight asset handle types for render components
///
/// These handle types are POD-compatible for ECS archetype storage while
/// integrating with the void_asset system for full hot-reload support.
/// Handles use generation tracking to detect stale references after hot-reload.
///
/// Design Principles:
/// - POD types suitable for ECS archetype storage
/// - Generation-based invalidation for hot-reload compatibility
/// - Integration with void_asset::AssetId for unified asset management
/// - Zero-overhead validation at access time

#pragma once

#include <void_engine/asset/types.hpp>
#include <cstdint>

namespace void_render {

// =============================================================================
// Handle Base Template
// =============================================================================

/// @brief Base handle type for render assets
///
/// Lightweight POD handle containing:
/// - id: Unique identifier within the asset type
/// - generation: Incremented on hot-reload to invalidate stale references
///
/// @tparam Tag Empty tag struct for type safety between different handle types
template<typename Tag>
struct RenderHandle {
    std::uint64_t id = 0;
    std::uint32_t generation = 0;

    /// Check if handle is valid (non-zero id)
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return id != 0;
    }

    /// Create invalid handle
    [[nodiscard]] static constexpr RenderHandle invalid() noexcept {
        return RenderHandle{0, 0};
    }

    /// Create from void_asset::AssetId
    [[nodiscard]] static constexpr RenderHandle from_asset_id(
        void_asset::AssetId asset_id,
        std::uint32_t gen = 1) noexcept {
        return RenderHandle{asset_id.raw(), gen};
    }

    /// Convert to void_asset::AssetId
    [[nodiscard]] constexpr void_asset::AssetId to_asset_id() const noexcept {
        return void_asset::AssetId{id};
    }

    /// Equality comparison
    [[nodiscard]] constexpr bool operator==(const RenderHandle& other) const noexcept {
        return id == other.id && generation == other.generation;
    }

    /// Inequality comparison
    [[nodiscard]] constexpr bool operator!=(const RenderHandle& other) const noexcept {
        return !(*this == other);
    }

    /// Less-than for ordered containers
    [[nodiscard]] constexpr bool operator<(const RenderHandle& other) const noexcept {
        if (id != other.id) return id < other.id;
        return generation < other.generation;
    }

    /// Check if this handle is stale (generation mismatch)
    [[nodiscard]] constexpr bool is_stale(std::uint32_t current_generation) const noexcept {
        return generation != current_generation;
    }
};

// =============================================================================
// Handle Type Tags
// =============================================================================

namespace handle_tags {
    struct Model {};
    struct Mesh {};
    struct Material {};
    struct Texture {};
    struct Shader {};
}

// =============================================================================
// Concrete Handle Types
// =============================================================================

/// @brief Handle to a loaded 3D model (glTF, GLB, etc.)
/// Models contain meshes, materials, textures, and scene hierarchy
using ModelHandle = RenderHandle<handle_tags::Model>;

/// @brief Handle to a GPU mesh (VAO/VBO/EBO)
/// Can reference built-in meshes or meshes from loaded models
using AssetMeshHandle = RenderHandle<handle_tags::Mesh>;

/// @brief Handle to a material asset
/// Materials define PBR properties and texture references
using AssetMaterialHandle = RenderHandle<handle_tags::Material>;

/// @brief Handle to a GPU texture
/// Textures are uploaded to GPU with mipmaps and proper formats
using AssetTextureHandle = RenderHandle<handle_tags::Texture>;

/// @brief Handle to a compiled shader program
/// Shaders support hot-reload with automatic recompilation
using AssetShaderHandle = RenderHandle<handle_tags::Shader>;

// =============================================================================
// Handle Creation Utilities
// =============================================================================

/// @brief Create a model handle from raw ID
[[nodiscard]] inline constexpr ModelHandle make_model_handle(
    std::uint64_t id,
    std::uint32_t generation = 1) noexcept {
    return ModelHandle{id, generation};
}

/// @brief Create a mesh handle from raw ID
[[nodiscard]] inline constexpr AssetMeshHandle make_mesh_handle(
    std::uint64_t id,
    std::uint32_t generation = 1) noexcept {
    return AssetMeshHandle{id, generation};
}

/// @brief Create a material handle from raw ID
[[nodiscard]] inline constexpr AssetMaterialHandle make_material_handle(
    std::uint64_t id,
    std::uint32_t generation = 1) noexcept {
    return AssetMaterialHandle{id, generation};
}

/// @brief Create a texture handle from raw ID
[[nodiscard]] inline constexpr AssetTextureHandle make_texture_handle(
    std::uint64_t id,
    std::uint32_t generation = 1) noexcept {
    return AssetTextureHandle{id, generation};
}

/// @brief Create a shader handle from raw ID
[[nodiscard]] inline constexpr AssetShaderHandle make_shader_handle(
    std::uint64_t id,
    std::uint32_t generation = 1) noexcept {
    return AssetShaderHandle{id, generation};
}

// =============================================================================
// Handle Validation
// =============================================================================

/// @brief Check if a handle matches current asset generation
/// @param handle The handle to validate
/// @param current_gen The current generation of the asset
/// @return true if handle is valid and generation matches
template<typename Tag>
[[nodiscard]] inline constexpr bool validate_handle(
    const RenderHandle<Tag>& handle,
    std::uint32_t current_gen) noexcept {
    return handle.is_valid() && handle.generation == current_gen;
}

/// @brief Upgrade a stale handle to current generation
/// @param handle The handle to upgrade
/// @param new_gen The new generation value
/// @return Upgraded handle (or invalid if original was invalid)
template<typename Tag>
[[nodiscard]] inline constexpr RenderHandle<Tag> upgrade_handle(
    const RenderHandle<Tag>& handle,
    std::uint32_t new_gen) noexcept {
    if (!handle.is_valid()) return handle;
    return RenderHandle<Tag>{handle.id, new_gen};
}

} // namespace void_render

// =============================================================================
// Hash Specializations for Standard Containers
// =============================================================================

template<>
struct std::hash<void_render::ModelHandle> {
    std::size_t operator()(const void_render::ModelHandle& h) const noexcept {
        return std::hash<std::uint64_t>{}(h.id) ^ (std::hash<std::uint32_t>{}(h.generation) << 1);
    }
};

template<>
struct std::hash<void_render::AssetMeshHandle> {
    std::size_t operator()(const void_render::AssetMeshHandle& h) const noexcept {
        return std::hash<std::uint64_t>{}(h.id) ^ (std::hash<std::uint32_t>{}(h.generation) << 1);
    }
};

template<>
struct std::hash<void_render::AssetMaterialHandle> {
    std::size_t operator()(const void_render::AssetMaterialHandle& h) const noexcept {
        return std::hash<std::uint64_t>{}(h.id) ^ (std::hash<std::uint32_t>{}(h.generation) << 1);
    }
};

template<>
struct std::hash<void_render::AssetTextureHandle> {
    std::size_t operator()(const void_render::AssetTextureHandle& h) const noexcept {
        return std::hash<std::uint64_t>{}(h.id) ^ (std::hash<std::uint32_t>{}(h.generation) << 1);
    }
};

template<>
struct std::hash<void_render::AssetShaderHandle> {
    std::size_t operator()(const void_render::AssetShaderHandle& h) const noexcept {
        return std::hash<std::uint64_t>{}(h.id) ^ (std::hash<std::uint32_t>{}(h.generation) << 1);
    }
};
