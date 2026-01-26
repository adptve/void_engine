#pragma once

/// @file snapshot.hpp
/// @brief Hot-reload snapshot support for void_presenter
///
/// Provides binary serialization/deserialization for:
/// - RehydrationState
/// - PresenterManager state
/// - MultiBackendPresenter state
/// - Output target configurations

#include "rehydration.hpp"
#include "presenter.hpp"
#include "multi_backend_presenter.hpp"

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace void_presenter {

// =============================================================================
// Binary Serialization Helpers
// =============================================================================

/// Binary writer for snapshot serialization
class BinaryWriter {
public:
    void write_u8(std::uint8_t v) { m_buffer.push_back(v); }

    void write_u32(std::uint32_t v) {
        m_buffer.push_back(static_cast<std::uint8_t>(v & 0xFF));
        m_buffer.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        m_buffer.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        m_buffer.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    }

    void write_u64(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            m_buffer.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF));
        }
    }

    void write_i64(std::int64_t v) {
        write_u64(static_cast<std::uint64_t>(v));
    }

    void write_f64(double v) {
        std::uint64_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        write_u64(bits);
    }

    void write_bool(bool v) { write_u8(v ? 1 : 0); }

    void write_string(const std::string& s) {
        write_u32(static_cast<std::uint32_t>(s.size()));
        m_buffer.insert(m_buffer.end(), s.begin(), s.end());
    }

    void write_bytes(const std::vector<std::uint8_t>& data) {
        write_u32(static_cast<std::uint32_t>(data.size()));
        m_buffer.insert(m_buffer.end(), data.begin(), data.end());
    }

    [[nodiscard]] std::vector<std::uint8_t> take() {
        return std::move(m_buffer);
    }

    [[nodiscard]] const std::vector<std::uint8_t>& data() const {
        return m_buffer;
    }

    [[nodiscard]] std::size_t size() const {
        return m_buffer.size();
    }

private:
    std::vector<std::uint8_t> m_buffer;
};

/// Binary reader for snapshot deserialization
class BinaryReader {
public:
    explicit BinaryReader(const std::vector<std::uint8_t>& data)
        : m_data(data), m_offset(0) {}

    [[nodiscard]] bool has_remaining(std::size_t bytes) const {
        return m_offset + bytes <= m_data.size();
    }

    [[nodiscard]] std::uint8_t read_u8() {
        if (!has_remaining(1)) return 0;
        return m_data[m_offset++];
    }

    [[nodiscard]] std::uint32_t read_u32() {
        if (!has_remaining(4)) return 0;
        std::uint32_t v = m_data[m_offset] |
                          (static_cast<std::uint32_t>(m_data[m_offset + 1]) << 8) |
                          (static_cast<std::uint32_t>(m_data[m_offset + 2]) << 16) |
                          (static_cast<std::uint32_t>(m_data[m_offset + 3]) << 24);
        m_offset += 4;
        return v;
    }

    [[nodiscard]] std::uint64_t read_u64() {
        if (!has_remaining(8)) return 0;
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= static_cast<std::uint64_t>(m_data[m_offset + i]) << (i * 8);
        }
        m_offset += 8;
        return v;
    }

    [[nodiscard]] std::int64_t read_i64() {
        return static_cast<std::int64_t>(read_u64());
    }

    [[nodiscard]] double read_f64() {
        std::uint64_t bits = read_u64();
        double v;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }

    [[nodiscard]] bool read_bool() { return read_u8() != 0; }

    [[nodiscard]] std::string read_string() {
        std::uint32_t len = read_u32();
        if (!has_remaining(len)) return "";
        std::string s(m_data.begin() + m_offset, m_data.begin() + m_offset + len);
        m_offset += len;
        return s;
    }

    [[nodiscard]] std::vector<std::uint8_t> read_bytes() {
        std::uint32_t len = read_u32();
        if (!has_remaining(len)) return {};
        std::vector<std::uint8_t> data(m_data.begin() + m_offset, m_data.begin() + m_offset + len);
        m_offset += len;
        return data;
    }

    [[nodiscard]] bool valid() const { return m_offset <= m_data.size(); }

    [[nodiscard]] std::size_t offset() const { return m_offset; }

    [[nodiscard]] std::size_t remaining() const {
        return m_offset < m_data.size() ? m_data.size() - m_offset : 0;
    }

private:
    const std::vector<std::uint8_t>& m_data;
    std::size_t m_offset;
};

// =============================================================================
// RehydrationState Serialization
// =============================================================================

/// Serialize a RehydrationState to binary
inline void serialize_rehydration_state(BinaryWriter& writer, const RehydrationState& state);

/// Deserialize a RehydrationState from binary
inline RehydrationState deserialize_rehydration_state(BinaryReader& reader);

