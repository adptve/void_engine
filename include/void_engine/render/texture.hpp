#pragma once

/// @file texture.hpp
/// @brief Texture loading, management, and hot-reload system for void_render

#include "resource.hpp"
#include "fwd.hpp"
#include <void_engine/core/hot_reload.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

namespace void_render {

// =============================================================================
// Forward Declarations
// =============================================================================

class TextureManager;
class Texture;
class Cubemap;

// =============================================================================
// TextureHandle - Hot-reloadable texture reference
// =============================================================================

/// @brief Handle to a managed texture with automatic hot-reload support
class TextureHandle {
public:
    TextureHandle() = default;
    explicit TextureHandle(std::uint64_t id) noexcept : m_id(id) {}

    [[nodiscard]] std::uint64_t id() const noexcept { return m_id; }
    [[nodiscard]] bool is_valid() const noexcept { return m_id != 0; }
    [[nodiscard]] explicit operator bool() const noexcept { return is_valid(); }

    bool operator==(const TextureHandle& other) const noexcept = default;
    bool operator<(const TextureHandle& other) const noexcept { return m_id < other.m_id; }

    static TextureHandle invalid() noexcept { return TextureHandle{}; }

private:
    std::uint64_t m_id = 0;
};

// =============================================================================
// TextureData - CPU-side texture data
// =============================================================================

/// @brief CPU-side texture data container
struct TextureData {
    std::vector<std::uint8_t> pixels;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t depth = 1;
    std::uint32_t channels = 4;
    std::uint32_t mip_levels = 1;
    TextureFormat format = TextureFormat::Rgba8Unorm;
    bool is_hdr = false;
    bool is_srgb = true;

    /// @brief Calculate total size in bytes
    [[nodiscard]] std::size_t size_bytes() const noexcept {
        return pixels.size();
    }

    /// @brief Check if data is valid
    [[nodiscard]] bool is_valid() const noexcept {
        return !pixels.empty() && width > 0 && height > 0;
    }

    /// @brief Get pixel at (x, y) - assumes RGBA8
    [[nodiscard]] std::array<std::uint8_t, 4> get_pixel(std::uint32_t x, std::uint32_t y) const {
        if (x >= width || y >= height || channels < 1) {
            return {0, 0, 0, 255};
        }
        std::size_t idx = (y * width + x) * channels;
        std::array<std::uint8_t, 4> result = {0, 0, 0, 255};
        for (std::uint32_t c = 0; c < channels && c < 4; ++c) {
            result[c] = pixels[idx + c];
        }
        return result;
    }

    /// @brief Generate mipmaps (returns new TextureData with all mip levels)
    [[nodiscard]] TextureData generate_mipmaps() const;

    /// @brief Create from raw RGBA data
    static TextureData from_rgba(const std::uint8_t* data, std::uint32_t w, std::uint32_t h);

    /// @brief Create solid color texture
    static TextureData solid_color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    /// @brief Create checkerboard pattern
    static TextureData checkerboard(std::uint32_t size = 256, std::uint32_t cell_size = 32);

    /// @brief Create default normal map (flat)
    static TextureData default_normal();

    /// @brief Create default white texture
    static TextureData default_white();

    /// @brief Create default black texture
    static TextureData default_black();
};

// =============================================================================
// HDR TextureData - Float-based HDR texture data
// =============================================================================

/// @brief CPU-side HDR texture data container
struct HdrTextureData {
    std::vector<float> pixels;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t channels = 3;  // RGB for HDR

    /// @brief Check if data is valid
    [[nodiscard]] bool is_valid() const noexcept {
        return !pixels.empty() && width > 0 && height > 0;
    }

    /// @brief Get pixel at (x, y) as RGB float
    [[nodiscard]] std::array<float, 3> get_pixel(std::uint32_t x, std::uint32_t y) const {
        if (x >= width || y >= height) {
            return {0.0f, 0.0f, 0.0f};
        }
        std::size_t idx = (y * width + x) * channels;
        return {pixels[idx], pixels[idx + 1], pixels[idx + 2]};
    }

    /// @brief Convert to LDR TextureData with tonemapping
    [[nodiscard]] TextureData to_ldr(float exposure = 1.0f) const;
};

// =============================================================================
// CubemapData - 6-face cubemap data
// =============================================================================

/// @brief CPU-side cubemap data
struct CubemapData {
    enum class Face : std::uint8_t {
        PositiveX = 0,  // Right
        NegativeX,      // Left
        PositiveY,      // Top
        NegativeY,      // Bottom
        PositiveZ,      // Front
        NegativeZ,      // Back
        Count
    };

