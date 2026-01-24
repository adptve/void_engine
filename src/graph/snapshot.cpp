/// @file snapshot.cpp
/// @brief Hot-reload snapshot system implementation

#include "types.hpp"
#include <void_engine/graph/snapshot.hpp>

#include <cstring>
#include <stdexcept>

namespace void_graph {

// =============================================================================
// SnapshotWriter Implementation
// =============================================================================

void SnapshotWriter::write_u32(std::uint32_t v) {
    data_.push_back(static_cast<std::uint8_t>(v & 0xFF));
    data_.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    data_.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    data_.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

void SnapshotWriter::write_u64(std::uint64_t v) {
    write_u32(static_cast<std::uint32_t>(v & 0xFFFFFFFF));
    write_u32(static_cast<std::uint32_t>((v >> 32) & 0xFFFFFFFF));
}

void SnapshotWriter::write_f32(float v) {
    std::uint32_t bits;
    std::memcpy(&bits, &v, sizeof(float));
    write_u32(bits);
}

void SnapshotWriter::write_string(const std::string& s) {
    write_u32(static_cast<std::uint32_t>(s.size()));
    for (char c : s) {
        data_.push_back(static_cast<std::uint8_t>(c));
    }
}

void SnapshotWriter::write_value(const PinValue& v) {
    // Write variant index
    write_u8(static_cast<std::uint8_t>(v.index()));

    std::visit([this](const auto& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            // Nothing to write
        } else if constexpr (std::is_same_v<T, bool>) {
            write_u8(val ? 1 : 0);
        } else if constexpr (std::is_same_v<T, std::int32_t>) {
            write_u32(static_cast<std::uint32_t>(val));
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            write_u64(static_cast<std::uint64_t>(val));
        } else if constexpr (std::is_same_v<T, float>) {
            write_f32(val);
        } else if constexpr (std::is_same_v<T, double>) {
            std::uint64_t bits;
            std::memcpy(&bits, &val, sizeof(double));
            write_u64(bits);
        } else if constexpr (std::is_same_v<T, std::string>) {
            write_string(val);
        } else if constexpr (std::is_same_v<T, std::array<float, 2>>) {
            // Vec2
            write_f32(val[0]);
            write_f32(val[1]);
        } else if constexpr (std::is_same_v<T, std::array<float, 3>>) {
            // Vec3
            write_f32(val[0]);
            write_f32(val[1]);
            write_f32(val[2]);
        } else if constexpr (std::is_same_v<T, std::array<float, 4>>) {
            // Vec4/Quat
            write_f32(val[0]);
            write_f32(val[1]);
            write_f32(val[2]);
            write_f32(val[3]);
        } else if constexpr (std::is_same_v<T, std::array<float, 16>>) {
            // Mat4
            for (int i = 0; i < 16; ++i) {
                write_f32(val[i]);
            }
        } else if constexpr (std::is_same_v<T, std::uint64_t>) {
            write_u64(val);
        } else if constexpr (std::is_same_v<T, std::vector<std::any>>) {
            // Array of any - write count but cannot serialize contents
            write_u32(static_cast<std::uint32_t>(val.size()));
            // Cannot serialize std::any contents
        } else if constexpr (std::is_same_v<T, std::any>) {
            // Cannot serialize std::any - write empty marker
            write_u8(0);
        }
    }, v);
}

// =============================================================================
// SnapshotReader Implementation
// =============================================================================

std::uint8_t SnapshotReader::read_u8() {
    if (pos_ >= data_.size()) {
        throw std::runtime_error("Snapshot read overflow");
    }
    return data_[pos_++];
}

std::uint32_t SnapshotReader::read_u32() {
    std::uint32_t v = 0;
    v |= static_cast<std::uint32_t>(read_u8());
    v |= static_cast<std::uint32_t>(read_u8()) << 8;
    v |= static_cast<std::uint32_t>(read_u8()) << 16;
    v |= static_cast<std::uint32_t>(read_u8()) << 24;
    return v;
}

std::uint64_t SnapshotReader::read_u64() {
    std::uint64_t low = read_u32();
    std::uint64_t high = read_u32();
    return low | (high << 32);
}

float SnapshotReader::read_f32() {
    std::uint32_t bits = read_u32();
    float v;
    std::memcpy(&v, &bits, sizeof(float));
    return v;
}

std::string SnapshotReader::read_string() {
    std::uint32_t len = read_u32();
    std::string s;
    s.reserve(len);
    for (std::uint32_t i = 0; i < len; ++i) {
        s.push_back(static_cast<char>(read_u8()));
    }
    return s;
}

PinValue SnapshotReader::read_value() {
    std::uint8_t index = read_u8();

    switch (index) {
        case 0: // monostate
            return std::monostate{};
        case 1: // bool
            return read_u8() != 0;
        case 2: // int32
            return static_cast<std::int32_t>(read_u32());
        case 3: // int64
            return static_cast<std::int64_t>(read_u64());
        case 4: // float
            return read_f32();
        case 5: { // double
            std::uint64_t bits = read_u64();
            double v;
            std::memcpy(&v, &bits, sizeof(double));
            return v;
        }
        case 6: // string
            return read_string();
        case 7: { // Vec2
            std::array<float, 2> arr;
            arr[0] = read_f32();
            arr[1] = read_f32();
            return arr;
        }
        case 8: { // Vec3
            std::array<float, 3> arr;
            arr[0] = read_f32();
            arr[1] = read_f32();
            arr[2] = read_f32();
            return arr;
        }
        case 9:   // Vec4
        case 10: { // Quat (same as Vec4 storage)
            std::array<float, 4> arr;
            arr[0] = read_f32();
            arr[1] = read_f32();
            arr[2] = read_f32();
            arr[3] = read_f32();
            return arr;
        }
        case 11: { // Mat4
            std::array<float, 16> arr;
            for (int i = 0; i < 16; ++i) {
                arr[i] = read_f32();
            }
            return arr;
        }
        case 12: // uint64 (entity)
            return read_u64();
        case 13: { // vector<any> - read count but cannot deserialize contents
            std::uint32_t count = read_u32();
            (void)count;  // Cannot reconstruct std::any contents
            return std::vector<std::any>{};
        }
        case 14: // any - cannot deserialize
            read_u8(); // consume marker
            return std::monostate{};
        default:
            return std::monostate{};
    }
}

// =============================================================================
// GraphSystemSnapshot Serialization
// =============================================================================

std::vector<std::uint8_t> GraphSystemSnapshot::serialize() const {
    SnapshotWriter writer;

    // Write header
    writer.write_u32(magic);
    writer.write_u32(version);

    // Write flags
    writer.write_u8(debug_mode ? 1 : 0);
    writer.write_u8(hot_reload_enabled ? 1 : 0);

    // Write instance count
    writer.write_u32(static_cast<std::uint32_t>(instances.size()));

    // Write each instance
    for (const auto& instance : instances) {
        // Graph ID
        writer.write_u32(instance.graph_id.value);

        // Owner entity
        writer.write_u64(instance.owner_entity);

        // Execution state
        writer.write_u8(static_cast<std::uint8_t>(instance.state));

        // Timing
        writer.write_f32(instance.total_time);
        writer.write_u64(instance.frame_count);

        // Variables
        writer.write_u32(static_cast<std::uint32_t>(instance.variables.size()));
        for (const auto& var : instance.variables) {
            writer.write_u32(var.id.value);
            writer.write_string(var.name);
            writer.write_u8(static_cast<std::uint8_t>(var.type));
            writer.write_value(var.value);
        }
    }

    return writer.take_data();
}

std::optional<GraphSystemSnapshot> GraphSystemSnapshot::deserialize(
    const std::vector<std::uint8_t>& data) {

    if (data.size() < 8) {
        return std::nullopt;
    }

    try {
        SnapshotReader reader(data);

        GraphSystemSnapshot snapshot;

        // Read header
        snapshot.magic = reader.read_u32();
        snapshot.version = reader.read_u32();

        if (!snapshot.is_valid()) {
            return std::nullopt;
        }

        // Read flags
        snapshot.debug_mode = reader.read_u8() != 0;
        snapshot.hot_reload_enabled = reader.read_u8() != 0;

        // Read instance count
        std::uint32_t instance_count = reader.read_u32();
        snapshot.instances.reserve(instance_count);

        // Read each instance
        for (std::uint32_t i = 0; i < instance_count; ++i) {
            GraphInstanceSnapshot instance;

            // Graph ID
            instance.graph_id.value = reader.read_u32();

            // Owner entity
            instance.owner_entity = reader.read_u64();

            // Execution state
            instance.state = static_cast<ExecutionState>(reader.read_u8());

            // Timing
            instance.total_time = reader.read_f32();
            instance.frame_count = reader.read_u64();

            // Variables
            std::uint32_t var_count = reader.read_u32();
            instance.variables.reserve(var_count);
            for (std::uint32_t j = 0; j < var_count; ++j) {
                VariableSnapshot var;
                var.id.value = reader.read_u32();
                var.name = reader.read_string();
                var.type = static_cast<PinType>(reader.read_u8());
                var.value = reader.read_value();
                instance.variables.push_back(std::move(var));
            }

            snapshot.instances.push_back(std::move(instance));
        }

        return snapshot;

    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace void_graph
