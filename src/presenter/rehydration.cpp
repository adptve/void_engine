/// @file rehydration.cpp
/// @brief Rehydration (hot-reload state preservation) implementation for void_presenter

#include <void_engine/presenter/rehydration.hpp>
#include <void_engine/presenter/backend.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace void_presenter {

// =============================================================================
// RehydrationState Utilities
// =============================================================================

std::string format_rehydration_state(const RehydrationState& state) {
    std::string result = "RehydrationState {\n";

    // Count entries of each type
    std::size_t string_count = 0;
    std::size_t int_count = 0;
    std::size_t float_count = 0;
    std::size_t bool_count = 0;
    std::size_t binary_count = 0;
    std::size_t nested_count = 0;

    // We can check for existence of known keys
    // The actual enumeration would require access to internal maps

    result += "  (state contents not enumerable)\n";
    result += "}";
    return result;
}

bool validate_rehydration_state(const RehydrationState& state, const std::vector<std::string>& required_keys) {
    // Check if all required string keys exist
    for (const auto& key : required_keys) {
        if (!state.get_string(key)) {
            return false;
        }
    }
    return true;
}

RehydrationState merge_rehydration_states(const RehydrationState& base, const RehydrationState& overlay) {
    // Create a new state starting with base
    // In a full implementation, we would iterate over all entries in overlay
    // and add/overwrite them in the result

    // For now, return base (overlay would need iteration support)
    RehydrationState result;

    // Since RehydrationState doesn't expose iteration, we return empty state
    // In production, the class would need to be extended with iteration support

    return result;
}

// =============================================================================
// RehydrationStore Utilities
// =============================================================================

std::string format_rehydration_store(const RehydrationStore& store) {
    std::string result = "RehydrationStore {\n";

    result += "  components: " + std::to_string(store.size()) + "\n";

    auto keys = store.keys();
    if (!keys.empty()) {
        result += "  keys: [";
        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (i > 0) result += ", ";
            result += keys[i];
        }
        result += "]\n";
    }

    result += "}";
    return result;
}

// =============================================================================
// Hot-Reload Helper Functions
// =============================================================================

namespace {

/// Validate that a state can be used for rehydration
bool is_state_valid_for_rehydration(const RehydrationState& state) {
    // Check for corruption markers or version mismatches
    auto version = state.get_int("__version");
    if (!version) {
        return true; // No version = assume valid (legacy)
    }

    // Check version compatibility
    // For now, accept all versions
    return true;
}

/// Calculate a hash of the state for change detection
std::uint64_t calculate_state_hash(const RehydrationState& state) {
    // Simple hash based on what we can access
    // In production, this would hash all values

    std::uint64_t hash = 0;

    // Use FNV-1a style mixing for any available data
    auto mix = [&hash](std::uint64_t value) {
        hash ^= value;
        hash *= 0x100000001b3ULL;
    };

    // Try to hash known common keys
    if (auto v = state.get_int("width")) mix(static_cast<std::uint64_t>(*v));
    if (auto v = state.get_int("height")) mix(static_cast<std::uint64_t>(*v));
    if (auto v = state.get_int("format")) mix(static_cast<std::uint64_t>(*v));

    return hash;
}

} // anonymous namespace

// =============================================================================
// Presenter State Serialization
// =============================================================================

/// Serialize presenter configuration to RehydrationState
RehydrationState serialize_presenter_config(
    const PresenterConfig& config,
    const std::string& prefix) {

    RehydrationState state;

    state.set_int(prefix + "width", config.width);
    state.set_int(prefix + "height", config.height);
    state.set_int(prefix + "format", static_cast<std::int64_t>(config.format));
    state.set_int(prefix + "present_mode", static_cast<std::int64_t>(config.present_mode));
    state.set_bool(prefix + "enable_hdr", config.enable_hdr);
    state.set_int(prefix + "target_frame_rate", config.target_frame_rate);
    state.set_bool(prefix + "allow_tearing", config.allow_tearing);

    return state;
}

/// Deserialize presenter configuration from RehydrationState
PresenterConfig deserialize_presenter_config(
    const RehydrationState& state,
    const std::string& prefix) {

    PresenterConfig config;

    if (auto v = state.get_int(prefix + "width")) {
        config.width = static_cast<std::uint32_t>(*v);
    }
    if (auto v = state.get_int(prefix + "height")) {
        config.height = static_cast<std::uint32_t>(*v);
    }
    if (auto v = state.get_int(prefix + "format")) {
        config.format = static_cast<SurfaceFormat>(*v);
    }
    if (auto v = state.get_int(prefix + "present_mode")) {
        config.present_mode = static_cast<PresentMode>(*v);
    }
    if (auto v = state.get_bool(prefix + "enable_hdr")) {
        config.enable_hdr = *v;
    }
    if (auto v = state.get_int(prefix + "target_frame_rate")) {
        config.target_frame_rate = static_cast<std::uint32_t>(*v);
    }
    if (auto v = state.get_bool(prefix + "allow_tearing")) {
        config.allow_tearing = *v;
    }

    return config;
}

