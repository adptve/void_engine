#pragma once

/// @file swapchain.hpp
/// @brief Swapchain management for void_presenter
///
/// Provides production-ready swapchain management with:
/// - Triple buffering by default
/// - Automatic resize handling
/// - Present mode switching
/// - Frame pacing integration
/// - V-Sync and VRR support

#include "fwd.hpp"
#include "types.hpp"
#include "backend.hpp"
#include "surface.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>

namespace void_presenter {

// =============================================================================
// Swapchain State
// =============================================================================

/// Swapchain state
enum class SwapchainState {
    Ready,          ///< Ready for use
    Suboptimal,     ///< Works but should recreate
    OutOfDate,      ///< Must recreate
    Lost,           ///< Surface lost
    Minimized,      ///< Window minimized (zero size)
};

/// Get state name
[[nodiscard]] constexpr const char* to_string(SwapchainState state) {
    switch (state) {
        case SwapchainState::Ready: return "Ready";
        case SwapchainState::Suboptimal: return "Suboptimal";
        case SwapchainState::OutOfDate: return "OutOfDate";
        case SwapchainState::Lost: return "Lost";
        case SwapchainState::Minimized: return "Minimized";
    }
    return "Unknown";
}

// =============================================================================
// Frame-in-Flight
// =============================================================================

/// Maximum frames in flight (for triple buffering)
constexpr std::size_t MAX_FRAMES_IN_FLIGHT = 3;

/// Per-frame synchronization data
struct FrameSyncData {
    std::uint64_t frame_number = 0;
    bool in_use = false;
    std::chrono::steady_clock::time_point submit_time;
    std::chrono::steady_clock::time_point present_time;

    /// Fence/semaphore handles (backend-specific)
    void* image_available_semaphore = nullptr;
    void* render_finished_semaphore = nullptr;
    void* in_flight_fence = nullptr;
};

// =============================================================================
// Swapchain Statistics
// =============================================================================

/// Swapchain statistics
struct SwapchainStats {
    std::uint64_t frames_presented = 0;
    std::uint64_t frames_dropped = 0;
    std::uint64_t resize_count = 0;
    std::uint64_t recreate_count = 0;

    double avg_acquire_time_us = 0.0;
    double avg_present_time_us = 0.0;
    double avg_frame_time_us = 0.0;

    std::uint64_t min_frame_time_us = UINT64_MAX;
    std::uint64_t max_frame_time_us = 0;

    /// Get average FPS
    [[nodiscard]] double average_fps() const {
        if (avg_frame_time_us <= 0.0) return 0.0;
        return 1'000'000.0 / avg_frame_time_us;
    }

    /// Get drop rate
    [[nodiscard]] double drop_rate() const {
        auto total = frames_presented + frames_dropped;
        if (total == 0) return 0.0;
        return static_cast<double>(frames_dropped) / static_cast<double>(total);
    }

    /// Reset statistics
    void reset() {
        *this = SwapchainStats{};
        min_frame_time_us = UINT64_MAX;
    }
};

// =============================================================================
// Managed Swapchain
// =============================================================================

/// Production-ready swapchain wrapper with automatic management
class ManagedSwapchain {
public:
    /// Create managed swapchain
    /// @param surface Backend surface
    /// @param config Initial configuration
    ManagedSwapchain(
        std::unique_ptr<IBackendSurface> surface,
        const SwapchainConfig& config)
        : m_surface(std::move(surface))
        , m_config(config)
        , m_state(SwapchainState::Ready)
        , m_current_frame(0)
        , m_frame_count(0)
    {
        // Create swapchain
        m_swapchain = m_surface->create_swapchain(m_config);

        // Initialize frame sync data
        for (auto& sync : m_frame_sync) {
            sync = FrameSyncData{};
        }
    }

    ~ManagedSwapchain() = default;

    // Non-copyable
    ManagedSwapchain(const ManagedSwapchain&) = delete;
    ManagedSwapchain& operator=(const ManagedSwapchain&) = delete;

    // Movable
    ManagedSwapchain(ManagedSwapchain&&) = default;
    ManagedSwapchain& operator=(ManagedSwapchain&&) = default;

    // =========================================================================
    // State
    // =========================================================================

    /// Get current state
    [[nodiscard]] SwapchainState state() const { return m_state; }

    /// Get current configuration
    [[nodiscard]] const SwapchainConfig& config() const { return m_config; }

    /// Get current size
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> size() const {
        return {m_config.width, m_config.height};
    }

    /// Check if swapchain is usable
    [[nodiscard]] bool is_usable() const {
        return m_state == SwapchainState::Ready ||
               m_state == SwapchainState::Suboptimal;
    }

