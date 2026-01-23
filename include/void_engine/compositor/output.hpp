#pragma once

/// @file output.hpp
/// @brief Display/output management
///
/// Provides output information and management for connected displays.

#include "fwd.hpp"
#include "types.hpp"
#include "vrr.hpp"
#include "hdr.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace void_compositor {

// =============================================================================
// Output Mode
// =============================================================================

/// Output display mode
struct OutputMode {
    /// Width in pixels
    std::uint32_t width = 0;
    /// Height in pixels
    std::uint32_t height = 0;
    /// Refresh rate in millihertz (e.g., 60000 = 60Hz)
    std::uint32_t refresh_mhz = 0;

    /// Get refresh rate in Hz
    [[nodiscard]] std::uint32_t refresh_hz() const {
        return refresh_mhz / 1000;
    }

    /// Get refresh rate as float
    [[nodiscard]] float refresh_hz_f() const {
        return static_cast<float>(refresh_mhz) / 1000.0f;
    }

    /// Check if modes are equal
    [[nodiscard]] bool operator==(const OutputMode& other) const {
        return width == other.width &&
               height == other.height &&
               refresh_mhz == other.refresh_mhz;
    }

    /// Get mode as string (e.g., "1920x1080@60Hz")
    [[nodiscard]] std::string to_string() const {
        return std::to_string(width) + "x" + std::to_string(height) +
               "@" + std::to_string(refresh_hz()) + "Hz";
    }
};

// =============================================================================
// Output Info
// =============================================================================

/// Output information
struct OutputInfo {
    /// Unique output ID
    std::uint64_t id = 0;
    /// Output name (e.g., "HDMI-A-1", "DP-1")
    std::string name;
    /// Physical size in mm (if available)
    std::optional<std::pair<std::uint32_t, std::uint32_t>> physical_size;
    /// Current mode
    OutputMode current_mode;
    /// Available modes
    std::vector<OutputMode> available_modes;
    /// Is this the primary output?
    bool primary = false;
    /// Position on virtual screen
    std::int32_t position_x = 0;
    std::int32_t position_y = 0;
    /// Scale factor
    float scale = 1.0f;
    /// Transform
    OutputTransform transform = OutputTransform::Normal;
    /// Manufacturer name
    std::string manufacturer;
    /// Model name
    std::string model;
    /// Serial number
    std::string serial;

    /// Get physical DPI (if physical size is known)
    [[nodiscard]] std::optional<float> dpi() const {
        if (!physical_size || physical_size->first == 0 || physical_size->second == 0) {
            return std::nullopt;
        }
        // Calculate diagonal size in inches
        float width_in = static_cast<float>(physical_size->first) / 25.4f;
        float height_in = static_cast<float>(physical_size->second) / 25.4f;
        float diag_in = std::sqrt(width_in * width_in + height_in * height_in);

        // Calculate diagonal pixels
        float diag_px = std::sqrt(
            static_cast<float>(current_mode.width * current_mode.width) +
            static_cast<float>(current_mode.height * current_mode.height)
        );

        return diag_px / diag_in;
    }

    /// Get aspect ratio as string
    [[nodiscard]] std::string aspect_ratio_string() const {
        auto gcd = [](std::uint32_t a, std::uint32_t b) -> std::uint32_t {
            while (b != 0) {
                auto t = b;
                b = a % b;
                a = t;
            }
            return a;
        };
        auto d = gcd(current_mode.width, current_mode.height);
        return std::to_string(current_mode.width / d) + ":" +
               std::to_string(current_mode.height / d);
    }
};

// =============================================================================
// Output Interface
// =============================================================================

/// Output interface - represents a connected display
class IOutput {
public:
    virtual ~IOutput() = default;

    /// Get output info
    [[nodiscard]] virtual const OutputInfo& info() const = 0;

    /// Get VRR capability
    [[nodiscard]] virtual const VrrCapability& vrr_capability() const = 0;

    /// Get HDR capability
    [[nodiscard]] virtual const HdrCapability& hdr_capability() const = 0;

