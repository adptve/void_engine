#pragma once

/// @file snapshot.hpp
/// @brief Hot-reload snapshot system for void_graph

#include "graph.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace void_graph {

// =============================================================================
// Graph Snapshot
// =============================================================================

/// @brief Snapshot of a single variable value
struct VariableSnapshot {
    VariableId id;
    std::string name;
    PinType type = PinType::Any;
    PinValue value;
};

/// @brief Snapshot of a graph instance's runtime state
struct GraphInstanceSnapshot {
    GraphId graph_id;
    std::uint64_t owner_entity = 0;
    std::vector<VariableSnapshot> variables;
    ExecutionState state = ExecutionState::Idle;
    float total_time = 0.0f;
    std::uint64_t frame_count = 0;
};

/// @brief Snapshot of the entire graph system
struct GraphSystemSnapshot {
    static constexpr std::uint32_t k_magic = 0x47525048;  // "GRPH"
    static constexpr std::uint32_t k_version = 1;

    std::uint32_t magic = k_magic;
    std::uint32_t version = k_version;

    std::vector<GraphInstanceSnapshot> instances;
    bool debug_mode = false;
    bool hot_reload_enabled = false;

    /// @brief Serialize snapshot to binary
    [[nodiscard]] std::vector<std::uint8_t> serialize() const;

    /// @brief Deserialize snapshot from binary
    [[nodiscard]] static std::optional<GraphSystemSnapshot> deserialize(
        const std::vector<std::uint8_t>& data);

    /// @brief Check if snapshot is valid
    [[nodiscard]] bool is_valid() const {
        return magic == k_magic && version <= k_version;
    }
};

// =============================================================================
// Snapshot Serialization Helpers
// =============================================================================

/// @brief Binary writer for snapshot data
class SnapshotWriter {
public:
    void write_u8(std::uint8_t v) { data_.push_back(v); }
    void write_u32(std::uint32_t v);
    void write_u64(std::uint64_t v);
    void write_f32(float v);
    void write_string(const std::string& s);
    void write_value(const PinValue& v);

    [[nodiscard]] const std::vector<std::uint8_t>& data() const { return data_; }
    [[nodiscard]] std::vector<std::uint8_t> take_data() { return std::move(data_); }

private:
    std::vector<std::uint8_t> data_;
};

/// @brief Binary reader for snapshot data
class SnapshotReader {
public:
    explicit SnapshotReader(const std::vector<std::uint8_t>& data) : data_(data) {}

    [[nodiscard]] std::uint8_t read_u8();
    [[nodiscard]] std::uint32_t read_u32();
    [[nodiscard]] std::uint64_t read_u64();
    [[nodiscard]] float read_f32();
    [[nodiscard]] std::string read_string();
    [[nodiscard]] PinValue read_value();

    [[nodiscard]] bool has_data() const { return pos_ < data_.size(); }
    [[nodiscard]] bool eof() const { return pos_ >= data_.size(); }

private:
    const std::vector<std::uint8_t>& data_;
    std::size_t pos_ = 0;
};

} // namespace void_graph
