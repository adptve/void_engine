#pragma once

/// @file snapshot.hpp
/// @brief Hot-reload snapshot support for void_compositor
///
/// Provides binary serialization/deserialization for:
/// - RehydrationState
/// - Compositor state
/// - Frame scheduler state
/// - VRR/HDR configurations
/// - Layer configurations

#include "rehydration.hpp"
#include "compositor.hpp"
#include "vrr.hpp"
#include "hdr.hpp"
#include "frame.hpp"
#include "types.hpp"

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace void_compositor {

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

    void write_f32(float v) {
        std::uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        write_u32(bits);
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

    [[nodiscard]] float read_f32() {
        std::uint32_t bits = read_u32();
        float v;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
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
// VRR Config Serialization
// =============================================================================

/// Serialize VRR config to binary
inline void serialize_vrr_config_binary(BinaryWriter& writer, const VrrConfig& config) {
    writer.write_bool(config.enabled);
    writer.write_u32(config.min_refresh_rate);
    writer.write_u32(config.max_refresh_rate);
    writer.write_u32(config.current_refresh_rate);
    writer.write_u8(static_cast<std::uint8_t>(config.mode));
}

/// Deserialize VRR config from binary
inline VrrConfig deserialize_vrr_config_binary(BinaryReader& reader) {
    VrrConfig config;
    config.enabled = reader.read_bool();
    config.min_refresh_rate = reader.read_u32();
    config.max_refresh_rate = reader.read_u32();
    config.current_refresh_rate = reader.read_u32();
    config.mode = static_cast<VrrMode>(reader.read_u8());
    return config;
}

// =============================================================================
// HDR Config Serialization
// =============================================================================

/// Serialize HDR config to binary
inline void serialize_hdr_config_binary(BinaryWriter& writer, const HdrConfig& config) {
    writer.write_bool(config.enabled);
    writer.write_u8(static_cast<std::uint8_t>(config.transfer_function));
    writer.write_u8(static_cast<std::uint8_t>(config.color_primaries));
    writer.write_u32(config.max_luminance);
    writer.write_f32(config.min_luminance);
    writer.write_bool(config.max_content_light_level.has_value());
    if (config.max_content_light_level) {
        writer.write_u32(*config.max_content_light_level);
    }
    writer.write_bool(config.max_frame_average_light_level.has_value());
    if (config.max_frame_average_light_level) {
        writer.write_u32(*config.max_frame_average_light_level);
    }
}

/// Deserialize HDR config from binary
inline HdrConfig deserialize_hdr_config_binary(BinaryReader& reader) {
    HdrConfig config;
    config.enabled = reader.read_bool();
    config.transfer_function = static_cast<TransferFunction>(reader.read_u8());
    config.color_primaries = static_cast<ColorPrimaries>(reader.read_u8());
    config.max_luminance = reader.read_u32();
    config.min_luminance = reader.read_f32();
    if (reader.read_bool()) {
        config.max_content_light_level = reader.read_u32();
    }
    if (reader.read_bool()) {
        config.max_frame_average_light_level = reader.read_u32();
    }
    return config;
}

// =============================================================================
// Compositor Config Serialization
// =============================================================================

/// Serialize compositor config to binary
inline void serialize_compositor_config_binary(BinaryWriter& writer, const CompositorConfig& config) {
    writer.write_u32(config.target_fps);
    writer.write_bool(config.vsync);
    writer.write_bool(config.allow_tearing);
    writer.write_bool(config.xwayland);
    writer.write_bool(config.enable_vrr);
    writer.write_bool(config.enable_hdr);
    writer.write_u8(static_cast<std::uint8_t>(config.preferred_format));
}

/// Deserialize compositor config from binary
inline CompositorConfig deserialize_compositor_config_binary(BinaryReader& reader) {
    CompositorConfig config;
    config.target_fps = reader.read_u32();
    config.vsync = reader.read_bool();
    config.allow_tearing = reader.read_bool();
    config.xwayland = reader.read_bool();
    config.enable_vrr = reader.read_bool();
    config.enable_hdr = reader.read_bool();
    config.preferred_format = static_cast<RenderFormat>(reader.read_u8());
    return config;
}

// =============================================================================
// Output Mode Serialization
// =============================================================================

/// Serialize output mode to binary
inline void serialize_output_mode_binary(BinaryWriter& writer, const OutputMode& mode) {
    writer.write_u32(mode.width);
    writer.write_u32(mode.height);
    writer.write_u32(mode.refresh_mhz);
}

/// Deserialize output mode from binary
inline OutputMode deserialize_output_mode_binary(BinaryReader& reader) {
    OutputMode mode;
    mode.width = reader.read_u32();
    mode.height = reader.read_u32();
    mode.refresh_mhz = reader.read_u32();
    return mode;
}

// =============================================================================
// Frame Scheduler Snapshot
// =============================================================================

/// Snapshot of FrameScheduler state
struct FrameSchedulerSnapshot {
    static constexpr std::uint32_t VERSION = 1;
    static constexpr std::uint32_t MAGIC = 0x46524D53;  // "FRMS"

    std::uint32_t version = VERSION;
    std::uint32_t target_fps = 60;
    std::uint64_t frame_number = 0;
    std::uint64_t dropped_frame_count = 0;
    float content_velocity = 0.0f;
    bool has_vrr_config = false;
    VrrConfig vrr_config{};

    [[nodiscard]] bool is_compatible() const {
        return version == VERSION;
    }
};

/// Take a snapshot of FrameScheduler
[[nodiscard]] inline FrameSchedulerSnapshot take_frame_scheduler_snapshot(
    const FrameScheduler& scheduler) {

    FrameSchedulerSnapshot snapshot;
    snapshot.version = FrameSchedulerSnapshot::VERSION;
    snapshot.target_fps = scheduler.target_fps();
    snapshot.frame_number = scheduler.frame_number();
    snapshot.dropped_frame_count = scheduler.dropped_frame_count();
    snapshot.content_velocity = scheduler.content_velocity();

    if (scheduler.vrr_config()) {
        snapshot.has_vrr_config = true;
        snapshot.vrr_config = *scheduler.vrr_config();
    }

    return snapshot;
}

/// Serialize FrameScheduler snapshot to binary
[[nodiscard]] inline std::vector<std::uint8_t> serialize_frame_scheduler_snapshot(
    const FrameSchedulerSnapshot& snapshot) {

    BinaryWriter writer;

    writer.write_u32(FrameSchedulerSnapshot::MAGIC);
    writer.write_u32(snapshot.version);
    writer.write_u32(snapshot.target_fps);
    writer.write_u64(snapshot.frame_number);
    writer.write_u64(snapshot.dropped_frame_count);
    writer.write_f32(snapshot.content_velocity);
    writer.write_bool(snapshot.has_vrr_config);

    if (snapshot.has_vrr_config) {
        serialize_vrr_config_binary(writer, snapshot.vrr_config);
    }

    return writer.take();
}

/// Deserialize FrameScheduler snapshot from binary
[[nodiscard]] inline std::optional<FrameSchedulerSnapshot> deserialize_frame_scheduler_snapshot(
    const std::vector<std::uint8_t>& data) {

    if (data.size() < 8) return std::nullopt;

    BinaryReader reader(data);

    std::uint32_t magic = reader.read_u32();
    if (magic != FrameSchedulerSnapshot::MAGIC) {
        return std::nullopt;
    }

    FrameSchedulerSnapshot snapshot;
    snapshot.version = reader.read_u32();

    if (!snapshot.is_compatible()) {
        return std::nullopt;
    }

    snapshot.target_fps = reader.read_u32();
    snapshot.frame_number = reader.read_u64();
    snapshot.dropped_frame_count = reader.read_u64();
    snapshot.content_velocity = reader.read_f32();
    snapshot.has_vrr_config = reader.read_bool();

    if (snapshot.has_vrr_config) {
        snapshot.vrr_config = deserialize_vrr_config_binary(reader);
    }

    if (!reader.valid()) {
        return std::nullopt;
    }

    return snapshot;
}

// =============================================================================
// Compositor Snapshot
// =============================================================================

/// Snapshot of compositor state
struct CompositorSnapshot {
    static constexpr std::uint32_t VERSION = 1;
    static constexpr std::uint32_t MAGIC = 0x434F4D50;  // "COMP"

    std::uint32_t version = VERSION;
    CompositorConfig config{};
    FrameSchedulerSnapshot frame_scheduler{};
    bool has_vrr_config = false;
    VrrConfig vrr_config{};
    bool has_hdr_config = false;
    HdrConfig hdr_config{};
    std::uint64_t frame_number = 0;
    bool is_running = true;

    [[nodiscard]] bool is_compatible() const {
        return version == VERSION;
    }
};

/// Take a snapshot of compositor state
[[nodiscard]] inline CompositorSnapshot take_compositor_snapshot(const ICompositor& compositor) {
    CompositorSnapshot snapshot;
    snapshot.version = CompositorSnapshot::VERSION;
    snapshot.config = compositor.config();
    snapshot.frame_scheduler = take_frame_scheduler_snapshot(compositor.frame_scheduler());
    snapshot.frame_number = compositor.frame_number();
    snapshot.is_running = compositor.is_running();

    if (auto vrr = compositor.vrr_config()) {
        snapshot.has_vrr_config = true;
        snapshot.vrr_config = *vrr;
    }

    if (auto hdr = compositor.hdr_config()) {
        snapshot.has_hdr_config = true;
        snapshot.hdr_config = *hdr;
    }

    return snapshot;
}

/// Serialize compositor snapshot to binary
[[nodiscard]] inline std::vector<std::uint8_t> serialize_compositor_snapshot(
    const CompositorSnapshot& snapshot) {

    BinaryWriter writer;

    writer.write_u32(CompositorSnapshot::MAGIC);
    writer.write_u32(snapshot.version);

    // Config
    serialize_compositor_config_binary(writer, snapshot.config);

    // Frame scheduler
    auto fs_data = serialize_frame_scheduler_snapshot(snapshot.frame_scheduler);
    writer.write_bytes(fs_data);

    // VRR
    writer.write_bool(snapshot.has_vrr_config);
    if (snapshot.has_vrr_config) {
        serialize_vrr_config_binary(writer, snapshot.vrr_config);
    }

    // HDR
    writer.write_bool(snapshot.has_hdr_config);
    if (snapshot.has_hdr_config) {
        serialize_hdr_config_binary(writer, snapshot.hdr_config);
    }

    // State
    writer.write_u64(snapshot.frame_number);
    writer.write_bool(snapshot.is_running);

    return writer.take();
}

/// Deserialize compositor snapshot from binary
[[nodiscard]] inline std::optional<CompositorSnapshot> deserialize_compositor_snapshot(
    const std::vector<std::uint8_t>& data) {

    if (data.size() < 8) return std::nullopt;

    BinaryReader reader(data);

    std::uint32_t magic = reader.read_u32();
    if (magic != CompositorSnapshot::MAGIC) {
        return std::nullopt;
    }

    CompositorSnapshot snapshot;
    snapshot.version = reader.read_u32();

    if (!snapshot.is_compatible()) {
        return std::nullopt;
    }

    // Config
    snapshot.config = deserialize_compositor_config_binary(reader);

    // Frame scheduler
    auto fs_data = reader.read_bytes();
    auto fs_snapshot = deserialize_frame_scheduler_snapshot(fs_data);
    if (!fs_snapshot) {
        return std::nullopt;
    }
    snapshot.frame_scheduler = *fs_snapshot;

    // VRR
    snapshot.has_vrr_config = reader.read_bool();
    if (snapshot.has_vrr_config) {
        snapshot.vrr_config = deserialize_vrr_config_binary(reader);
    }

    // HDR
    snapshot.has_hdr_config = reader.read_bool();
    if (snapshot.has_hdr_config) {
        snapshot.hdr_config = deserialize_hdr_config_binary(reader);
    }

    // State
    snapshot.frame_number = reader.read_u64();
    snapshot.is_running = reader.read_bool();

    if (!reader.valid()) {
        return std::nullopt;
    }

    return snapshot;
}

// =============================================================================
// Convenience Functions
// =============================================================================

/// Take and serialize compositor snapshot in one call
[[nodiscard]] inline std::vector<std::uint8_t> take_and_serialize_compositor(
    const ICompositor& compositor) {
    return serialize_compositor_snapshot(take_compositor_snapshot(compositor));
}

/// Take and serialize frame scheduler snapshot in one call
[[nodiscard]] inline std::vector<std::uint8_t> take_and_serialize_frame_scheduler(
    const FrameScheduler& scheduler) {
    return serialize_frame_scheduler_snapshot(take_frame_scheduler_snapshot(scheduler));
}

} // namespace void_compositor
