#pragma once

/// @file surface.hpp
/// @brief Surface abstraction for void_presenter
///
/// Represents the renderable target (window, canvas, XR session).

#include "fwd.hpp"
#include "types.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

namespace void_presenter {

// =============================================================================
// Surface Error
// =============================================================================

/// Surface error types
enum class SurfaceErrorKind {
    CreationFailed,  ///< Surface creation failed
    Lost,            ///< Surface was lost
    Outdated,        ///< Surface is outdated (needs reconfigure)
    Timeout,         ///< Operation timed out
};

/// Surface error
struct SurfaceError {
    SurfaceErrorKind kind;
    std::string message;

    [[nodiscard]] static SurfaceError creation_failed(std::string msg) {
        return {SurfaceErrorKind::CreationFailed, std::move(msg)};
    }

    [[nodiscard]] static SurfaceError lost() {
        return {SurfaceErrorKind::Lost, "Surface lost"};
    }

    [[nodiscard]] static SurfaceError outdated() {
        return {SurfaceErrorKind::Outdated, "Surface outdated"};
    }

    [[nodiscard]] static SurfaceError timeout() {
        return {SurfaceErrorKind::Timeout, "Surface timeout"};
    }
};

// =============================================================================
// Surface Configuration
// =============================================================================

/// Surface configuration
struct SurfaceConfig {
    std::uint32_t width = 800;              ///< Width in pixels
    std::uint32_t height = 600;             ///< Height in pixels
    SurfaceFormat format = SurfaceFormat::Bgra8UnormSrgb;  ///< Pixel format
    VSync vsync = VSync::Adaptive;                         ///< VSync mode
    PresentMode present_mode = PresentMode::Fifo;          ///< Presentation mode
    AlphaMode alpha_mode = AlphaMode::Opaque;              ///< Alpha blending
    std::uint32_t desired_frame_latency = 2;               ///< Max frame latency

    /// Builder pattern - set size
    [[nodiscard]] SurfaceConfig with_size(std::uint32_t w, std::uint32_t h) const {
        SurfaceConfig copy = *this;
        copy.width = w;
        copy.height = h;
        return copy;
    }

    /// Builder pattern - set format
    [[nodiscard]] SurfaceConfig with_format(SurfaceFormat f) const {
        SurfaceConfig copy = *this;
        copy.format = f;
        return copy;
    }

    /// Builder pattern - set present mode
    [[nodiscard]] SurfaceConfig with_present_mode(PresentMode mode) const {
        SurfaceConfig copy = *this;
        copy.present_mode = mode;
        return copy;
    }

    /// Builder pattern - set alpha mode
    [[nodiscard]] SurfaceConfig with_alpha_mode(AlphaMode mode) const {
        SurfaceConfig copy = *this;
        copy.alpha_mode = mode;
        return copy;
    }

    /// Get aspect ratio
    [[nodiscard]] float aspect_ratio() const {
        return static_cast<float>(width) / static_cast<float>(std::max(height, 1u));
    }
};

// =============================================================================
// Surface Capabilities
// =============================================================================

/// Surface capabilities
struct SurfaceCapabilities {
    std::vector<SurfaceFormat> formats;        ///< Supported formats
    std::vector<PresentMode> present_modes;    ///< Supported present modes
    std::vector<AlphaMode> alpha_modes;        ///< Supported alpha modes
    std::uint32_t min_width = 1;               ///< Minimum width
    std::uint32_t min_height = 1;              ///< Minimum height
    std::uint32_t max_width = 16384;           ///< Maximum width
    std::uint32_t max_height = 16384;          ///< Maximum height

    /// Default capabilities
    [[nodiscard]] static SurfaceCapabilities default_caps() {
        return {
            .formats = {SurfaceFormat::Bgra8UnormSrgb},
            .present_modes = {PresentMode::Fifo},
            .alpha_modes = {AlphaMode::Opaque},
            .min_width = 1,
            .min_height = 1,
            .max_width = 16384,
            .max_height = 16384,
        };
    }

    /// Check if format is supported
    [[nodiscard]] bool supports_format(SurfaceFormat format) const {
        return std::find(formats.begin(), formats.end(), format) != formats.end();
    }

    /// Check if present mode is supported
    [[nodiscard]] bool supports_present_mode(PresentMode mode) const {
        return std::find(present_modes.begin(), present_modes.end(), mode) !=
               present_modes.end();
    }

    /// Get preferred format (prefer sRGB)
    [[nodiscard]] SurfaceFormat preferred_format() const {
        // Prefer sRGB formats
        for (const auto& f : formats) {
            if (is_srgb(f)) return f;
        }
        return formats.empty() ? SurfaceFormat::Bgra8UnormSrgb : formats[0];
    }