    std::array<TextureData, 6> faces;
    bool is_hdr = false;

    /// @brief Check if all faces are valid
    [[nodiscard]] bool is_valid() const noexcept {
        for (const auto& face : faces) {
            if (!face.is_valid()) return false;
        }
        return true;
    }

    /// @brief Get face size (assumes all faces are same size)
    [[nodiscard]] std::uint32_t face_size() const noexcept {
        return faces[0].width;
    }

    /// @brief Create from equirectangular HDR map
    static CubemapData from_equirectangular(const HdrTextureData& equirect, std::uint32_t face_size = 512);

    /// @brief Create from 6 individual face images
    static CubemapData from_faces(const std::array<std::filesystem::path, 6>& paths);
};

// =============================================================================
// TextureLoadOptions
// =============================================================================

/// @brief Options for texture loading
struct TextureLoadOptions {
    bool generate_mipmaps = true;
    bool flip_y = true;  // Most formats need Y flip for OpenGL
    bool force_rgba = true;  // Force 4-channel output
    bool srgb = true;  // Interpret as sRGB color space
    bool hdr = false;  // Load as HDR (float) data
    FilterMode filter = FilterMode::Linear;
    AddressMode wrap = AddressMode::Repeat;
    std::uint16_t anisotropy = 16;

    static TextureLoadOptions default_diffuse() {
        TextureLoadOptions opts;
        opts.srgb = true;
        return opts;
    }

    static TextureLoadOptions default_normal() {
        TextureLoadOptions opts;
        opts.srgb = false;  // Normal maps are linear
        return opts;
    }

    static TextureLoadOptions default_hdr() {
        TextureLoadOptions opts;
        opts.hdr = true;
        opts.srgb = false;
        opts.generate_mipmaps = false;  // Usually generate separately for IBL
        return opts;
    }
};

// =============================================================================
// Texture - GPU texture wrapper
// =============================================================================

/// @brief GPU texture with metadata
class Texture {
public:
    Texture() = default;
    ~Texture();

    // Non-copyable, moveable
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    /// @brief Create texture from data
    bool create(const TextureData& data, const TextureLoadOptions& options = {});

    /// @brief Create texture from HDR data
    bool create_hdr(const HdrTextureData& data, const TextureLoadOptions& options = {});

    /// @brief Create render target
    bool create_render_target(std::uint32_t width, std::uint32_t height,
                               TextureFormat format, std::uint32_t samples = 1);

    /// @brief Create depth buffer
    bool create_depth(std::uint32_t width, std::uint32_t height,
                      TextureFormat format = TextureFormat::Depth32Float,
                      std::uint32_t samples = 1);

    /// @brief Destroy GPU resources
    void destroy();

    /// @brief Bind to texture unit
    void bind(std::uint32_t unit = 0) const;

    /// @brief Unbind from texture unit
    static void unbind(std::uint32_t unit = 0);

    /// @brief Update texture data (sub-region)
    void update(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h,
                const void* data);

    /// @brief Generate mipmaps on GPU
    void generate_mipmaps();

    // Accessors
    [[nodiscard]] std::uint32_t id() const noexcept { return m_id; }
    [[nodiscard]] std::uint32_t width() const noexcept { return m_width; }
    [[nodiscard]] std::uint32_t height() const noexcept { return m_height; }
    [[nodiscard]] TextureFormat format() const noexcept { return m_format; }
    [[nodiscard]] bool is_valid() const noexcept { return m_id != 0; }
    [[nodiscard]] bool is_hdr() const noexcept { return m_is_hdr; }
    [[nodiscard]] std::uint32_t mip_levels() const noexcept { return m_mip_levels; }

    /// @brief Get GPU memory usage estimate
    [[nodiscard]] std::size_t gpu_memory_bytes() const noexcept;

private:
    std::uint32_t m_id = 0;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
    std::uint32_t m_mip_levels = 1;
    TextureFormat m_format = TextureFormat::Rgba8Unorm;
    bool m_is_hdr = false;
};

// =============================================================================
// Cubemap - GPU cubemap texture
// =============================================================================

/// @brief GPU cubemap texture
class Cubemap {
public:
    Cubemap() = default;
    ~Cubemap();

    // Non-copyable, moveable
    Cubemap(const Cubemap&) = delete;
    Cubemap& operator=(const Cubemap&) = delete;
    Cubemap(Cubemap&& other) noexcept;
    Cubemap& operator=(Cubemap&& other) noexcept;

