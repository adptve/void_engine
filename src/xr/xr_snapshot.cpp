/// @file xr_snapshot.cpp
/// @brief XR hot-reload snapshot system

#include <void_engine/presenter/xr/xr_system.hpp>

#include <cstring>
#include <optional>
#include <vector>

namespace void_presenter {
namespace xr {

// =============================================================================
// XR Snapshot Types
// =============================================================================

/// @brief Snapshot of XR session state for hot-reload
struct XrSessionSnapshot {
    XrSessionConfig config;
    XrSessionState state = XrSessionState::Idle;
    ReferenceSpaceType reference_space = ReferenceSpaceType::LocalFloor;
    FoveatedRenderingConfig foveation;

    // Last known head pose for smooth transition
    Pose last_head_pose;

    // Last known controller poses
    Pose left_controller_pose;
    Pose right_controller_pose;
    bool left_controller_active = false;
    bool right_controller_active = false;
};

/// @brief Snapshot of entire XR system for hot-reload
struct XrSystemSnapshot {
    static constexpr std::uint32_t k_magic = 0x58525353;  // "XRSS"
    static constexpr std::uint32_t k_version = 1;

    std::uint32_t magic = k_magic;
    std::uint32_t version = k_version;

    std::string runtime_name;
    XrSystemType system_type = XrSystemType::None;

    std::optional<XrSessionSnapshot> session;

    /// @brief Serialize snapshot to binary
    [[nodiscard]] std::vector<std::uint8_t> serialize() const;

    /// @brief Deserialize snapshot from binary
    [[nodiscard]] static std::optional<XrSystemSnapshot> deserialize(
        const std::vector<std::uint8_t>& data);

    /// @brief Check if snapshot is valid
    [[nodiscard]] bool is_valid() const {
        return magic == k_magic && version <= k_version;
    }
};

// =============================================================================
// Binary Serialization Helpers
// =============================================================================

namespace {

class XrSnapshotWriter {
public:
    void write_u8(std::uint8_t v) { data_.push_back(v); }

    void write_u32(std::uint32_t v) {
        data_.push_back(static_cast<std::uint8_t>(v & 0xFF));
        data_.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        data_.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        data_.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    }

    void write_f32(float v) {
        std::uint32_t bits;
        std::memcpy(&bits, &v, sizeof(float));
        write_u32(bits);
    }

    void write_string(const std::string& s) {
        write_u32(static_cast<std::uint32_t>(s.size()));
        for (char c : s) {
            data_.push_back(static_cast<std::uint8_t>(c));
        }
    }

    void write_pose(const Pose& pose) {
        write_f32(pose.position.x);
        write_f32(pose.position.y);
        write_f32(pose.position.z);
        write_f32(pose.orientation.x);
        write_f32(pose.orientation.y);
        write_f32(pose.orientation.z);
        write_f32(pose.orientation.w);
    }

    void write_foveation(const FoveatedRenderingConfig& config) {
        write_u8(static_cast<std::uint8_t>(config.level));
        write_u8(config.dynamic ? 1 : 0);
        write_f32(config.inner_radius);
        write_f32(config.middle_radius);
    }

    [[nodiscard]] std::vector<std::uint8_t> take_data() { return std::move(data_); }

private:
    std::vector<std::uint8_t> data_;
};

class XrSnapshotReader {
public:
    explicit XrSnapshotReader(const std::vector<std::uint8_t>& data) : data_(data) {}

    [[nodiscard]] std::uint8_t read_u8() {
        if (pos_ >= data_.size()) return 0;
        return data_[pos_++];
    }

    [[nodiscard]] std::uint32_t read_u32() {
        std::uint32_t v = 0;
        v |= static_cast<std::uint32_t>(read_u8());
        v |= static_cast<std::uint32_t>(read_u8()) << 8;
        v |= static_cast<std::uint32_t>(read_u8()) << 16;
        v |= static_cast<std::uint32_t>(read_u8()) << 24;
        return v;
    }

    [[nodiscard]] float read_f32() {
        std::uint32_t bits = read_u32();
        float v;
        std::memcpy(&v, &bits, sizeof(float));
        return v;
    }

    [[nodiscard]] std::string read_string() {
        std::uint32_t len = read_u32();
        std::string s;
        s.reserve(len);
        for (std::uint32_t i = 0; i < len; ++i) {
            s.push_back(static_cast<char>(read_u8()));
        }
        return s;
    }

    [[nodiscard]] Pose read_pose() {
        Pose pose;
        pose.position.x = read_f32();
        pose.position.y = read_f32();
        pose.position.z = read_f32();
        pose.orientation.x = read_f32();
        pose.orientation.y = read_f32();
        pose.orientation.z = read_f32();
        pose.orientation.w = read_f32();
        return pose;
    }

    [[nodiscard]] FoveatedRenderingConfig read_foveation() {
        FoveatedRenderingConfig config;
        config.level = static_cast<FoveationLevel>(read_u8());
        config.dynamic = read_u8() != 0;
        config.inner_radius = read_f32();
        config.middle_radius = read_f32();
        return config;
    }

