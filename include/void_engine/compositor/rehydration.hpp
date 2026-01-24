#pragma once

/// @file rehydration.hpp
/// @brief Rehydration support for hot-swap
///
/// Enables state restoration without restart, supporting hot-reload scenarios.
/// Maintains frame scheduler state, VRR/HDR configurations, and output settings
/// across compositor restarts.

#include "fwd.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_compositor {

// =============================================================================
// Rehydration Error
// =============================================================================

/// Rehydration error types
enum class RehydrationErrorKind {
    MissingField,       ///< Required field missing
    InvalidData,        ///< Data is invalid
    VersionMismatch,    ///< Version mismatch
    SerializationError, ///< Serialization failed
    BackendMismatch,    ///< Backend type mismatch
    OutputMismatch,     ///< Output configuration mismatch
};

/// Rehydration error
struct RehydrationError {
    RehydrationErrorKind kind;
    std::string message;

    [[nodiscard]] static RehydrationError missing_field(const std::string& field) {
        return {RehydrationErrorKind::MissingField, "Missing required field: " + field};
    }

    [[nodiscard]] static RehydrationError invalid_data(const std::string& msg) {
        return {RehydrationErrorKind::InvalidData, "Invalid data: " + msg};
    }

    [[nodiscard]] static RehydrationError version_mismatch(
        const std::string& expected, const std::string& actual) {
        return {RehydrationErrorKind::VersionMismatch,
                "Version mismatch: expected " + expected + ", got " + actual};
    }

    [[nodiscard]] static RehydrationError serialization_error(const std::string& msg) {
        return {RehydrationErrorKind::SerializationError, "Serialization error: " + msg};
    }

    [[nodiscard]] static RehydrationError backend_mismatch(const std::string& msg) {
        return {RehydrationErrorKind::BackendMismatch, "Backend mismatch: " + msg};
    }

    [[nodiscard]] static RehydrationError output_mismatch(const std::string& msg) {
        return {RehydrationErrorKind::OutputMismatch, "Output mismatch: " + msg};
    }
};

// =============================================================================
// Rehydration State
// =============================================================================

/// Rehydration state container
/// Stores typed values for state persistence across hot-reloads.
class RehydrationState {
public:
    RehydrationState() = default;

    // =========================================================================
    // String Values
    // =========================================================================

    /// Set string value
    void set_string(const std::string& key, const std::string& value) {
        m_string_values[key] = value;
    }