/// Serialize swapchain configuration to RehydrationState
RehydrationState serialize_swapchain_config(
    const SwapchainConfig& config,
    const std::string& prefix) {

    RehydrationState state;

    state.set_int(prefix + "width", config.width);
    state.set_int(prefix + "height", config.height);
    state.set_int(prefix + "format", static_cast<std::int64_t>(config.format));
    state.set_int(prefix + "present_mode", static_cast<std::int64_t>(config.present_mode));
    state.set_int(prefix + "alpha_mode", static_cast<std::int64_t>(config.alpha_mode));
    state.set_int(prefix + "image_count", config.image_count);
    state.set_bool(prefix + "enable_hdr", config.enable_hdr);

    return state;
}

/// Deserialize swapchain configuration from RehydrationState
SwapchainConfig deserialize_swapchain_config(
    const RehydrationState& state,
    const std::string& prefix) {

    SwapchainConfig config;

    if (auto v = state.get_int(prefix + "width")) {
        config.width = static_cast<std::uint32_t>(*v);
    }
    if (auto v = state.get_int(prefix + "height")) {
        config.height = static_cast<std::uint32_t>(*v);
    }
    if (auto v = state.get_int(prefix + "format")) {
        config.format = static_cast<SurfaceFormat>(*v);
    }
    if (auto v = state.get_int(prefix + "present_mode")) {
        config.present_mode = static_cast<PresentMode>(*v);
    }
    if (auto v = state.get_int(prefix + "alpha_mode")) {
        config.alpha_mode = static_cast<AlphaMode>(*v);
    }
    if (auto v = state.get_int(prefix + "image_count")) {
        config.image_count = static_cast<std::uint32_t>(*v);
    }
    if (auto v = state.get_bool(prefix + "enable_hdr")) {
        config.enable_hdr = *v;
    }

    return config;
}

/// Serialize backend configuration to RehydrationState
RehydrationState serialize_backend_config(
    const BackendConfig& config,
    const std::string& prefix) {

    RehydrationState state;

    state.set_int(prefix + "preferred_type", static_cast<std::int64_t>(config.preferred_type));
    state.set_int(prefix + "power_preference", static_cast<std::int64_t>(config.power_preference));
    state.set_bool(prefix + "enable_validation", config.enable_validation);
    state.set_bool(prefix + "enable_gpu_timing", config.enable_gpu_timing);
    state.set_bool(prefix + "allow_software_fallback", config.allow_software_fallback);

    // Serialize fallback types
    std::string fallbacks;
    for (std::size_t i = 0; i < config.fallback_types.size(); ++i) {
        if (i > 0) fallbacks += ",";
        fallbacks += std::to_string(static_cast<int>(config.fallback_types[i]));
    }
    state.set_string(prefix + "fallback_types", fallbacks);

    return state;
}

/// Deserialize backend configuration from RehydrationState
BackendConfig deserialize_backend_config(
    const RehydrationState& state,
    const std::string& prefix) {

    BackendConfig config;

    if (auto v = state.get_int(prefix + "preferred_type")) {
        config.preferred_type = static_cast<BackendType>(*v);
    }
    if (auto v = state.get_int(prefix + "power_preference")) {
        config.power_preference = static_cast<PowerPreference>(*v);
    }
    if (auto v = state.get_bool(prefix + "enable_validation")) {
        config.enable_validation = *v;
    }
    if (auto v = state.get_bool(prefix + "enable_gpu_timing")) {
        config.enable_gpu_timing = *v;
    }
    if (auto v = state.get_bool(prefix + "allow_software_fallback")) {
        config.allow_software_fallback = *v;
    }

    // Deserialize fallback types
    if (auto fallbacks_str = state.get_string(prefix + "fallback_types")) {
        config.fallback_types.clear();
        std::string s = *fallbacks_str;
        std::size_t pos = 0;
        while ((pos = s.find(',')) != std::string::npos || !s.empty()) {
            std::string token;
            if (pos != std::string::npos) {
                token = s.substr(0, pos);
                s.erase(0, pos + 1);
            } else {
                token = s;
                s.clear();
            }
            if (!token.empty()) {
                config.fallback_types.push_back(static_cast<BackendType>(std::stoi(token)));
            }
        }
    }

    return config;
}

// =============================================================================
// Hot-Reload Lifecycle
// =============================================================================

/// Context for managing hot-reload state during a reload cycle
class HotReloadContext {
public:
    HotReloadContext() = default;

    /// Begin a hot-reload cycle
    void begin_reload() {
        m_in_reload = true;
        m_reload_start = std::chrono::steady_clock::now();
        m_dehydrated_components.clear();
    }

    /// End a hot-reload cycle
    void end_reload() {
        m_in_reload = false;
        m_reload_end = std::chrono::steady_clock::now();
    }