    [[nodiscard]] bool eof() const { return pos_ >= data_.size(); }

private:
    const std::vector<std::uint8_t>& data_;
    std::size_t pos_ = 0;
};

} // anonymous namespace

// =============================================================================
// XrSystemSnapshot Serialization
// =============================================================================

std::vector<std::uint8_t> XrSystemSnapshot::serialize() const {
    XrSnapshotWriter writer;

    // Header
    writer.write_u32(magic);
    writer.write_u32(version);

    // Runtime info
    writer.write_string(runtime_name);
    writer.write_u8(static_cast<std::uint8_t>(system_type));

    // Session state
    writer.write_u8(session.has_value() ? 1 : 0);
    if (session) {
        const auto& s = *session;

        // Config
        writer.write_u8(static_cast<std::uint8_t>(s.config.primary_reference_space));
        writer.write_u8(s.config.enable_hand_tracking ? 1 : 0);
        writer.write_u8(s.config.enable_eye_tracking ? 1 : 0);
        writer.write_u8(s.config.enable_passthrough ? 1 : 0);
        writer.write_u32(s.config.view_count);
        writer.write_u8(static_cast<std::uint8_t>(s.config.color_format));
        writer.write_u32(s.config.sample_count);
        writer.write_foveation(s.config.foveation);

        // State
        writer.write_u8(static_cast<std::uint8_t>(s.state));
        writer.write_u8(static_cast<std::uint8_t>(s.reference_space));
        writer.write_foveation(s.foveation);

        // Last known poses
        writer.write_pose(s.last_head_pose);
        writer.write_pose(s.left_controller_pose);
        writer.write_pose(s.right_controller_pose);
        writer.write_u8(s.left_controller_active ? 1 : 0);
        writer.write_u8(s.right_controller_active ? 1 : 0);
    }

    return writer.take_data();
}

std::optional<XrSystemSnapshot> XrSystemSnapshot::deserialize(
    const std::vector<std::uint8_t>& data) {

    if (data.size() < 8) {
        return std::nullopt;
    }

    try {
        XrSnapshotReader reader(data);
        XrSystemSnapshot snapshot;

        // Header
        snapshot.magic = reader.read_u32();
        snapshot.version = reader.read_u32();

        if (!snapshot.is_valid()) {
            return std::nullopt;
        }

        // Runtime info
        snapshot.runtime_name = reader.read_string();
        snapshot.system_type = static_cast<XrSystemType>(reader.read_u8());

        // Session state
        bool has_session = reader.read_u8() != 0;
        if (has_session) {
            XrSessionSnapshot s;

            // Config
            s.config.primary_reference_space = static_cast<ReferenceSpaceType>(reader.read_u8());
            s.config.enable_hand_tracking = reader.read_u8() != 0;
            s.config.enable_eye_tracking = reader.read_u8() != 0;
            s.config.enable_passthrough = reader.read_u8() != 0;
            s.config.view_count = reader.read_u32();
            s.config.color_format = static_cast<SurfaceFormat>(reader.read_u8());
            s.config.sample_count = reader.read_u32();
            s.config.foveation = reader.read_foveation();

            // State
            s.state = static_cast<XrSessionState>(reader.read_u8());
            s.reference_space = static_cast<ReferenceSpaceType>(reader.read_u8());
            s.foveation = reader.read_foveation();

            // Last known poses
            s.last_head_pose = reader.read_pose();
            s.left_controller_pose = reader.read_pose();
            s.right_controller_pose = reader.read_pose();
            s.left_controller_active = reader.read_u8() != 0;
            s.right_controller_active = reader.read_u8() != 0;

            snapshot.session = std::move(s);
        }

        return snapshot;

    } catch (...) {
        return std::nullopt;
    }
}

// =============================================================================
// XR Hot-Reload Manager
// =============================================================================

/// @brief Manages XR hot-reload state
class XrHotReloadManager {
public:
    /// @brief Capture current XR state for hot-reload
    [[nodiscard]] XrSystemSnapshot capture(IXrSystem* system, IXrSession* session) const {
        XrSystemSnapshot snapshot;

        if (system) {
            const auto& info = system->runtime_info();
            snapshot.runtime_name = info.name;
            snapshot.system_type = info.system_type;
        }

        if (session) {
            XrSessionSnapshot session_snap;
            session_snap.config = session->config();
            session_snap.state = session->state();
            session_snap.reference_space = session_snap.config.primary_reference_space;

            // Capture last known poses
            auto head = session->get_head_pose();
            session_snap.last_head_pose = head.pose;

            auto left = session->get_controller(Hand::Left);
            if (left) {
                session_snap.left_controller_pose = left->pose.pose;
                session_snap.left_controller_active = left->active;
            }

            auto right = session->get_controller(Hand::Right);
            if (right) {
                session_snap.right_controller_pose = right->pose.pose;
                session_snap.right_controller_active = right->active;
            }

            snapshot.session = std::move(session_snap);
        }

        return snapshot;
    }

    /// @brief Restore XR state after hot-reload
    /// @return Session config to use for recreation
    [[nodiscard]] std::optional<XrSessionConfig> restore(const XrSystemSnapshot& snapshot) const {
        if (!snapshot.is_valid() || !snapshot.session) {
            return std::nullopt;
        }

        return snapshot.session->config;
    }

    /// @brief Serialize XR state for persistence
    [[nodiscard]] std::vector<std::uint8_t> serialize_state(
        IXrSystem* system, IXrSession* session) const {
        return capture(system, session).serialize();
    }

    /// @brief Restore XR state from persisted data
    [[nodiscard]] std::optional<XrSessionConfig> restore_state(
        const std::vector<std::uint8_t>& data) const {
        auto snapshot = XrSystemSnapshot::deserialize(data);
        if (!snapshot) {
            return std::nullopt;
        }
        return restore(*snapshot);
    }
};

// Global instance
static XrHotReloadManager g_xr_hot_reload;

/// @brief Get the XR hot-reload manager
XrHotReloadManager& get_xr_hot_reload_manager() {
    return g_xr_hot_reload;
}

} // namespace xr
} // namespace void_presenter