    /// Get string value
    [[nodiscard]] std::optional<std::string> get_string(const std::string& key) const {
        auto it = m_string_values.find(key);
        if (it != m_string_values.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /// Builder pattern
    [[nodiscard]] RehydrationState& with_string(const std::string& key, const std::string& value) {
        set_string(key, value);
        return *this;
    }

    // =========================================================================
    // Integer Values
    // =========================================================================

    /// Set int value
    void set_int(const std::string& key, std::int64_t value) {
        m_int_values[key] = value;
    }

    /// Get int value
    [[nodiscard]] std::optional<std::int64_t> get_int(const std::string& key) const {
        auto it = m_int_values.find(key);
        if (it != m_int_values.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /// Get uint value
    [[nodiscard]] std::optional<std::uint64_t> get_uint(const std::string& key) const {
        auto v = get_int(key);
        if (v) return static_cast<std::uint64_t>(*v);
        return std::nullopt;
    }

    /// Get uint32 value
    [[nodiscard]] std::optional<std::uint32_t> get_u32(const std::string& key) const {
        auto v = get_int(key);
        if (v) return static_cast<std::uint32_t>(*v);
        return std::nullopt;
    }

    /// Builder pattern
    [[nodiscard]] RehydrationState& with_int(const std::string& key, std::int64_t value) {
        set_int(key, value);
        return *this;
    }

    /// Builder pattern (uint)
    [[nodiscard]] RehydrationState& with_uint(const std::string& key, std::uint64_t value) {
        set_int(key, static_cast<std::int64_t>(value));
        return *this;
    }

    /// Builder pattern (uint32)
    [[nodiscard]] RehydrationState& with_u32(const std::string& key, std::uint32_t value) {
        set_int(key, static_cast<std::int64_t>(value));
        return *this;
    }

    // =========================================================================
    // Float Values
    // =========================================================================

    /// Set float value
    void set_float(const std::string& key, double value) {
        m_float_values[key] = value;
    }

    /// Get float value
    [[nodiscard]] std::optional<double> get_float(const std::string& key) const {
        auto it = m_float_values.find(key);
        if (it != m_float_values.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /// Builder pattern
    [[nodiscard]] RehydrationState& with_float(const std::string& key, double value) {
        set_float(key, value);
        return *this;
    }

    // =========================================================================
    // Boolean Values
    // =========================================================================

    /// Set bool value
    void set_bool(const std::string& key, bool value) {
        m_bool_values[key] = value;
    }

    /// Get bool value
    [[nodiscard]] std::optional<bool> get_bool(const std::string& key) const {
        auto it = m_bool_values.find(key);
        if (it != m_bool_values.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /// Builder pattern
    [[nodiscard]] RehydrationState& with_bool(const std::string& key, bool value) {
        set_bool(key, value);
        return *this;
    }

    // =========================================================================
    // Binary Values
    // =========================================================================

    /// Set binary value
    void set_binary(const std::string& key, std::vector<std::uint8_t> value) {
        m_binary_values[key] = std::move(value);
    }

    /// Get binary value
    [[nodiscard]] const std::vector<std::uint8_t>* get_binary(const std::string& key) const {
        auto it = m_binary_values.find(key);
        if (it != m_binary_values.end()) {
            return &it->second;
        }
        return nullptr;
    }

    /// Builder pattern
    [[nodiscard]] RehydrationState& with_binary(const std::string& key, std::vector<std::uint8_t> value) {
        set_binary(key, std::move(value));
        return *this;
    }

    // =========================================================================
    // Nested States
    // =========================================================================

    /// Set nested state
    void set_nested(const std::string& key, RehydrationState state) {
        m_nested_states[key] = std::move(state);
    }

    /// Get nested state
    [[nodiscard]] const RehydrationState* get_nested(const std::string& key) const {
        auto it = m_nested_states.find(key);
        if (it != m_nested_states.end()) {
            return &it->second;
        }
        return nullptr;
    }

    /// Get nested state (mutable)
    [[nodiscard]] RehydrationState* get_nested_mut(const std::string& key) {
        auto it = m_nested_states.find(key);
        if (it != m_nested_states.end()) {
            return &it->second;
        }
        return nullptr;
    }

    /// Builder pattern
    [[nodiscard]] RehydrationState& with_nested(const std::string& key, RehydrationState state) {
        set_nested(key, std::move(state));
        return *this;
    }

    // =========================================================================
    // Utility
    // =========================================================================

    /// Check if empty
    [[nodiscard]] bool is_empty() const {
        return m_string_values.empty() &&
               m_int_values.empty() &&
               m_float_values.empty() &&
               m_bool_values.empty() &&
               m_binary_values.empty() &&
               m_nested_states.empty();
    }

    /// Clear all values
    void clear() {
        m_string_values.clear();
        m_int_values.clear();
        m_float_values.clear();
        m_bool_values.clear();
        m_binary_values.clear();
        m_nested_states.clear();
    }

    /// Merge another state into this one
    void merge(const RehydrationState& other) {
        for (const auto& [k, v] : other.m_string_values) {
            m_string_values[k] = v;
        }
        for (const auto& [k, v] : other.m_int_values) {
            m_int_values[k] = v;
        }
        for (const auto& [k, v] : other.m_float_values) {
            m_float_values[k] = v;
        }
        for (const auto& [k, v] : other.m_bool_values) {
            m_bool_values[k] = v;
        }
        for (const auto& [k, v] : other.m_binary_values) {
            m_binary_values[k] = v;
        }
        for (const auto& [k, v] : other.m_nested_states) {
            m_nested_states[k] = v;
        }
    }

    /// Get number of values stored
    [[nodiscard]] std::size_t count() const {
        return m_string_values.size() +
               m_int_values.size() +
               m_float_values.size() +
               m_bool_values.size() +
               m_binary_values.size() +
               m_nested_states.size();
    }

private:
    std::unordered_map<std::string, std::string> m_string_values;
    std::unordered_map<std::string, std::int64_t> m_int_values;
    std::unordered_map<std::string, double> m_float_values;
    std::unordered_map<std::string, bool> m_bool_values;
    std::unordered_map<std::string, std::vector<std::uint8_t>> m_binary_values;
    std::unordered_map<std::string, RehydrationState> m_nested_states;
};

// =============================================================================
// Rehydratable Interface
// =============================================================================

/// Interface for types that can be rehydrated
class IRehydratable {
public:
    virtual ~IRehydratable() = default;

    /// Get current state for rehydration (dehydrate)
    [[nodiscard]] virtual RehydrationState dehydrate() const = 0;

    /// Restore from rehydration state
    /// @return true on success
    virtual bool rehydrate(const RehydrationState& state) = 0;
};

// =============================================================================
// Rehydration Store
// =============================================================================

/// Thread-safe store for managing multiple rehydration states
class RehydrationStore {
public:
    RehydrationStore() = default;

    /// Store state
    void store(const std::string& key, RehydrationState state) {
        std::unique_lock lock(m_mutex);
        m_states[key] = std::move(state);
    }

    /// Retrieve state (copy)
    [[nodiscard]] std::optional<RehydrationState> retrieve(const std::string& key) const {
        std::shared_lock lock(m_mutex);
        auto it = m_states.find(key);
        if (it != m_states.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /// Remove state
    [[nodiscard]] std::optional<RehydrationState> remove(const std::string& key) {
        std::unique_lock lock(m_mutex);
        auto it = m_states.find(key);
        if (it != m_states.end()) {
            auto state = std::move(it->second);
            m_states.erase(it);
            return state;
        }
        return std::nullopt;
    }

    /// Check if state exists
    [[nodiscard]] bool contains(const std::string& key) const {
        std::shared_lock lock(m_mutex);
        return m_states.count(key) > 0;
    }

    /// Get all keys
    [[nodiscard]] std::vector<std::string> keys() const {
        std::shared_lock lock(m_mutex);
        std::vector<std::string> result;
        result.reserve(m_states.size());
        for (const auto& [k, _] : m_states) {
            result.push_back(k);
        }
        return result;
    }

    /// Get state count
    [[nodiscard]] std::size_t size() const {
        std::shared_lock lock(m_mutex);
        return m_states.size();
    }

    /// Clear all states
    void clear() {
        std::unique_lock lock(m_mutex);
        m_states.clear();
    }

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, RehydrationState> m_states;
};

// =============================================================================
// VRR State Serialization
// =============================================================================

/// Serialize VRR configuration to rehydration state
inline RehydrationState serialize_vrr_config(const struct VrrConfig& config) {
    RehydrationState state;
    state.set_bool("enabled", config.enabled);
    state.set_int("min_refresh_rate", config.min_refresh_rate);
    state.set_int("max_refresh_rate", config.max_refresh_rate);
    state.set_int("current_refresh_rate", config.current_refresh_rate);
    state.set_int("mode", static_cast<std::int64_t>(config.mode));
    return state;
}

/// Deserialize VRR configuration from rehydration state
inline bool deserialize_vrr_config(const RehydrationState& state, struct VrrConfig& config) {
    auto enabled = state.get_bool("enabled");
    auto min_rate = state.get_u32("min_refresh_rate");
    auto max_rate = state.get_u32("max_refresh_rate");
    auto current_rate = state.get_u32("current_refresh_rate");
    auto mode = state.get_int("mode");

    if (!enabled || !min_rate || !max_rate || !current_rate || !mode) {
        return false;
    }

    config.enabled = *enabled;
    config.min_refresh_rate = *min_rate;
    config.max_refresh_rate = *max_rate;
    config.current_refresh_rate = *current_rate;
    config.mode = static_cast<VrrMode>(*mode);
    return true;
}

// =============================================================================
// HDR State Serialization
// =============================================================================

/// Serialize HDR configuration to rehydration state
inline RehydrationState serialize_hdr_config(const struct HdrConfig& config) {
    RehydrationState state;
    state.set_bool("enabled", config.enabled);
    state.set_int("transfer_function", static_cast<std::int64_t>(config.transfer_function));
    state.set_int("color_primaries", static_cast<std::int64_t>(config.color_primaries));
    state.set_int("max_luminance", config.max_luminance);
    state.set_float("min_luminance", config.min_luminance);
    if (config.max_content_light_level) {
        state.set_int("max_cll", *config.max_content_light_level);
    }
    if (config.max_frame_average_light_level) {
        state.set_int("max_fall", *config.max_frame_average_light_level);
    }
    return state;
}

/// Deserialize HDR configuration from rehydration state
inline bool deserialize_hdr_config(const RehydrationState& state, struct HdrConfig& config) {
    auto enabled = state.get_bool("enabled");
    auto tf = state.get_int("transfer_function");
    auto cp = state.get_int("color_primaries");
    auto max_lum = state.get_u32("max_luminance");
    auto min_lum = state.get_float("min_luminance");

    if (!enabled || !tf || !cp || !max_lum || !min_lum) {
        return false;
    }

    config.enabled = *enabled;
    config.transfer_function = static_cast<TransferFunction>(*tf);
    config.color_primaries = static_cast<ColorPrimaries>(*cp);
    config.max_luminance = *max_lum;
    config.min_luminance = static_cast<float>(*min_lum);

    if (auto cll = state.get_u32("max_cll")) {
        config.max_content_light_level = *cll;
    }
    if (auto fall = state.get_u32("max_fall")) {
        config.max_frame_average_light_level = *fall;
    }

    return true;
}

// =============================================================================
// Frame Scheduler State Serialization
// =============================================================================

/// Serialize frame scheduler state to rehydration state
inline RehydrationState serialize_frame_scheduler(const struct FrameScheduler& scheduler) {
    RehydrationState state;
    state.set_int("target_fps", scheduler.target_fps());
    state.set_uint("frame_number", scheduler.frame_number());
    state.set_uint("dropped_count", scheduler.dropped_frame_count());
    state.set_float("content_velocity", scheduler.content_velocity());

    if (scheduler.vrr_config()) {
        state.set_nested("vrr_config", serialize_vrr_config(*scheduler.vrr_config()));
    }

    return state;
}

} // namespace void_compositor
