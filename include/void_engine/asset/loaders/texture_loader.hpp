#pragma once

/// @file texture_loader.hpp
/// @brief Texture asset loader for PNG, JPG, HDR, and other image formats

#include <void_engine/asset/loader.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace void_asset {

// =============================================================================
// Texture Asset Types
// =============================================================================

/// Texture format
enum class TextureFormat : std::uint8_t {
    R8,
    RG8,
    RGB8,
    RGBA8,
    R16F,
    RG16F,
    RGB16F,
    RGBA16F,
    R32F,
    RG32F,
    RGB32F,
    RGBA32F,
    BC1,  // DXT1
    BC3,  // DXT5
    BC5,  // Normal maps
    BC7,  // High quality
};

/// Texture type
enum class TextureType : std::uint8_t {
    Texture2D,
    Cubemap,
    Texture2DArray,
    Texture3D,
};

/// Texture usage hints
enum class TextureUsage : std::uint8_t {
    Default = 0,
    Albedo = 1,       // sRGB
    Normal = 2,       // Linear, BC5
    MetallicRoughness = 3,
    Emissive = 4,     // sRGB
    AO = 5,           // Linear, single channel
    Height = 6,       // Linear, single channel
    Environment = 7,  // HDR cubemap
};

/// Loaded texture asset
struct TextureAsset {
    std::string name;
    std::vector<std::uint8_t> data;  // Raw pixel data or compressed
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t depth = 1;
    std::uint32_t mip_levels = 1;
    std::uint32_t array_layers = 1;
    TextureFormat format = TextureFormat::RGBA8;
    TextureType type = TextureType::Texture2D;
    TextureUsage usage = TextureUsage::Default;
    bool is_srgb = false;
    bool is_hdr = false;
    bool generate_mipmaps = true;

    /// Get bytes per pixel for uncompressed formats
    [[nodiscard]] std::uint32_t bytes_per_pixel() const {
        switch (format) {
            case TextureFormat::R8: return 1;
            case TextureFormat::RG8: return 2;
            case TextureFormat::RGB8: return 3;
            case TextureFormat::RGBA8: return 4;
            case TextureFormat::R16F: return 2;
            case TextureFormat::RG16F: return 4;
            case TextureFormat::RGB16F: return 6;
            case TextureFormat::RGBA16F: return 8;
            case TextureFormat::R32F: return 4;
            case TextureFormat::RG32F: return 8;
            case TextureFormat::RGB32F: return 12;
            case TextureFormat::RGBA32F: return 16;
            default: return 0;  // Compressed
        }
    }

    /// Get expected data size for uncompressed texture
    [[nodiscard]] std::size_t expected_size() const {
        return static_cast<std::size_t>(width) * height * depth * bytes_per_pixel();
    }
};

// =============================================================================
// Texture Loader
// =============================================================================

/// Loads texture assets from various image formats
class TextureLoader : public AssetLoader<TextureAsset> {
public:
    TextureLoader() = default;

    [[nodiscard]] std::vector<std::string> extensions() const override {
        return {"png", "jpg", "jpeg", "hdr", "tga", "bmp", "psd", "gif", "ktx", "ktx2", "dds"};
    }

    [[nodiscard]] LoadResult<TextureAsset> load(LoadContext& ctx) override;

    [[nodiscard]] std::string type_name() const override {
        return "TextureAsset";
    }

    /// Set default sRGB behavior
    void set_default_srgb(bool srgb) { m_default_srgb = srgb; }

    /// Set whether to auto-detect sRGB from filename
    void set_auto_detect_srgb(bool detect) { m_auto_detect_srgb = detect; }

    /// Set whether to generate mipmaps by default
    void set_generate_mipmaps(bool generate) { m_generate_mipmaps = generate; }

private:
    LoadResult<TextureAsset> load_standard(LoadContext& ctx);
    LoadResult<TextureAsset> load_hdr(LoadContext& ctx);
    LoadResult<TextureAsset> load_ktx(LoadContext& ctx);
    LoadResult<TextureAsset> load_dds(LoadContext& ctx);

    TextureUsage detect_usage(const std::string& path) const;
    bool detect_srgb(const std::string& path, TextureUsage usage) const;

    bool m_default_srgb = true;
    bool m_auto_detect_srgb = true;
    bool m_generate_mipmaps = true;
};

// =============================================================================
// Cubemap Loader
// =============================================================================

/// Cubemap face identifiers
enum class CubemapFace : std::uint8_t {
    PositiveX = 0,
    NegativeX = 1,
    PositiveY = 2,
    NegativeY = 3,
    PositiveZ = 4,
    NegativeZ = 5,
};

/// Loaded cubemap asset
struct CubemapAsset {
    std::string name;
    std::array<std::vector<std::uint8_t>, 6> faces;
    std::uint32_t face_size = 0;
    TextureFormat format = TextureFormat::RGBA8;
    bool is_hdr = false;
    bool is_srgb = false;
};

/// Loads cubemap assets from HDR environment maps or 6-face images
class CubemapLoader : public AssetLoader<CubemapAsset> {
public:
    CubemapLoader() = default;

    [[nodiscard]] std::vector<std::string> extensions() const override {
        return {"hdr", "exr", "ktx", "ktx2", "dds"};
    }

    [[nodiscard]] LoadResult<CubemapAsset> load(LoadContext& ctx) override;

    [[nodiscard]] std::string type_name() const override {
        return "CubemapAsset";
    }

    /// Set cubemap face size for equirectangular conversion
    void set_face_size(std::uint32_t size) { m_face_size = size; }

private:
    LoadResult<CubemapAsset> load_equirectangular(LoadContext& ctx);
    LoadResult<CubemapAsset> load_ktx_cubemap(LoadContext& ctx);

    std::uint32_t m_face_size = 512;
};

} // namespace void_asset
