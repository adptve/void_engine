#pragma once

/// @file material.hpp
/// @brief PBR material system for void_render

#include "fwd.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <unordered_map>
#include <optional>

namespace void_render {

// =============================================================================
// Material Limits
// =============================================================================

/// Maximum materials per buffer
static constexpr std::size_t MAX_MATERIALS = 256;

// =============================================================================
// MaterialId
// =============================================================================

/// Material identifier
struct MaterialId {
    std::uint32_t index = UINT32_MAX;

    constexpr MaterialId() noexcept = default;
    constexpr explicit MaterialId(std::uint32_t idx) noexcept : index(idx) {}

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return index != UINT32_MAX;
    }

    [[nodiscard]] static constexpr MaterialId invalid() noexcept {
        return MaterialId{};
    }

    constexpr bool operator==(const MaterialId& other) const noexcept = default;
};

// =============================================================================
// GpuMaterial (GPU-ready, comprehensive PBR)
// =============================================================================

/// GPU material data (256 bytes, aligned for uniform buffer)
/// Supports full PBR with extensions: clearcoat, transmission, subsurface, sheen, anisotropy, iridescence
struct alignas(16) GpuMaterial {
    // Core PBR (16 bytes)
    std::array<float, 4> base_color = {1, 1, 1, 1};  // RGBA

    // Metallic/Roughness/AO (16 bytes)
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;  // Ambient occlusion
    std::uint32_t flags = 0;  // Packed feature flags

    // Emissive (16 bytes)
    std::array<float, 3> emissive = {0, 0, 0};
    float alpha_cutoff = 0.5f;  // For masked blend mode

    // Clearcoat (16 bytes)
    float clearcoat = 0.0f;
    float clearcoat_roughness = 0.0f;
    std::array<float, 2> _pad0 = {0, 0};

    // Transmission (16 bytes)
    float transmission = 0.0f;  // For glass-like materials
    float ior = 1.5f;  // Index of refraction
    float thickness = 0.0f;
    float attenuation_distance = 0.0f;

    // Attenuation + Subsurface (16 bytes)
    std::array<float, 3> attenuation_color = {1, 1, 1};
    float subsurface = 0.0f;

    // Subsurface color + Sheen (16 bytes)
    std::array<float, 3> subsurface_color = {1, 1, 1};
    float sheen = 0.0f;

    // Sheen color + roughness (16 bytes)
    std::array<float, 3> sheen_color = {1, 1, 1};
    float sheen_roughness = 0.0f;

    // Anisotropy + Iridescence (16 bytes)
    float anisotropy = 0.0f;
    float anisotropy_rotation = 0.0f;
    float iridescence = 0.0f;
    float iridescence_ior = 1.3f;

    // Iridescence thickness (16 bytes)
    float iridescence_thickness_min = 100.0f;
    float iridescence_thickness_max = 400.0f;
    std::array<float, 2> _pad1 = {0, 0};

    // Subsurface radius RGB (16 bytes)
    std::array<float, 3> subsurface_radius = {1, 0.2f, 0.1f};
    float _pad2 = 0.0f;

    // Texture indices - first set (16 bytes)
    std::int32_t tex_base_color = -1;  // -1 = no texture
    std::int32_t tex_normal = -1;
    std::int32_t tex_metallic_roughness = -1;
    std::int32_t tex_emissive = -1;

    // Texture indices - second set (16 bytes)
    std::int32_t tex_occlusion = -1;
    std::int32_t tex_clearcoat = -1;
    std::int32_t tex_clearcoat_roughness = -1;
    std::int32_t tex_clearcoat_normal = -1;

    // Texture indices - third set (16 bytes)
    std::int32_t tex_transmission = -1;
    std::int32_t tex_thickness = -1;
    std::int32_t tex_sheen_color = -1;
    std::int32_t tex_sheen_roughness = -1;

    // Texture indices - fourth set (16 bytes)
    std::int32_t tex_anisotropy = -1;
    std::int32_t tex_iridescence = -1;
    std::int32_t tex_iridescence_thickness = -1;
    std::int32_t tex_subsurface = -1;

    // Padding to reach 256 bytes (16 bytes)
    std::array<float, 4> _pad3 = {0, 0, 0, 0};

    /// Size in bytes
    static constexpr std::size_t SIZE = 256;

    // -------------------------------------------------------------------------
    // Material flags
    // -------------------------------------------------------------------------

    /// Flag bits
    enum Flags : std::uint32_t {
        FLAG_DOUBLE_SIDED       = 1 << 0,
        FLAG_ALPHA_MASK         = 1 << 1,
        FLAG_ALPHA_BLEND        = 1 << 2,
        FLAG_UNLIT              = 1 << 3,
        FLAG_HAS_NORMAL_MAP     = 1 << 4,
        FLAG_HAS_CLEARCOAT      = 1 << 5,
        FLAG_HAS_TRANSMISSION   = 1 << 6,
        FLAG_HAS_SUBSURFACE     = 1 << 7,
        FLAG_HAS_SHEEN          = 1 << 8,
        FLAG_HAS_ANISOTROPY     = 1 << 9,
        FLAG_HAS_IRIDESCENCE    = 1 << 10,
        FLAG_RECEIVES_SHADOWS   = 1 << 11,
        FLAG_CASTS_SHADOWS      = 1 << 12,
    };