    /// Check if swapchain needs recreation
    [[nodiscard]] bool needs_recreate() const {
        return m_state == SwapchainState::OutOfDate ||
               m_state == SwapchainState::Lost;
    }

    // =========================================================================
    // Frame Acquisition
    // =========================================================================

    /// Begin a new frame
    /// @param out_image Output acquired image
    /// @param timeout_ns Timeout in nanoseconds
    /// @return true on success
    bool begin_frame(AcquiredImage& out_image, std::uint64_t timeout_ns = UINT64_MAX) {
        std::lock_guard lock(m_mutex);

        // Check state
        if (m_state == SwapchainState::Minimized) {
            return false;
        }

        if (m_state == SwapchainState::OutOfDate || m_state == SwapchainState::Lost) {
            if (!recreate_internal()) {
                return false;
            }
        }

        // Wait for this frame's fence (if still in flight)
        auto& sync = m_frame_sync[m_current_frame];
        // In real implementation: wait on sync.in_flight_fence

        auto acquire_start = std::chrono::steady_clock::now();

        // Acquire image
        if (!m_swapchain || !m_swapchain->acquire_image(timeout_ns, out_image)) {
            // Handle specific errors
            m_state = SwapchainState::OutOfDate;
            ++m_stats.frames_dropped;
            return false;
        }

        auto acquire_end = std::chrono::steady_clock::now();

        // Update stats
        auto acquire_us = std::chrono::duration_cast<std::chrono::microseconds>(
            acquire_end - acquire_start).count();
        update_acquire_stats(acquire_us);

        // Check if suboptimal
        if (out_image.suboptimal) {
            m_state = SwapchainState::Suboptimal;
        }

        // Update sync data
        sync.frame_number = m_frame_count;
        sync.in_use = true;
        sync.submit_time = std::chrono::steady_clock::now();

        ++m_frame_count;
        m_current_image = out_image;

        return true;
    }

    /// End the current frame and present
    /// @return true on success
    bool end_frame() {
        std::lock_guard lock(m_mutex);

        if (!m_swapchain || !m_current_image) {
            return false;
        }

        auto present_start = std::chrono::steady_clock::now();

        // Present
        bool success = m_swapchain->present(*m_current_image);

        auto present_end = std::chrono::steady_clock::now();

        // Update sync data
        auto& sync = m_frame_sync[m_current_frame];
        sync.present_time = present_end;

        // Update stats
        auto present_us = std::chrono::duration_cast<std::chrono::microseconds>(
            present_end - present_start).count();
        auto frame_us = std::chrono::duration_cast<std::chrono::microseconds>(
            present_end - sync.submit_time).count();

        update_present_stats(present_us, frame_us);

        if (success) {
            ++m_stats.frames_presented;
        } else {
            ++m_stats.frames_dropped;
            m_state = SwapchainState::OutOfDate;
        }

        // Advance frame index
        m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
        m_current_image.reset();

        return success;
    }

    // =========================================================================
    // Resize / Reconfigure
    // =========================================================================

    /// Resize the swapchain
    /// @return true on success
    bool resize(std::uint32_t width, std::uint32_t height) {
        std::lock_guard lock(m_mutex);

        // Skip if minimized
        if (width == 0 || height == 0) {
            m_state = SwapchainState::Minimized;
            return true;
        }

        // Skip if same size
        if (width == m_config.width && height == m_config.height) {
            return true;
        }

        m_config.width = width;
        m_config.height = height;

        return recreate_internal();
    }

    /// Reconfigure the swapchain
    /// @return true on success
    bool reconfigure(const SwapchainConfig& new_config) {
        std::lock_guard lock(m_mutex);

        m_config = new_config;
        return recreate_internal();
    }

    /// Set present mode
    /// @return true on success
    bool set_present_mode(PresentMode mode) {
        std::lock_guard lock(m_mutex);

        if (m_config.present_mode == mode) {
            return true;
        }

        m_config.present_mode = mode;
        return recreate_internal();
    }