    /// @brief Create from cubemap data
    bool create(const CubemapData& data, bool generate_mipmaps = true);

    /// @brief Create from equirectangular HDR
    bool create_from_equirectangular(const HdrTextureData& equirect,
                                      std::uint32_t face_size = 512);

    /// @brief Destroy GPU resources
    void destroy();

    /// @brief Bind to texture unit
    void bind(std::uint32_t unit = 0) const;

    // Accessors
    [[nodiscard]] std::uint32_t id() const noexcept { return m_id; }
    [[nodiscard]] std::uint32_t face_size() const noexcept { return m_face_size; }
    [[nodiscard]] bool is_valid() const noexcept { return m_id != 0; }

private:
    std::uint32_t m_id = 0;
    std::uint32_t m_face_size = 0;
    bool m_is_hdr = false;
};

// =============================================================================
// Sampler - GPU sampler state
// =============================================================================

/// @brief GPU sampler object
class Sampler {
public:
    Sampler() = default;
    ~Sampler();

    // Non-copyable, moveable
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;
    Sampler(Sampler&& other) noexcept;
    Sampler& operator=(Sampler&& other) noexcept;

    /// @brief Create sampler from descriptor
    bool create(const SamplerDesc& desc);

    /// @brief Destroy GPU resources
    void destroy();

    /// @brief Bind to texture unit
    void bind(std::uint32_t unit = 0) const;

    [[nodiscard]] std::uint32_t id() const noexcept { return m_id; }
    [[nodiscard]] bool is_valid() const noexcept { return m_id != 0; }

private:
    std::uint32_t m_id = 0;
};

// =============================================================================
// TextureLoader - File loading utilities
// =============================================================================

/// @brief Texture file loading utilities
class TextureLoader {
public:
    /// @brief Load texture from file
    [[nodiscard]] static std::optional<TextureData> load(
        const std::filesystem::path& path,
        const TextureLoadOptions& options = {});

    /// @brief Load HDR texture from file
    [[nodiscard]] static std::optional<HdrTextureData> load_hdr(
        const std::filesystem::path& path);

    /// @brief Load cubemap from 6 face files
    [[nodiscard]] static std::optional<CubemapData> load_cubemap(
        const std::array<std::filesystem::path, 6>& paths);

    /// @brief Load cubemap from directory (expects +x, -x, +y, -y, +z, -z or right, left, top, bottom, front, back)
    [[nodiscard]] static std::optional<CubemapData> load_cubemap_directory(
        const std::filesystem::path& directory);

    /// @brief Load cubemap from equirectangular HDR
    [[nodiscard]] static std::optional<CubemapData> load_cubemap_equirectangular(
        const std::filesystem::path& hdr_path,
        std::uint32_t face_size = 512);

    /// @brief Save texture to file (PNG, JPG, BMP, TGA)
    static bool save(const std::filesystem::path& path, const TextureData& data);

    /// @brief Save HDR texture to file
    static bool save_hdr(const std::filesystem::path& path, const HdrTextureData& data);

    /// @brief Check if file extension is supported
    [[nodiscard]] static bool is_supported_format(const std::filesystem::path& path);

    /// @brief Check if file is HDR format
    [[nodiscard]] static bool is_hdr_format(const std::filesystem::path& path);

private:
    static std::optional<TextureData> load_stb(const std::filesystem::path& path,
                                                const TextureLoadOptions& options);
    static std::optional<HdrTextureData> load_stb_hdr(const std::filesystem::path& path);
};

// =============================================================================
// TextureManager - Hot-reloadable texture cache
// =============================================================================

/// @brief Manages textures with hot-reload support for metaverse applications
class TextureManager {
public:
    TextureManager();
    ~TextureManager();

    // Non-copyable
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    /// @brief Initialize the manager
    bool initialize();

    /// @brief Shutdown and release all resources
    void shutdown();

    /// @brief Load texture from file (returns handle for hot-reload)
    [[nodiscard]] TextureHandle load(const std::filesystem::path& path,
                                     const TextureLoadOptions& options = {});

    /// @brief Load texture from memory
    [[nodiscard]] TextureHandle load_from_memory(const std::string& name,
                                                  const TextureData& data,
                                                  const TextureLoadOptions& options = {});

    /// @brief Load cubemap from equirectangular HDR
    [[nodiscard]] TextureHandle load_cubemap(const std::filesystem::path& path,
                                              std::uint32_t face_size = 512);

