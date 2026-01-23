/// @file texture_loader.cpp
/// @brief Texture asset loader implementation

#include <void_engine/asset/loaders/texture_loader.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

// Forward declare stb_image functions (implemented elsewhere or linked)
extern "C" {
    unsigned char* stbi_load_from_memory(const unsigned char* buffer, int len, int* x, int* y, int* channels_in_file, int desired_channels);
    float* stbi_loadf_from_memory(const unsigned char* buffer, int len, int* x, int* y, int* channels_in_file, int desired_channels);
    void stbi_image_free(void* retval_from_stbi_load);
    const char* stbi_failure_reason(void);
}

namespace void_asset {

// =============================================================================
// TextureLoader Implementation
// =============================================================================

LoadResult<TextureAsset> TextureLoader::load(LoadContext& ctx) {
    std::string ext = ctx.extension();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "hdr" || ext == "exr") {
        return load_hdr(ctx);
    } else if (ext == "ktx" || ext == "ktx2") {
        return load_ktx(ctx);
    } else if (ext == "dds") {
        return load_dds(ctx);
    } else {
        return load_standard(ctx);
    }
}

LoadResult<TextureAsset> TextureLoader::load_standard(LoadContext& ctx) {
    const auto& data = ctx.data();

    int width, height, channels;
    unsigned char* pixels = stbi_load_from_memory(
        data.data(), static_cast<int>(data.size()),
        &width, &height, &channels, 4);  // Always request RGBA

    if (!pixels) {
        const char* reason = stbi_failure_reason();
        return void_core::Err<std::unique_ptr<TextureAsset>>(
            void_core::Error("Failed to load texture: " + std::string(reason ? reason : "unknown error")));
    }

    auto asset = std::make_unique<TextureAsset>();
    asset->name = ctx.path().filename();
    asset->width = static_cast<std::uint32_t>(width);
    asset->height = static_cast<std::uint32_t>(height);
    asset->format = TextureFormat::RGBA8;
    asset->type = TextureType::Texture2D;
    asset->generate_mipmaps = m_generate_mipmaps;

    // Detect usage and sRGB from filename
    asset->usage = detect_usage(ctx.path().str());
    asset->is_srgb = detect_srgb(ctx.path().str(), asset->usage);

    // Copy pixel data
    std::size_t size = static_cast<std::size_t>(width) * height * 4;
    asset->data.resize(size);
    std::memcpy(asset->data.data(), pixels, size);

    stbi_image_free(pixels);

    return void_core::Ok(std::move(asset));
}

LoadResult<TextureAsset> TextureLoader::load_hdr(LoadContext& ctx) {
    const auto& data = ctx.data();

    int width, height, channels;
    float* pixels = stbi_loadf_from_memory(
        data.data(), static_cast<int>(data.size()),
        &width, &height, &channels, 4);

    if (!pixels) {
        const char* reason = stbi_failure_reason();
        return void_core::Err<std::unique_ptr<TextureAsset>>(
            void_core::Error("Failed to load HDR texture: " + std::string(reason ? reason : "unknown error")));
    }

    auto asset = std::make_unique<TextureAsset>();
    asset->name = ctx.path().filename();
    asset->width = static_cast<std::uint32_t>(width);
    asset->height = static_cast<std::uint32_t>(height);
    asset->format = TextureFormat::RGBA32F;
    asset->type = TextureType::Texture2D;
    asset->is_hdr = true;
    asset->is_srgb = false;  // HDR is always linear
    asset->usage = TextureUsage::Environment;
    asset->generate_mipmaps = m_generate_mipmaps;

    // Copy pixel data
    std::size_t size = static_cast<std::size_t>(width) * height * 4 * sizeof(float);
    asset->data.resize(size);
    std::memcpy(asset->data.data(), pixels, size);

    stbi_image_free(pixels);

    return void_core::Ok(std::move(asset));
}

