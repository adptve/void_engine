/// @file fwd.cpp
/// @brief Implementation of forward declaration utilities

#include <void_engine/package/fwd.hpp>
#include <cstring>
#include <algorithm>

namespace void_package {

const char* package_type_to_string(PackageType type) noexcept {
    switch (type) {
        case PackageType::World:  return "world";
        case PackageType::Layer:  return "layer";
        case PackageType::Plugin: return "plugin";
        case PackageType::Widget: return "widget";
        case PackageType::Asset:  return "asset";
        default:                  return "unknown";
    }
}

bool package_type_from_string(const std::string& str, PackageType& out_type) noexcept {
    // Convert to lowercase for comparison
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "world") {
        out_type = PackageType::World;
        return true;
    }
    if (lower == "layer") {
        out_type = PackageType::Layer;
        return true;
    }
    if (lower == "plugin") {
        out_type = PackageType::Plugin;
        return true;
    }
    if (lower == "widget") {
        out_type = PackageType::Widget;
        return true;
    }
    if (lower == "asset" || lower == "bundle") {
        out_type = PackageType::Asset;
        return true;
    }
    return false;
}

const char* package_type_extension(PackageType type) noexcept {
    switch (type) {
        case PackageType::World:  return ".world.json";
        case PackageType::Layer:  return ".layer.json";
        case PackageType::Plugin: return ".plugin.json";
        case PackageType::Widget: return ".widget.json";
        case PackageType::Asset:  return ".bundle.json";
        default:                  return ".json";
    }
}

const char* package_status_to_string(PackageStatus status) noexcept {
    switch (status) {
        case PackageStatus::Available:  return "Available";
        case PackageStatus::Loading:    return "Loading";
        case PackageStatus::Loaded:     return "Loaded";
        case PackageStatus::Unloading:  return "Unloading";
        case PackageStatus::Failed:     return "Failed";
        default:                        return "Unknown";
    }
}

} // namespace void_package