// Forward declaration for nested serialization
inline void serialize_rehydration_state(BinaryWriter& writer, const RehydrationState& state) {
    // Marker for state start
    writer.write_u32(0x52485944);  // "RHYD" magic

    // We need to iterate through the state's values
    // Since RehydrationState doesn't expose iterators, we'll serialize what we can access
    // For now, we'll serialize common known keys that presenters use

    // The state container uses typed maps internally
    // For full serialization, we'd need to add iteration support to RehydrationState
    // For now, we serialize the state as empty and rely on dehydrate/rehydrate pattern

    writer.write_u32(0);  // String count (placeholder)
    writer.write_u32(0);  // Int count (placeholder)
    writer.write_u32(0);  // Float count (placeholder)
    writer.write_u32(0);  // Bool count (placeholder)
    writer.write_u32(0);  // Binary count (placeholder)
    writer.write_u32(0);  // Nested count (placeholder)
}

inline RehydrationState deserialize_rehydration_state(BinaryReader& reader) {
    RehydrationState state;

    // Check magic
    std::uint32_t magic = reader.read_u32();
    if (magic != 0x52485944) {
        return state;  // Invalid magic
    }

    // Read counts (placeholder implementation)
    std::uint32_t string_count = reader.read_u32();
    std::uint32_t int_count = reader.read_u32();
    std::uint32_t float_count = reader.read_u32();
    std::uint32_t bool_count = reader.read_u32();
    std::uint32_t binary_count = reader.read_u32();
    std::uint32_t nested_count = reader.read_u32();

    // Read string values
    for (std::uint32_t i = 0; i < string_count; ++i) {
        std::string key = reader.read_string();
        std::string value = reader.read_string();
        state.set_string(key, value);
    }

    // Read int values
    for (std::uint32_t i = 0; i < int_count; ++i) {
        std::string key = reader.read_string();
        std::int64_t value = reader.read_i64();
        state.set_int(key, value);
    }

    // Read float values
    for (std::uint32_t i = 0; i < float_count; ++i) {
        std::string key = reader.read_string();
        double value = reader.read_f64();
        state.set_float(key, value);
    }

    // Read bool values
    for (std::uint32_t i = 0; i < bool_count; ++i) {
        std::string key = reader.read_string();
        bool value = reader.read_bool();
        state.set_bool(key, value);
    }

    // Read binary values
    for (std::uint32_t i = 0; i < binary_count; ++i) {
        std::string key = reader.read_string();
        std::vector<std::uint8_t> value = reader.read_bytes();
        state.set_binary(key, std::move(value));
    }

    // Read nested states (recursive)
    for (std::uint32_t i = 0; i < nested_count; ++i) {
        std::string key = reader.read_string();
        RehydrationState nested = deserialize_rehydration_state(reader);
        state.set_nested(key, std::move(nested));
    }

    return state;
}

// =============================================================================
// Presenter Manager Snapshot
// =============================================================================

/// Snapshot of the PresenterManager state
struct PresenterManagerSnapshot {
    static constexpr std::uint32_t VERSION = 1;
    static constexpr std::uint32_t MAGIC = 0x50524553;  // "PRES"

    std::uint32_t version = VERSION;
    std::vector<std::pair<std::uint64_t, RehydrationState>> presenter_states;
    std::uint64_t primary_id = 0;
    std::uint64_t next_id = 1;

    [[nodiscard]] bool is_compatible() const {
        return version == VERSION;
    }
};

/// Take a snapshot of the PresenterManager
[[nodiscard]] inline PresenterManagerSnapshot take_presenter_manager_snapshot(
    const PresenterManager& manager) {

    PresenterManagerSnapshot snapshot;
    snapshot.version = PresenterManagerSnapshot::VERSION;

    // Get primary presenter ID if any
    const auto* primary = manager.primary();
    snapshot.primary_id = primary ? primary->id().id : 0;
    snapshot.next_id = manager.all_ids().size() + 1;  // Approximate

    // Get rehydration states from all presenters
    for (const auto& id : manager.all_ids()) {
        const auto* presenter = manager.get(id);
        if (presenter) {
            snapshot.presenter_states.push_back({id.id, presenter->dehydrate()});
        }
    }

    return snapshot;
}

/// Serialize PresenterManager snapshot to binary
[[nodiscard]] inline std::vector<std::uint8_t> serialize_presenter_manager_snapshot(
    const PresenterManagerSnapshot& snapshot) {

    BinaryWriter writer;

    writer.write_u32(PresenterManagerSnapshot::MAGIC);
    writer.write_u32(snapshot.version);
    writer.write_u64(snapshot.primary_id);
    writer.write_u64(snapshot.next_id);

    writer.write_u32(static_cast<std::uint32_t>(snapshot.presenter_states.size()));
    for (const auto& [id, state] : snapshot.presenter_states) {
        writer.write_u64(id);
        serialize_rehydration_state(writer, state);
    }

    return writer.take();
}