    /// Get preferred present mode for low latency
    [[nodiscard]] PresentMode preferred_present_mode_low_latency() const {
        if (supports_present_mode(PresentMode::Mailbox)) {
            return PresentMode::Mailbox;
        }
        if (supports_present_mode(PresentMode::Immediate)) {
            return PresentMode::Immediate;
        }
        return PresentMode::Fifo;
    }

    /// Get preferred present mode for VSync
    [[nodiscard]] PresentMode preferred_present_mode_vsync() const {
        if (supports_present_mode(PresentMode::Fifo)) {
            return PresentMode::Fifo;
        }
        return present_modes.empty() ? PresentMode::Fifo : present_modes[0];
    }

    /// Clamp extent to supported range
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> clamp_extent(
        std::uint32_t width, std::uint32_t height) const {
        return {
            std::clamp(width, min_width, max_width),
            std::clamp(height, min_height, max_height)
        };
    }
};

// =============================================================================
// Surface Texture
// =============================================================================

/// Surface texture handle
struct SurfaceTexture {
    std::uint64_t id = 0;                       ///< Texture identifier
    std::uint32_t width = 0;                    ///< Texture width
    std::uint32_t height = 0;                   ///< Texture height
    SurfaceFormat format = SurfaceFormat::Bgra8UnormSrgb;  ///< Pixel format
    bool suboptimal = false;                    ///< Should reconfigure soon
    void* native_handle = nullptr;              ///< Backend-specific handle

    /// Create a new surface texture
    [[nodiscard]] static SurfaceTexture create(
        std::uint64_t tex_id,
        std::uint32_t w,
        std::uint32_t h,
        SurfaceFormat fmt) {
        return {tex_id, w, h, fmt, false};
    }

    /// Mark as suboptimal
    [[nodiscard]] SurfaceTexture with_suboptimal(bool is_suboptimal) const {
        SurfaceTexture copy = *this;
        copy.suboptimal = is_suboptimal;
        return copy;
    }

    /// Get size as pair
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> size() const {
        return {width, height};
    }
};

// =============================================================================
// Surface Interface
// =============================================================================

/// Abstract surface interface
class ISurface {
public:
    virtual ~ISurface() = default;

    /// Get current configuration
    [[nodiscard]] virtual const SurfaceConfig& config() const = 0;

    /// Get capabilities
    [[nodiscard]] virtual const SurfaceCapabilities& capabilities() const = 0;

    /// Get current state
    [[nodiscard]] virtual SurfaceState state() const = 0;

    /// Configure the surface
    /// @return true on success
    virtual bool configure(const SurfaceConfig& config) = 0;

    /// Get current texture for rendering
    /// @param out_texture Output texture handle
    /// @return true on success, false with error info
    virtual bool get_current_texture(SurfaceTexture& out_texture) = 0;

    /// Present the current texture
    virtual void present() = 0;

    /// Get current size
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> size() const {
        return {config().width, config().height};
    }

    /// Check if surface is ready
    [[nodiscard]] bool is_ready() const {
        return state() == SurfaceState::Ready;
    }
};

// =============================================================================
// Null Surface (for testing)
// =============================================================================

/// Null surface for testing
class NullSurface : public ISurface {
public:
    NullSurface() = default;

    explicit NullSurface(const SurfaceConfig& cfg)
        : m_config(cfg) {}

    [[nodiscard]] const SurfaceConfig& config() const override {
        return m_config;
    }

    [[nodiscard]] const SurfaceCapabilities& capabilities() const override {
        return m_capabilities;
    }

    [[nodiscard]] SurfaceState state() const override {
        return m_state;
    }

    bool configure(const SurfaceConfig& cfg) override {
        m_config = cfg;
        m_state = SurfaceState::Ready;
        return true;
    }

    bool get_current_texture(SurfaceTexture& out_texture) override {
        ++m_texture_id;
        out_texture = SurfaceTexture::create(
            m_texture_id,
            m_config.width,
            m_config.height,
            m_config.format
        );
        return true;
    }

    void present() override {
        // No-op for null surface
    }

    /// Set state (for testing)
    void set_state(SurfaceState s) { m_state = s; }

private:
    SurfaceConfig m_config;
    SurfaceCapabilities m_capabilities = SurfaceCapabilities::default_caps();
    SurfaceState m_state = SurfaceState::Ready;
    std::uint64_t m_texture_id = 0;
};

} // namespace void_presenter