LoadResult<TextureAsset> TextureLoader::load_ktx(LoadContext& ctx) {
    const auto& data = ctx.data();

    // KTX file magic
    static const std::uint8_t KTX_MAGIC[] = {
        0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
    };

    if (data.size() < 64 || std::memcmp(data.data(), KTX_MAGIC, 12) != 0) {
        return void_core::Err<std::unique_ptr<TextureAsset>>(
            void_core::Error("Invalid KTX file format"));
    }

    // Parse KTX header with endianness handling
    auto swap_u32 = [](std::uint32_t value) -> std::uint32_t {
        return ((value & 0xFF000000) >> 24) |
               ((value & 0x00FF0000) >> 8) |
               ((value & 0x0000FF00) << 8) |
               ((value & 0x000000FF) << 24);
    };

    std::uint32_t endianness_marker = 0;
    std::memcpy(&endianness_marker, data.data() + 12, 4);
    bool swap_endian = (endianness_marker == 0x01020304);

    auto read_u32 = [&data, swap_endian, &swap_u32](std::size_t offset) -> std::uint32_t {
        std::uint32_t value = 0;
        std::memcpy(&value, data.data() + offset, 4);
        return swap_endian ? swap_u32(value) : value;
    };

    std::uint32_t gl_type = read_u32(16);
    std::uint32_t gl_format = read_u32(24);
    std::uint32_t width = read_u32(36);
    std::uint32_t height = read_u32(40);
    std::uint32_t depth = read_u32(44);
    std::uint32_t array_elements = read_u32(48);
    std::uint32_t faces = read_u32(52);
    std::uint32_t mip_levels = read_u32(56);
    std::uint32_t metadata_size = read_u32(60);

    auto asset = std::make_unique<TextureAsset>();
    asset->name = ctx.path().filename();
    asset->width = width;
    asset->height = height;
    asset->depth = depth > 0 ? depth : 1;
    asset->array_layers = array_elements > 0 ? array_elements : 1;
    asset->mip_levels = mip_levels > 0 ? mip_levels : 1;
    asset->generate_mipmaps = false;  // Already has mipmaps

    // Determine format from GL type/format
    if (gl_type == 0) {
        // Compressed format
        asset->format = TextureFormat::BC7;  // Assume BC7 for now
    } else if (gl_format == 0x1908) {  // GL_RGBA
        asset->format = TextureFormat::RGBA8;
    } else {
        asset->format = TextureFormat::RGBA8;
    }

    // Determine type
    if (faces == 6) {
        asset->type = TextureType::Cubemap;
    } else if (depth > 1) {
        asset->type = TextureType::Texture3D;
    } else if (array_elements > 1) {
        asset->type = TextureType::Texture2DArray;
    } else {
        asset->type = TextureType::Texture2D;
    }

    // Copy texture data (skip header + metadata)
    std::size_t data_offset = 64 + metadata_size;
    if (data_offset < data.size()) {
        asset->data.assign(data.begin() + data_offset, data.end());
    }

    return void_core::Ok(std::move(asset));
}

LoadResult<TextureAsset> TextureLoader::load_dds(LoadContext& ctx) {
    const auto& data = ctx.data();

    // DDS magic number
    if (data.size() < 128 || std::memcmp(data.data(), "DDS ", 4) != 0) {
        return void_core::Err<std::unique_ptr<TextureAsset>>(
            void_core::Error("Invalid DDS file format"));
    }

    auto read_u32 = [&data](std::size_t offset) -> std::uint32_t {
        std::uint32_t value = 0;
        std::memcpy(&value, data.data() + offset, 4);
        return value;
    };

    // Parse DDS header
    std::uint32_t height = read_u32(12);
    std::uint32_t width = read_u32(16);
    std::uint32_t mip_count = read_u32(28);
    std::uint32_t fourcc = read_u32(84);

    auto asset = std::make_unique<TextureAsset>();
    asset->name = ctx.path().filename();
    asset->width = width;
    asset->height = height;
    asset->mip_levels = mip_count > 0 ? mip_count : 1;
    asset->type = TextureType::Texture2D;
    asset->generate_mipmaps = false;

    // Determine format from FourCC
    switch (fourcc) {
        case 0x31545844:  // DXT1
            asset->format = TextureFormat::BC1;
            break;
        case 0x33545844:  // DXT3
        case 0x35545844:  // DXT5
            asset->format = TextureFormat::BC3;
            break;
        default:
            asset->format = TextureFormat::RGBA8;
            break;
    }

    // Copy texture data
    std::size_t data_offset = 128;
    if (data.size() > data_offset) {
        asset->data.assign(data.begin() + data_offset, data.end());
    }

    return void_core::Ok(std::move(asset));
}