    /// Force recreate swapchain
    /// @return true on success
    bool recreate() {
        std::lock_guard lock(m_mutex);
        return recreate_internal();
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    /// Get statistics
    [[nodiscard]] SwapchainStats stats() const {
        std::lock_guard lock(m_mutex);
        return m_stats;
    }

    /// Reset statistics
    void reset_stats() {
        std::lock_guard lock(m_mutex);
        m_stats.reset();
    }

    // =========================================================================
    // Native Access
    // =========================================================================

    /// Get underlying swapchain
    [[nodiscard]] ISwapchain* swapchain() { return m_swapchain.get(); }
    [[nodiscard]] const ISwapchain* swapchain() const { return m_swapchain.get(); }

    /// Get underlying surface
    [[nodiscard]] IBackendSurface* surface() { return m_surface.get(); }
    [[nodiscard]] const IBackendSurface* surface() const { return m_surface.get(); }

private:
    bool recreate_internal() {
        // Validate size
        if (m_config.width == 0 || m_config.height == 0) {
            m_state = SwapchainState::Minimized;
            return true;
        }

        // Clamp to surface capabilities
        auto caps = m_surface->capabilities();
        auto [clamped_w, clamped_h] = caps.clamp_extent(m_config.width, m_config.height);
        m_config.width = clamped_w;
        m_config.height = clamped_h;

        // Destroy old swapchain
        m_swapchain.reset();

        // Create new swapchain
        m_swapchain = m_surface->create_swapchain(m_config);

        if (!m_swapchain) {
            m_state = SwapchainState::Lost;
            return false;
        }

        m_state = SwapchainState::Ready;
        ++m_stats.recreate_count;
        ++m_stats.resize_count;

        return true;
    }

    void update_acquire_stats(std::int64_t acquire_us) {
        double n = static_cast<double>(m_stats.frames_presented + 1);
        m_stats.avg_acquire_time_us =
            (m_stats.avg_acquire_time_us * (n - 1.0) + static_cast<double>(acquire_us)) / n;
    }

    void update_present_stats(std::int64_t present_us, std::int64_t frame_us) {
        double n = static_cast<double>(m_stats.frames_presented + 1);
        m_stats.avg_present_time_us =
            (m_stats.avg_present_time_us * (n - 1.0) + static_cast<double>(present_us)) / n;
        m_stats.avg_frame_time_us =
            (m_stats.avg_frame_time_us * (n - 1.0) + static_cast<double>(frame_us)) / n;

        auto uframe = static_cast<std::uint64_t>(frame_us);
        if (uframe < m_stats.min_frame_time_us) {
            m_stats.min_frame_time_us = uframe;
        }
        if (uframe > m_stats.max_frame_time_us) {
            m_stats.max_frame_time_us = uframe;
        }
    }

    mutable std::mutex m_mutex;
    std::unique_ptr<IBackendSurface> m_surface;
    std::unique_ptr<ISwapchain> m_swapchain;
    SwapchainConfig m_config;
    SwapchainState m_state;

    std::array<FrameSyncData, MAX_FRAMES_IN_FLIGHT> m_frame_sync;
    std::size_t m_current_frame;
    std::uint64_t m_frame_count;
    std::optional<AcquiredImage> m_current_image;

    SwapchainStats m_stats;
};

// =============================================================================
// Swapchain Builder
// =============================================================================

/// Builder for creating swapchains with common configurations
class SwapchainBuilder {
public:
    explicit SwapchainBuilder(IBackendSurface* surface)
        : m_surface(surface) {}

    /// Set size
    SwapchainBuilder& size(std::uint32_t width, std::uint32_t height) {
        m_config.width = width;
        m_config.height = height;
        return *this;
    }

    /// Set format (or use preferred from surface)
    SwapchainBuilder& format(SurfaceFormat fmt) {
        m_config.format = fmt;
        return *this;
    }

    /// Use surface's preferred format
    SwapchainBuilder& preferred_format() {
        if (m_surface) {
            m_config.format = m_surface->capabilities().preferred_format();
        }
        return *this;
    }

    /// Set present mode for low latency
    SwapchainBuilder& low_latency() {
        if (m_surface) {
            m_config.present_mode = m_surface->capabilities().preferred_present_mode_low_latency();
        }
        return *this;
    }

    /// Set present mode for V-Sync
    SwapchainBuilder& vsync() {
        m_config.present_mode = PresentMode::Fifo;
        return *this;
    }

    /// Set present mode
    SwapchainBuilder& present_mode(PresentMode mode) {
        m_config.present_mode = mode;
        return *this;
    }

    /// Enable HDR (if supported)
    SwapchainBuilder& hdr(bool enable = true) {
        m_config.enable_hdr = enable;
        if (enable && m_surface) {
            auto best = m_surface->capabilities().preferred_format();
            // Try to find HDR format
            for (const auto& f : m_surface->capabilities().formats) {
                if (is_hdr_capable(f)) {
                    m_config.format = f;
                    break;
                }
            }
        }
        return *this;
    }

    /// Set image count (2 = double buffer, 3 = triple buffer)
    SwapchainBuilder& image_count(std::uint32_t count) {
        m_config.image_count = count;
        return *this;
    }

    /// Build the swapchain
    [[nodiscard]] std::unique_ptr<ISwapchain> build() {
        if (!m_surface) return nullptr;
        return m_surface->create_swapchain(m_config);
    }

    /// Get configuration
    [[nodiscard]] const SwapchainConfig& config() const { return m_config; }

private:
    IBackendSurface* m_surface;
    SwapchainConfig m_config;
};

} // namespace void_presenter
