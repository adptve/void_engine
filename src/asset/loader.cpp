/// @file loader.cpp
/// @brief void_asset loader implementation
///
/// Provides non-template utilities for the loader system.
/// Core loader functionality is template-based in the header.

#include <void_engine/asset/loader.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace void_asset {

// =============================================================================
// Extension Utilities
// =============================================================================

std::string normalize_extension(const std::string& ext) {
    std::string result = ext;

    // Remove leading dot if present
    if (!result.empty() && result[0] == '.') {
        result = result.substr(1);
    }

    // Convert to lowercase
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return result;
}

bool is_supported_extension(const LoaderRegistry& registry, const std::string& ext) {
    return registry.supports_extension(normalize_extension(ext));
}

std::vector<std::string> get_extensions_for_type(const LoaderRegistry& registry, std::type_index type) {
    std::vector<std::string> extensions;

    auto loaders = registry.find_by_type(type);
    for (const auto* loader : loaders) {
        auto exts = loader->extensions();
        extensions.insert(extensions.end(), exts.begin(), exts.end());
    }

    // Remove duplicates
    std::sort(extensions.begin(), extensions.end());
    extensions.erase(std::unique(extensions.begin(), extensions.end()), extensions.end());

    return extensions;
}

// =============================================================================
// LoadContext Utilities
// =============================================================================

bool is_binary_extension(const std::string& ext) {
    static const std::vector<std::string> text_extensions = {
        "txt", "text", "md", "json", "toml", "yaml", "yml", "xml",
        "html", "htm", "css", "js", "ts", "glsl", "hlsl", "vert",
        "frag", "geom", "comp", "tesc", "tese", "wgsl", "conf", "cfg",
        "ini", "csv", "svg", "lua", "py", "rb", "sh", "bat"
    };

    std::string normalized = normalize_extension(ext);
    return std::find(text_extensions.begin(), text_extensions.end(), normalized)
           == text_extensions.end();
}

// =============================================================================
// Debug Utilities
// =============================================================================

namespace debug {

std::string format_load_context(const LoadContext& ctx) {
    std::ostringstream oss;
    oss << "LoadContext {\n";
    oss << "  path: \"" << ctx.path().str() << "\"\n";
    oss << "  id: " << ctx.id().raw() << "\n";
    oss << "  size: " << ctx.size() << " bytes\n";
    oss << "  extension: \"" << ctx.extension() << "\"\n";
    oss << "  dependencies: " << ctx.dependencies().size() << "\n";
    oss << "  dependency_ids: " << ctx.dependency_ids().size() << "\n";
    oss << "}";
    return oss.str();
}

std::string format_loader_registry(const LoaderRegistry& registry) {
    std::ostringstream oss;
    oss << "LoaderRegistry {\n";
    oss << "  loader_count: " << registry.len() << "\n";
    oss << "  extensions: [";

    auto extensions = registry.supported_extensions();
    for (std::size_t i = 0; i < extensions.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << extensions[i] << "\"";
    }

    oss << "]\n";
    oss << "}";
    return oss.str();
}

std::string format_erased_loader(const ErasedLoader& loader) {
    std::ostringstream oss;
    oss << "ErasedLoader {\n";
    oss << "  type_name: \"" << loader.type_name() << "\"\n";
    oss << "  extensions: [";

    auto extensions = loader.extensions();
    for (std::size_t i = 0; i < extensions.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << extensions[i] << "\"";
    }

    oss << "]\n";
    oss << "}";
    return oss.str();
}

} // namespace debug

// =============================================================================
// Loader Statistics
// =============================================================================

namespace {

struct LoaderStatistics {
    std::atomic<std::uint64_t> total_loads{0};
    std::atomic<std::uint64_t> successful_loads{0};
    std::atomic<std::uint64_t> failed_loads{0};
    std::atomic<std::uint64_t> total_bytes_processed{0};
};

LoaderStatistics s_loader_stats;

} // anonymous namespace

void record_loader_operation(bool success, std::size_t bytes) {
    s_loader_stats.total_loads.fetch_add(1, std::memory_order_relaxed);
    if (success) {
        s_loader_stats.successful_loads.fetch_add(1, std::memory_order_relaxed);
        s_loader_stats.total_bytes_processed.fetch_add(bytes, std::memory_order_relaxed);
    } else {
        s_loader_stats.failed_loads.fetch_add(1, std::memory_order_relaxed);
    }
}

std::string format_loader_statistics() {
    std::ostringstream oss;
    oss << "Loader Statistics:\n";
    oss << "  Total loads: " << s_loader_stats.total_loads.load() << "\n";
    oss << "  Successful: " << s_loader_stats.successful_loads.load() << "\n";
    oss << "  Failed: " << s_loader_stats.failed_loads.load() << "\n";
    oss << "  Bytes processed: " << s_loader_stats.total_bytes_processed.load() << "\n";
    return oss.str();
}

void reset_loader_statistics() {
    s_loader_stats.total_loads.store(0);
    s_loader_stats.successful_loads.store(0);
    s_loader_stats.failed_loads.store(0);
    s_loader_stats.total_bytes_processed.store(0);
}

// =============================================================================
// MIME Type Mapping
// =============================================================================

std::string extension_to_mime_type(const std::string& ext) {
    static const std::map<std::string, std::string> mime_types = {
        // Images
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"bmp", "image/bmp"},
        {"webp", "image/webp"},
        {"tga", "image/tga"},
        {"dds", "image/vnd-ms.dds"},
        {"hdr", "image/vnd.radiance"},
        {"exr", "image/x-exr"},

        // Audio
        {"wav", "audio/wav"},
        {"mp3", "audio/mpeg"},
        {"ogg", "audio/ogg"},
        {"flac", "audio/flac"},

        // Models
        {"gltf", "model/gltf+json"},
        {"glb", "model/gltf-binary"},
        {"obj", "model/obj"},
        {"fbx", "model/fbx"},

        // Shaders
        {"glsl", "text/x-glsl"},
        {"hlsl", "text/x-hlsl"},
        {"vert", "text/x-glsl"},
        {"frag", "text/x-glsl"},
        {"geom", "text/x-glsl"},
        {"comp", "text/x-glsl"},
        {"wgsl", "text/x-wgsl"},
        {"spv", "application/x-spir-v"},

        // Text/Data
        {"txt", "text/plain"},
        {"json", "application/json"},
        {"toml", "application/toml"},
        {"yaml", "application/yaml"},
        {"yml", "application/yaml"},
        {"xml", "application/xml"},

        // Binary
        {"bin", "application/octet-stream"},
        {"dat", "application/octet-stream"},
    };

    std::string normalized = normalize_extension(ext);
    auto it = mime_types.find(normalized);
    if (it != mime_types.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

std::string mime_type_to_extension(const std::string& mime) {
    static const std::map<std::string, std::string> extensions = {
        {"image/png", "png"},
        {"image/jpeg", "jpg"},
        {"image/gif", "gif"},
        {"image/bmp", "bmp"},
        {"image/webp", "webp"},
        {"audio/wav", "wav"},
        {"audio/mpeg", "mp3"},
        {"audio/ogg", "ogg"},
        {"audio/flac", "flac"},
        {"model/gltf+json", "gltf"},
        {"model/gltf-binary", "glb"},
        {"text/plain", "txt"},
        {"application/json", "json"},
        {"application/toml", "toml"},
        {"application/yaml", "yaml"},
        {"application/xml", "xml"},
    };

    auto it = extensions.find(mime);
    if (it != extensions.end()) {
        return it->second;
    }
    return "bin";
}

} // namespace void_asset