    /// @brief Get texture by handle
    [[nodiscard]] Texture* get(TextureHandle handle);
    [[nodiscard]] const Texture* get(TextureHandle handle) const;

    /// @brief Get cubemap by handle
    [[nodiscard]] Cubemap* get_cubemap(TextureHandle handle);
    [[nodiscard]] const Cubemap* get_cubemap(TextureHandle handle) const;

    /// @brief Check if handle is valid
    [[nodiscard]] bool is_valid(TextureHandle handle) const;

    /// @brief Release texture (decrements ref count)
    void release(TextureHandle handle);

    /// @brief Force reload a specific texture
    bool reload(TextureHandle handle);

    /// @brief Check for file changes and hot-reload
    void update();

    /// @brief Get default textures
    [[nodiscard]] TextureHandle default_white() const { return m_default_white; }
    [[nodiscard]] TextureHandle default_black() const { return m_default_black; }
    [[nodiscard]] TextureHandle default_normal() const { return m_default_normal; }
    [[nodiscard]] TextureHandle default_checkerboard() const { return m_default_checker; }

    /// @brief Get statistics
    struct Stats {
        std::size_t texture_count = 0;
        std::size_t cubemap_count = 0;
        std::size_t total_gpu_memory = 0;
        std::size_t reload_count = 0;
    };
    [[nodiscard]] Stats stats() const;

    /// @brief Set hot-reload check interval
    void set_reload_interval(float seconds) { m_reload_interval = seconds; }

    /// @brief Callback when a texture is reloaded
    std::function<void(TextureHandle)> on_texture_reloaded;

private:
    struct TextureEntry {
        std::unique_ptr<Texture> texture;
        std::filesystem::path path;
        TextureLoadOptions options;
        std::filesystem::file_time_type last_modified;
        std::uint32_t ref_count = 1;
        bool is_cubemap = false;
    };

    struct CubemapEntry {
        std::unique_ptr<Cubemap> cubemap;
        std::filesystem::path path;
        std::uint32_t face_size = 512;
        std::filesystem::file_time_type last_modified;
        std::uint32_t ref_count = 1;
    };

    mutable std::mutex m_mutex;
    std::unordered_map<std::uint64_t, TextureEntry> m_textures;
    std::unordered_map<std::uint64_t, CubemapEntry> m_cubemaps;
    std::unordered_map<std::string, std::uint64_t> m_path_to_handle;
    std::atomic<std::uint64_t> m_next_handle{1};

    // Default textures
    TextureHandle m_default_white;
    TextureHandle m_default_black;
    TextureHandle m_default_normal;
    TextureHandle m_default_checker;

    // Hot-reload
    float m_reload_timer = 0.0f;
    float m_reload_interval = 0.5f;  // Check every 0.5 seconds
    std::size_t m_reload_count = 0;

    std::uint64_t allocate_handle();
    void check_for_reloads();
    void create_default_textures();
};

// =============================================================================
// IBL (Image-Based Lighting) Utilities
// =============================================================================

/// @brief Image-Based Lighting precomputation utilities
class IBLProcessor {
public:
    /// @brief Generate irradiance map from environment cubemap
    static std::unique_ptr<Cubemap> generate_irradiance_map(
        const Cubemap& environment, std::uint32_t size = 32);

    /// @brief Generate prefiltered environment map for specular IBL
    static std::unique_ptr<Cubemap> generate_prefiltered_map(
        const Cubemap& environment, std::uint32_t size = 128);

    /// @brief Generate BRDF LUT for split-sum approximation
    static std::unique_ptr<Texture> generate_brdf_lut(std::uint32_t size = 512);

    /// @brief Full IBL setup from HDR environment
    struct IBLMaps {
        std::unique_ptr<Cubemap> environment;
        std::unique_ptr<Cubemap> irradiance;
        std::unique_ptr<Cubemap> prefiltered;
        std::unique_ptr<Texture> brdf_lut;

        [[nodiscard]] bool is_valid() const {
            return environment && irradiance && prefiltered && brdf_lut;
        }
    };

    static IBLMaps create_from_hdr(const std::filesystem::path& hdr_path,
                                    std::uint32_t env_size = 512);
};

} // namespace void_render

// Hash specializations
template<>
struct std::hash<void_render::TextureHandle> {
    std::size_t operator()(const void_render::TextureHandle& h) const noexcept {
        return std::hash<std::uint64_t>{}(h.id());
    }
};
