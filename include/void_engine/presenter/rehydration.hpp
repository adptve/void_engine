#pragma once

/// @file rehydration.hpp
/// @brief Rehydration support for hot-swap
///
/// Enables state restoration without restart, supporting hot-reload scenarios.

#include "fwd.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace void_presenter {

// =============================================================================
// Rehydration Error
// =============================================================================

/// Rehydration error types
enum class RehydrationErrorKind {
    MissingField,       ///< Required field missing
    InvalidData,        ///< Data is invalid
    VersionMismatch,    ///< Version mismatch
    SerializationError, ///< Serialization failed
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

} // namespace void_presenter