TextureUsage TextureLoader::detect_usage(const std::string& path) const {
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("normal") != std::string::npos ||
        lower.find("_n.") != std::string::npos ||
        lower.find("_nrm") != std::string::npos) {
        return TextureUsage::Normal;
    }
    if (lower.find("metallic") != std::string::npos ||
        lower.find("roughness") != std::string::npos ||
        lower.find("_mr.") != std::string::npos ||
        lower.find("_orm") != std::string::npos) {
        return TextureUsage::MetallicRoughness;
    }
    if (lower.find("emissive") != std::string::npos ||
        lower.find("emission") != std::string::npos) {
        return TextureUsage::Emissive;
    }
    if (lower.find("ao") != std::string::npos ||
        lower.find("occlusion") != std::string::npos ||
        lower.find("_ao.") != std::string::npos) {
        return TextureUsage::AO;
    }
    if (lower.find("height") != std::string::npos ||
        lower.find("displacement") != std::string::npos ||
        lower.find("_h.") != std::string::npos) {
        return TextureUsage::Height;
    }
    if (lower.find("albedo") != std::string::npos ||
        lower.find("diffuse") != std::string::npos ||
        lower.find("color") != std::string::npos ||
        lower.find("_c.") != std::string::npos) {
        return TextureUsage::Albedo;
    }

    return TextureUsage::Default;
}

bool TextureLoader::detect_srgb(const std::string& path, TextureUsage usage) const {
    if (!m_auto_detect_srgb) {
        return m_default_srgb;
    }

    // These textures should NOT be sRGB (they contain linear data)
    switch (usage) {
        case TextureUsage::Normal:
        case TextureUsage::MetallicRoughness:
        case TextureUsage::AO:
        case TextureUsage::Height:
        case TextureUsage::Environment:
            return false;

        // These should be sRGB (perceptual color data)
        case TextureUsage::Albedo:
        case TextureUsage::Emissive:
        case TextureUsage::Default:
            return true;
    }

    return m_default_srgb;
}

// =============================================================================
// CubemapLoader Implementation
// =============================================================================

LoadResult<CubemapAsset> CubemapLoader::load(LoadContext& ctx) {
    std::string ext = ctx.extension();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "hdr" || ext == "exr") {
        return load_equirectangular(ctx);
    } else if (ext == "ktx" || ext == "ktx2") {
        return load_ktx_cubemap(ctx);
    }

    return void_core::Err<std::unique_ptr<CubemapAsset>>(
        void_core::Error("Unsupported cubemap format: " + ext));
}

