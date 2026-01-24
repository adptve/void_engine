#pragma once

/// @file snapshot.hpp
/// @brief XR hot-reload snapshot system for void_xr
///
/// Provides state preservation for XR sessions during hot-reload:
/// - Session configuration
/// - Last known poses (head, controllers)
/// - Reference space settings
/// - Foveation configuration

#include "xr.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace void_xr {

// =============================================================================
// XR Session Snapshot
// =============================================================================

/// @brief Snapshot of XR session state for hot-reload
struct XrSessionSnapshot {
    XrSessionConfig config;
    XrSessionState state = XrSessionState::Idle;
    ReferenceSpaceType reference_space = ReferenceSpaceType::LocalFloor;
    FoveatedRenderingConfig foveation;

    /// Last known head pose for smooth transition
    Pose last_head_pose;

    /// Last known controller poses
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
// Hot-Reload Functions
// =============================================================================

/// @brief Capture XR state for hot-reload
/// @param system Current XR system (may be nullptr)
/// @param session Current XR session (may be nullptr)
/// @return Snapshot of current state
[[nodiscard]] XrSystemSnapshot capture_xr_state(
    IXrSystem* system,
    IXrSession* session);

/// @brief Restore XR session config from snapshot
/// @param snapshot Previously captured snapshot
/// @return Session config to use for recreation, or nullopt if invalid
[[nodiscard]] std::optional<XrSessionConfig> restore_xr_config(
    const XrSystemSnapshot& snapshot);

/// @brief Serialize XR state to binary for persistence
/// @param system Current XR system
/// @param session Current XR session
/// @return Binary data
[[nodiscard]] std::vector<std::uint8_t> serialize_xr_state(
    IXrSystem* system,
    IXrSession* session);

/// @brief Restore XR config from binary data
/// @param data Binary data from serialize_xr_state
/// @return Session config, or nullopt if data is invalid
[[nodiscard]] std::optional<XrSessionConfig> restore_xr_state(
    const std::vector<std::uint8_t>& data);

} // namespace void_xr