    /// Check if flag is set
    [[nodiscard]] bool has_flag(Flags flag) const noexcept {
        return (flags & flag) != 0;
    }

    /// Set flag
    void set_flag(Flags flag, bool value = true) {
        if (value) {
            flags |= flag;
        } else {
            flags &= ~flag;
        }
    }

    // -------------------------------------------------------------------------
    // Factory methods
    // -------------------------------------------------------------------------

    /// Create default PBR material
    [[nodiscard]] static GpuMaterial pbr_default() {
        GpuMaterial mat;
        mat.set_flag(FLAG_RECEIVES_SHADOWS);
        mat.set_flag(FLAG_CASTS_SHADOWS);
        return mat;
    }

    /// Create metallic material
    [[nodiscard]] static GpuMaterial make_metallic(std::array<float, 3> color, float rough = 0.3f) {
        GpuMaterial mat;
        mat.base_color = {color[0], color[1], color[2], 1.0f};
        mat.metallic = 1.0f;
        mat.roughness = rough;
        mat.set_flag(FLAG_RECEIVES_SHADOWS);
        mat.set_flag(FLAG_CASTS_SHADOWS);
        return mat;
    }

    /// Create dielectric (non-metallic) material
    [[nodiscard]] static GpuMaterial dielectric(std::array<float, 3> color, float rough = 0.5f) {
        GpuMaterial mat;
        mat.base_color = {color[0], color[1], color[2], 1.0f};
        mat.metallic = 0.0f;
        mat.roughness = rough;
        mat.set_flag(FLAG_RECEIVES_SHADOWS);
        mat.set_flag(FLAG_CASTS_SHADOWS);
        return mat;
    }

    /// Create emissive material
    [[nodiscard]] static GpuMaterial make_emissive(std::array<float, 3> emit_color, float intensity = 1.0f) {
        GpuMaterial mat;
        mat.base_color = {0, 0, 0, 1};
        mat.emissive = {emit_color[0] * intensity, emit_color[1] * intensity, emit_color[2] * intensity};
        mat.set_flag(FLAG_UNLIT);
        return mat;
    }

    /// Create glass-like transmissive material
    [[nodiscard]] static GpuMaterial glass(float refraction_index = 1.5f, std::array<float, 3> tint = {1, 1, 1}) {
        GpuMaterial mat;
        mat.base_color = {tint[0], tint[1], tint[2], 1.0f};
        mat.metallic = 0.0f;
        mat.roughness = 0.0f;
        mat.transmission = 1.0f;
        mat.ior = refraction_index;
        mat.set_flag(FLAG_HAS_TRANSMISSION);
        mat.set_flag(FLAG_ALPHA_BLEND);
        return mat;
    }

    /// Create unlit (emissive-like) material
    [[nodiscard]] static GpuMaterial unlit(std::array<float, 3> color) {
        GpuMaterial mat;
        mat.base_color = {color[0], color[1], color[2], 1.0f};
        mat.set_flag(FLAG_UNLIT);
        return mat;
    }

    /// Create clearcoat material (car paint, lacquered surfaces)
    [[nodiscard]] static GpuMaterial make_clearcoat(std::array<float, 3> color, float coat = 1.0f, float coat_rough = 0.0f) {
        GpuMaterial mat = dielectric(color, 0.5f);
        mat.clearcoat = coat;
        mat.clearcoat_roughness = coat_rough;
        mat.set_flag(FLAG_HAS_CLEARCOAT);
        return mat;
    }

    /// Create subsurface scattering material (skin, wax, etc.)
    [[nodiscard]] static GpuMaterial make_subsurface(std::array<float, 3> color, std::array<float, 3> scatter_color, float sss = 0.5f) {
        GpuMaterial mat = dielectric(color, 0.5f);
        mat.subsurface = sss;
        mat.subsurface_color = scatter_color;
        mat.set_flag(FLAG_HAS_SUBSURFACE);
        return mat;
    }

    /// Create fabric/cloth material with sheen
    [[nodiscard]] static GpuMaterial fabric(std::array<float, 3> color, std::array<float, 3> sheen_col = {1, 1, 1}) {
        GpuMaterial mat;
        mat.base_color = {color[0], color[1], color[2], 1.0f};
        mat.metallic = 0.0f;
        mat.roughness = 0.8f;
        mat.sheen = 1.0f;
        mat.sheen_color = sheen_col;
        mat.sheen_roughness = 0.5f;
        mat.set_flag(FLAG_HAS_SHEEN);
        mat.set_flag(FLAG_DOUBLE_SIDED);
        mat.set_flag(FLAG_RECEIVES_SHADOWS);
        return mat;
    }