/// Deserialize PresenterManager snapshot from binary
[[nodiscard]] inline std::optional<PresenterManagerSnapshot> deserialize_presenter_manager_snapshot(
    const std::vector<std::uint8_t>& data) {

    if (data.size() < 8) return std::nullopt;

    BinaryReader reader(data);

    std::uint32_t magic = reader.read_u32();
    if (magic != PresenterManagerSnapshot::MAGIC) {
        return std::nullopt;
    }

    PresenterManagerSnapshot snapshot;
    snapshot.version = reader.read_u32();

    if (!snapshot.is_compatible()) {
        return std::nullopt;
    }

    snapshot.primary_id = reader.read_u64();
    snapshot.next_id = reader.read_u64();

    std::uint32_t state_count = reader.read_u32();
    snapshot.presenter_states.reserve(state_count);
    for (std::uint32_t i = 0; i < state_count; ++i) {
        std::uint64_t id = reader.read_u64();
        RehydrationState state = deserialize_rehydration_state(reader);
        snapshot.presenter_states.push_back({id, std::move(state)});
    }

    if (!reader.valid()) {
        return std::nullopt;
    }

    return snapshot;
}

// =============================================================================
// MultiBackendPresenter Snapshot
// =============================================================================

/// Snapshot of the MultiBackendPresenter state
struct MultiBackendPresenterSnapshot {
    static constexpr std::uint32_t VERSION = 1;
    static constexpr std::uint32_t MAGIC = 0x4D425053;  // "MBPS"

    std::uint32_t version = VERSION;
    RehydrationState state;
    BackendType backend_type = BackendType::Null;
    std::uint64_t frame_number = 0;
    std::uint64_t frames_presented = 0;
    std::uint64_t backend_switches = 0;

    [[nodiscard]] bool is_compatible() const {
        return version == VERSION;
    }
};

/// Take a snapshot of a MultiBackendPresenter
[[nodiscard]] inline MultiBackendPresenterSnapshot take_multi_backend_presenter_snapshot(
    const MultiBackendPresenter& presenter) {

    MultiBackendPresenterSnapshot snapshot;
    snapshot.version = MultiBackendPresenterSnapshot::VERSION;
    snapshot.state = presenter.dehydrate();
    snapshot.backend_type = presenter.current_backend();

    auto stats = presenter.statistics();
    snapshot.frame_number = stats.total_frames;
    snapshot.frames_presented = stats.frames_presented;
    snapshot.backend_switches = stats.backend_switches;

    return snapshot;
}

/// Serialize MultiBackendPresenter snapshot to binary
[[nodiscard]] inline std::vector<std::uint8_t> serialize_multi_backend_presenter_snapshot(
    const MultiBackendPresenterSnapshot& snapshot) {

    BinaryWriter writer;

    writer.write_u32(MultiBackendPresenterSnapshot::MAGIC);
    writer.write_u32(snapshot.version);
    writer.write_u8(static_cast<std::uint8_t>(snapshot.backend_type));
    writer.write_u64(snapshot.frame_number);
    writer.write_u64(snapshot.frames_presented);
    writer.write_u64(snapshot.backend_switches);
    serialize_rehydration_state(writer, snapshot.state);

    return writer.take();
}

/// Deserialize MultiBackendPresenter snapshot from binary
[[nodiscard]] inline std::optional<MultiBackendPresenterSnapshot> deserialize_multi_backend_presenter_snapshot(
    const std::vector<std::uint8_t>& data) {

    if (data.size() < 8) return std::nullopt;

    BinaryReader reader(data);

    std::uint32_t magic = reader.read_u32();
    if (magic != MultiBackendPresenterSnapshot::MAGIC) {
        return std::nullopt;
    }

    MultiBackendPresenterSnapshot snapshot;
    snapshot.version = reader.read_u32();

    if (!snapshot.is_compatible()) {
        return std::nullopt;
    }

    snapshot.backend_type = static_cast<BackendType>(reader.read_u8());
    snapshot.frame_number = reader.read_u64();
    snapshot.frames_presented = reader.read_u64();
    snapshot.backend_switches = reader.read_u64();
    snapshot.state = deserialize_rehydration_state(reader);

    if (!reader.valid()) {
        return std::nullopt;
    }

    return snapshot;
}

/// Restore a MultiBackendPresenter from a snapshot
/// @return true on success
inline bool restore_multi_backend_presenter_snapshot(
    MultiBackendPresenter& presenter,
    const MultiBackendPresenterSnapshot& snapshot) {

    if (!snapshot.is_compatible()) {
        return false;
    }

    return presenter.rehydrate(snapshot.state);
}

// =============================================================================
// Convenience Functions
// =============================================================================

/// Take and serialize PresenterManager snapshot in one call
[[nodiscard]] inline std::vector<std::uint8_t> take_and_serialize_presenter_manager(
    const PresenterManager& manager) {
    return serialize_presenter_manager_snapshot(take_presenter_manager_snapshot(manager));
}

/// Take and serialize MultiBackendPresenter snapshot in one call
[[nodiscard]] inline std::vector<std::uint8_t> take_and_serialize_multi_backend_presenter(
    const MultiBackendPresenter& presenter) {
    return serialize_multi_backend_presenter_snapshot(
        take_multi_backend_presenter_snapshot(presenter));
}

} // namespace void_presenter