    /// Set output mode
    virtual bool set_mode(const OutputMode& mode) = 0;

    /// Set scale factor
    virtual bool set_scale(float scale) = 0;

    /// Set transform
    virtual bool set_transform(OutputTransform transform) = 0;

    /// Set position
    virtual bool set_position(std::int32_t x, std::int32_t y) = 0;

    /// Enable VRR
    virtual bool enable_vrr(VrrMode mode) = 0;

    /// Disable VRR
    virtual bool disable_vrr() = 0;

    /// Get active VRR configuration
    [[nodiscard]] virtual std::optional<VrrConfig> vrr_config() const = 0;

    /// Enable HDR
    virtual bool enable_hdr(const HdrConfig& config) = 0;

    /// Disable HDR
    virtual bool disable_hdr() = 0;

    /// Get active HDR configuration
    [[nodiscard]] virtual std::optional<HdrConfig> hdr_config() const = 0;

    /// Set HDR metadata
    virtual bool set_hdr_metadata(const HdrConfig& config) = 0;

    /// Check if output is enabled
    [[nodiscard]] virtual bool is_enabled() const = 0;

    /// Enable output
    virtual bool enable() = 0;

    /// Disable output
    virtual bool disable() = 0;

    /// Get native handle (platform-specific)
    [[nodiscard]] virtual void* native_handle() const = 0;
};

// =============================================================================
// Null Output (for testing)
// =============================================================================

/// Null output implementation for testing
class NullOutput : public IOutput {
public:
    explicit NullOutput(const OutputInfo& info = OutputInfo{})
        : m_info(info)
    {
        // Setup default capabilities
        m_vrr_cap = VrrCapability::create_supported(48, 144, "Simulated VRR");
        m_hdr_cap = HdrCapability::hdr10_capable(1000, 0.0001f);
    }

    [[nodiscard]] const OutputInfo& info() const override { return m_info; }

    [[nodiscard]] const VrrCapability& vrr_capability() const override { return m_vrr_cap; }
    [[nodiscard]] const HdrCapability& hdr_capability() const override { return m_hdr_cap; }

    bool set_mode(const OutputMode& mode) override {
        m_info.current_mode = mode;
        return true;
    }

    bool set_scale(float scale) override {
        m_info.scale = scale;
        return true;
    }

    bool set_transform(OutputTransform transform) override {
        m_info.transform = transform;
        return true;
    }

    bool set_position(std::int32_t x, std::int32_t y) override {
        m_info.position_x = x;
        m_info.position_y = y;
        return true;
    }

    bool enable_vrr(VrrMode mode) override {
        if (!m_vrr_cap.supported) return false;
        m_vrr_config = m_vrr_cap.to_config();
        if (m_vrr_config) {
            m_vrr_config->enable(mode);
        }
        return m_vrr_config.has_value();
    }

    bool disable_vrr() override {
        m_vrr_config = std::nullopt;
        return true;
    }

    [[nodiscard]] std::optional<VrrConfig> vrr_config() const override {
        return m_vrr_config;
    }

    bool enable_hdr(const HdrConfig& config) override {
        if (!m_hdr_cap.supported) return false;
        m_hdr_config = config;
        return true;
    }

    bool disable_hdr() override {
        m_hdr_config = std::nullopt;
        return true;
    }

    [[nodiscard]] std::optional<HdrConfig> hdr_config() const override {
        return m_hdr_config;
    }

    bool set_hdr_metadata(const HdrConfig& config) override {
        return enable_hdr(config);
    }

    [[nodiscard]] bool is_enabled() const override { return m_enabled; }

    bool enable() override {
        m_enabled = true;
        return true;
    }

    bool disable() override {
        m_enabled = false;
        return true;
    }

    [[nodiscard]] void* native_handle() const override { return nullptr; }

private:
    OutputInfo m_info;
    VrrCapability m_vrr_cap;
    HdrCapability m_hdr_cap;
    std::optional<VrrConfig> m_vrr_config;
    std::optional<HdrConfig> m_hdr_config;
    bool m_enabled = true;
};

} // namespace void_compositor