    // -------------------------------------------------------------------------
    // Setters for fluent API
    // -------------------------------------------------------------------------

    GpuMaterial& with_base_color(float r, float g, float b, float a = 1.0f) {
        base_color = {r, g, b, a};
        return *this;
    }

    GpuMaterial& with_metallic(float m) {
        metallic = m;
        return *this;
    }

    GpuMaterial& with_roughness(float r) {
        roughness = r;
        return *this;
    }

    GpuMaterial& with_emissive(float r, float g, float b) {
        emissive = {r, g, b};
        return *this;
    }

    GpuMaterial& with_clearcoat(float coat, float rough = 0.0f) {
        clearcoat = coat;
        clearcoat_roughness = rough;
        set_flag(FLAG_HAS_CLEARCOAT, coat > 0.0f);
        return *this;
    }

    GpuMaterial& with_transmission(float t, float refraction = 1.5f) {
        transmission = t;
        ior = refraction;
        set_flag(FLAG_HAS_TRANSMISSION, t > 0.0f);
        return *this;
    }

    GpuMaterial& with_double_sided(bool ds = true) {
        set_flag(FLAG_DOUBLE_SIDED, ds);
        return *this;
    }

    GpuMaterial& with_alpha_mask(float cutoff = 0.5f) {
        alpha_cutoff = cutoff;
        set_flag(FLAG_ALPHA_MASK);
        set_flag(FLAG_ALPHA_BLEND, false);
        return *this;
    }

    GpuMaterial& with_alpha_blend() {
        set_flag(FLAG_ALPHA_BLEND);
        set_flag(FLAG_ALPHA_MASK, false);
        return *this;
    }
};

static_assert(sizeof(GpuMaterial) == 256, "GpuMaterial must be 256 bytes");

// =============================================================================
// MaterialBuffer
// =============================================================================

/// Buffer for all scene materials
class MaterialBuffer {
public:
    /// Default constructor
    MaterialBuffer() {
        m_materials.reserve(MAX_MATERIALS);
    }

    /// Add material
    [[nodiscard]] MaterialId add(const GpuMaterial& material) {
        if (m_materials.size() >= MAX_MATERIALS) {
            return MaterialId::invalid();
        }

        std::uint32_t index = static_cast<std::uint32_t>(m_materials.size());
        m_materials.push_back(material);
        return MaterialId(index);
    }

    /// Add material with asset ID mapping
    [[nodiscard]] MaterialId add(std::uint64_t asset_id, const GpuMaterial& material) {
        auto id = add(material);
        if (id.is_valid()) {
            m_asset_to_index[asset_id] = id.index;
        }
        return id;
    }

    /// Get material by ID
    [[nodiscard]] const GpuMaterial* get(MaterialId id) const {
        if (!id.is_valid() || id.index >= m_materials.size()) {
            return nullptr;
        }
        return &m_materials[id.index];
    }

    /// Get mutable material by ID
    [[nodiscard]] GpuMaterial* get_mut(MaterialId id) {
        if (!id.is_valid() || id.index >= m_materials.size()) {
            return nullptr;
        }
        return &m_materials[id.index];
    }

    /// Get material ID by asset ID
    [[nodiscard]] std::optional<MaterialId> get_by_asset(std::uint64_t asset_id) const {
        auto it = m_asset_to_index.find(asset_id);
        if (it == m_asset_to_index.end()) {
            return std::nullopt;
        }
        return MaterialId(it->second);
    }

    /// Update material
    bool update(MaterialId id, const GpuMaterial& material) {
        if (!id.is_valid() || id.index >= m_materials.size()) {
            return false;
        }
        m_materials[id.index] = material;
        return true;
    }

    /// Get material count
    [[nodiscard]] std::size_t count() const noexcept {
        return m_materials.size();
    }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept {
        return m_materials.empty();
    }

    /// Check if full
    [[nodiscard]] bool is_full() const noexcept {
        return m_materials.size() >= MAX_MATERIALS;
    }

    /// Get raw data pointer
    [[nodiscard]] const void* data() const noexcept {
        return m_materials.data();
    }

    /// Get data size in bytes
    [[nodiscard]] std::size_t data_size() const noexcept {
        return m_materials.size() * sizeof(GpuMaterial);
    }

    /// Get all materials
    [[nodiscard]] const std::vector<GpuMaterial>& materials() const noexcept {
        return m_materials;
    }

    /// Clear all materials
    void clear() {
        m_materials.clear();
        m_asset_to_index.clear();
    }

    /// Ensure default material exists at index 0
    void ensure_default() {
        if (m_materials.empty()) {
            add(GpuMaterial::pbr_default());
        }
    }

private:
    std::vector<GpuMaterial> m_materials;
    std::unordered_map<std::uint64_t, std::uint32_t> m_asset_to_index;
};

} // namespace void_render

// Hash specialization
template<>
struct std::hash<void_render::MaterialId> {
    std::size_t operator()(const void_render::MaterialId& id) const noexcept {
        return std::hash<std::uint32_t>{}(id.index);
    }
};