    /// Check if currently in a reload cycle
    [[nodiscard]] bool in_reload() const { return m_in_reload; }

    /// Get reload duration (if completed)
    [[nodiscard]] std::chrono::nanoseconds reload_duration() const {
        if (m_in_reload) {
            return std::chrono::steady_clock::now() - m_reload_start;
        }
        return m_reload_end - m_reload_start;
    }

    /// Register a dehydrated component
    void register_dehydrated(const std::string& name, RehydrationState state) {
        m_dehydrated_components[name] = std::move(state);
    }

    /// Get a dehydrated component's state
    [[nodiscard]] const RehydrationState* get_dehydrated(const std::string& name) const {
        auto it = m_dehydrated_components.find(name);
        if (it != m_dehydrated_components.end()) {
            return &it->second;
        }
        return nullptr;
    }

    /// Get all dehydrated component names
    [[nodiscard]] std::vector<std::string> dehydrated_names() const {
        std::vector<std::string> names;
        names.reserve(m_dehydrated_components.size());
        for (const auto& [name, _] : m_dehydrated_components) {
            names.push_back(name);
        }
        return names;
    }

private:
    bool m_in_reload = false;
    std::chrono::steady_clock::time_point m_reload_start;
    std::chrono::steady_clock::time_point m_reload_end;
    std::unordered_map<std::string, RehydrationState> m_dehydrated_components;
};

// =============================================================================
// State Validation
// =============================================================================

/// Validate that saved state matches current configuration constraints
struct StateValidationResult {
    bool valid = true;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;

    [[nodiscard]] bool has_errors() const { return !errors.empty(); }
    [[nodiscard]] bool has_warnings() const { return !warnings.empty(); }
};

StateValidationResult validate_state_for_reload(
    const RehydrationState& state,
    const BackendCapabilities& current_caps) {

    StateValidationResult result;

    // Check format compatibility
    if (auto format_val = state.get_int("format")) {
        auto format = static_cast<SurfaceFormat>(*format_val);
        bool found = false;
        for (const auto& f : current_caps.supported_formats) {
            if (f == format) {
                found = true;
                break;
            }
        }
        if (!found) {
            result.warnings.push_back("Saved format not supported by current backend");
        }
    }

    // Check present mode compatibility
    if (auto mode_val = state.get_int("present_mode")) {
        auto mode = static_cast<PresentMode>(*mode_val);
        bool found = false;
        for (const auto& m : current_caps.supported_present_modes) {
            if (m == mode) {
                found = true;
                break;
            }
        }
        if (!found) {
            result.warnings.push_back("Saved present mode not supported by current backend");
        }
    }

    // Check dimensions
    if (auto w = state.get_int("width")) {
        if (static_cast<std::uint32_t>(*w) > current_caps.limits.max_texture_dimension_2d) {
            result.errors.push_back("Saved width exceeds maximum texture dimension");
            result.valid = false;
        }
    }
    if (auto h = state.get_int("height")) {
        if (static_cast<std::uint32_t>(*h) > current_caps.limits.max_texture_dimension_2d) {
            result.errors.push_back("Saved height exceeds maximum texture dimension");
            result.valid = false;
        }
    }

    return result;
}

std::string format_validation_result(const StateValidationResult& result) {
    std::string output = "StateValidationResult {\n";

    output += "  valid: " + std::string(result.valid ? "true" : "false") + "\n";

    if (!result.warnings.empty()) {
        output += "  warnings:\n";
        for (const auto& w : result.warnings) {
            output += "    - " + w + "\n";
        }
    }

    if (!result.errors.empty()) {
        output += "  errors:\n";
        for (const auto& e : result.errors) {
            output += "    - " + e + "\n";
        }
    }

    output += "}";
    return output;
}

// =============================================================================
// Debug Utilities
// =============================================================================

namespace debug {

std::string dump_rehydration_state(const RehydrationState& state) {
    // Attempt to dump known keys
    std::string result = "RehydrationState dump:\n";

    // Try common keys
    const char* common_keys[] = {
        "width", "height", "format", "present_mode", "alpha_mode",
        "image_count", "enable_hdr", "target_frame_rate", "allow_tearing",
        "backend_type", "power_preference", "enable_validation"
    };

    for (const char* key : common_keys) {
        if (auto v = state.get_int(key)) {
            result += "  " + std::string(key) + " (int): " + std::to_string(*v) + "\n";
        } else if (auto v = state.get_bool(key)) {
            result += "  " + std::string(key) + " (bool): " + std::string(*v ? "true" : "false") + "\n";
        } else if (auto v = state.get_string(key)) {
            result += "  " + std::string(key) + " (string): " + *v + "\n";
        } else if (auto v = state.get_float(key)) {
            result += "  " + std::string(key) + " (float): " + std::to_string(*v) + "\n";
        }
    }

    return result;
}

} // namespace debug

} // namespace void_presenter
