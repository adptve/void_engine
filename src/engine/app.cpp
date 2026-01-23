/// @file app.cpp
/// @brief Application interface implementation for void_engine

#include <void_engine/engine/app.hpp>

namespace void_engine {

// =============================================================================
// AppConfig
// =============================================================================

AppConfig AppConfig::from_name(const std::string& name) {
    AppConfig config;
    config.name = name;
    config.version = "0.1.0";
    config.required_features = EngineFeature::Minimal;
    return config;
}

} // namespace void_engine