LoadResult<CubemapAsset> CubemapLoader::load_equirectangular(LoadContext& ctx) {
    const auto& data = ctx.data();

    int width, height, channels;
    float* pixels = stbi_loadf_from_memory(
        data.data(), static_cast<int>(data.size()),
        &width, &height, &channels, 4);

    if (!pixels) {
        return void_core::Err<std::unique_ptr<CubemapAsset>>(
            void_core::Error("Failed to load equirectangular map"));
    }

    auto asset = std::make_unique<CubemapAsset>();
    asset->name = ctx.path().filename();
    asset->face_size = m_face_size;
    asset->format = TextureFormat::RGBA32F;
    asset->is_hdr = true;
    asset->is_srgb = false;

    // Convert equirectangular to cubemap
    // Each face is face_size x face_size
    std::size_t face_bytes = m_face_size * m_face_size * 4 * sizeof(float);

    for (int face = 0; face < 6; ++face) {
        asset->faces[face].resize(face_bytes);
        auto* face_pixels = reinterpret_cast<float*>(asset->faces[face].data());

        for (std::uint32_t y = 0; y < m_face_size; ++y) {
            for (std::uint32_t x = 0; x < m_face_size; ++x) {
                // Convert face coordinates to direction vector
                float u = (static_cast<float>(x) + 0.5f) / m_face_size * 2.0f - 1.0f;
                float v = (static_cast<float>(y) + 0.5f) / m_face_size * 2.0f - 1.0f;

                float dir[3];
                switch (face) {
                    case 0: dir[0] =  1; dir[1] = -v; dir[2] = -u; break;  // +X
                    case 1: dir[0] = -1; dir[1] = -v; dir[2] =  u; break;  // -X
                    case 2: dir[0] =  u; dir[1] =  1; dir[2] =  v; break;  // +Y
                    case 3: dir[0] =  u; dir[1] = -1; dir[2] = -v; break;  // -Y
                    case 4: dir[0] =  u; dir[1] = -v; dir[2] =  1; break;  // +Z
                    case 5: dir[0] = -u; dir[1] = -v; dir[2] = -1; break;  // -Z
                }

                // Normalize direction
                float len = std::sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
                dir[0] /= len; dir[1] /= len; dir[2] /= len;

                // Convert to equirectangular coordinates
                float theta = std::atan2(dir[0], dir[2]);
                float phi = std::asin(dir[1]);

                float eq_u = (theta / (2.0f * 3.14159265f)) + 0.5f;
                float eq_v = (phi / 3.14159265f) + 0.5f;

                // Sample from equirectangular
                int src_x = static_cast<int>(eq_u * width) % width;
                int src_y = static_cast<int>(eq_v * height) % height;
                int src_idx = (src_y * width + src_x) * 4;

                int dst_idx = (y * m_face_size + x) * 4;
                face_pixels[dst_idx + 0] = pixels[src_idx + 0];
                face_pixels[dst_idx + 1] = pixels[src_idx + 1];
                face_pixels[dst_idx + 2] = pixels[src_idx + 2];
                face_pixels[dst_idx + 3] = pixels[src_idx + 3];
            }
        }
    }

    stbi_image_free(pixels);

    return void_core::Ok(std::move(asset));
}

LoadResult<CubemapAsset> CubemapLoader::load_ktx_cubemap(LoadContext& ctx) {
    // Reuse texture loader for KTX parsing
    TextureLoader tex_loader;
    auto tex_result = tex_loader.load(ctx);
    if (!tex_result) {
        return void_core::Err<std::unique_ptr<CubemapAsset>>(tex_result.error());
    }

    auto& tex = *tex_result.value();
    if (tex.type != TextureType::Cubemap) {
        return void_core::Err<std::unique_ptr<CubemapAsset>>(
            void_core::Error("KTX file is not a cubemap"));
    }

    auto asset = std::make_unique<CubemapAsset>();
    asset->name = tex.name;
    asset->face_size = tex.width;
    asset->format = tex.format;
    asset->is_hdr = tex.is_hdr;
    asset->is_srgb = tex.is_srgb;

    // Split data into 6 faces
    std::size_t face_size = tex.data.size() / 6;
    for (int i = 0; i < 6; ++i) {
        asset->faces[i].assign(
            tex.data.begin() + i * face_size,
            tex.data.begin() + (i + 1) * face_size);
    }

    return void_core::Ok(std::move(asset));
}

} // namespace void_asset
