/// @file null_backend.cpp
/// @brief Null backend implementation for void_presenter
///
/// The null backend provides a complete implementation for testing
/// and headless operation. All operations succeed but produce no
/// actual GPU output.

#include <void_engine/presenter/backends/null_backend.hpp>
#include <void_engine/presenter/backend.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace void_presenter {
namespace backends {

// =============================================================================
// Null Backend Factory Registration
// =============================================================================

namespace {

std::once_flag g_null_backend_registered;

} // anonymous namespace

void register_null_backend() {
    std::call_once(g_null_backend_registered, []() {
        BackendFactory::register_backend(BackendType::Null,
            [](const BackendConfig& /*config*/) {
                return std::make_unique<NullBackend>();
            }
        );
    });
}

bool is_null_backend_available() {
    // Null backend is always available
    return true;
}

// =============================================================================
// Null Backend Utilities
// =============================================================================

std::string format_null_backend_info(const NullBackend& backend) {
    std::string result = "NullBackend {\n";

    const auto& caps = backend.capabilities();
    result += "  type: " + std::string(to_string(caps.type)) + "\n";
    result += "  adapter: " + caps.adapter.name + "\n";
    result += "  vendor: " + caps.adapter.vendor + "\n";
    result += "  is_software: true\n";
    result += "  is_healthy: " + std::string(backend.is_healthy() ? "true" : "false") + "\n";

    result += "  supported_formats: [";
    for (std::size_t i = 0; i < caps.supported_formats.size(); ++i) {
        if (i > 0) result += ", ";
        result += to_string(caps.supported_formats[i]);
    }
    result += "]\n";

    result += "  supported_present_modes: [";
    for (std::size_t i = 0; i < caps.supported_present_modes.size(); ++i) {
        if (i > 0) result += ", ";
        result += to_string(caps.supported_present_modes[i]);
    }
    result += "]\n";

    result += "}";
    return result;
}

// =============================================================================
// Extended Null Swapchain with Frame Pacing Simulation
// =============================================================================

/// Extended null swapchain that simulates frame timing
class SimulatedNullSwapchain : public ISwapchain {
public:
    explicit SimulatedNullSwapchain(const SwapchainConfig& config, bool simulate_timing = false)
        : m_config(config)
        , m_image_index(0)
        , m_texture_id(0)
        , m_frame_count(0)
        , m_simulate_timing(simulate_timing)
    {
        // Calculate simulated frame time based on present mode
        switch (config.present_mode) {
            case PresentMode::Immediate:
                m_simulated_frame_time_us = 0;
                break;
            case PresentMode::Mailbox:
                m_simulated_frame_time_us = 8333; // ~120fps
                break;
            case PresentMode::Fifo:
                m_simulated_frame_time_us = 16667; // 60fps
                break;
            case PresentMode::FifoRelaxed:
                m_simulated_frame_time_us = 16667;
                break;
        }
    }

    [[nodiscard]] const SwapchainConfig& config() const override {
        return m_config;
    }

    bool resize(std::uint32_t width, std::uint32_t height) override {
        std::lock_guard lock(m_mutex);
        m_config.width = width;
        m_config.height = height;
        return true;
    }

    bool acquire_image(std::uint64_t /*timeout_ns*/, AcquiredImage& out_image) override {
        std::lock_guard lock(m_mutex);

        ++m_texture_id;
        m_image_index = static_cast<std::uint32_t>((m_image_index + 1) % m_config.image_count);

        out_image.texture = GpuResourceHandle{m_texture_id, BackendType::Null};
        out_image.width = m_config.width;
        out_image.height = m_config.height;
        out_image.format = m_config.format;
        out_image.image_index = m_image_index;
        out_image.suboptimal = false;
        out_image.native_handle = nullptr;

        m_acquire_time = std::chrono::steady_clock::now();

        return true;
    }

    bool present(const AcquiredImage& /*image*/) override {
        std::lock_guard lock(m_mutex);

        ++m_frame_count;

        // Simulate frame timing if enabled
        if (m_simulate_timing && m_simulated_frame_time_us > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                now - m_acquire_time).count();

            auto remaining = static_cast<std::int64_t>(m_simulated_frame_time_us) - elapsed;
            if (remaining > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(remaining));
            }
        }

        return true;
    }

    [[nodiscard]] std::uint64_t frame_count() const {
        std::lock_guard lock(m_mutex);
        return m_frame_count;
    }

    void enable_timing_simulation(bool enable) {
        std::lock_guard lock(m_mutex);
        m_simulate_timing = enable;
    }

private:
    mutable std::mutex m_mutex;
    SwapchainConfig m_config;
    std::uint32_t m_image_index;
    std::uint64_t m_texture_id;
    std::uint64_t m_frame_count;
    bool m_simulate_timing;
    std::uint64_t m_simulated_frame_time_us;
    std::chrono::steady_clock::time_point m_acquire_time;
};

// =============================================================================
// Null Backend with Statistics Tracking
// =============================================================================

/// Extended null backend that tracks usage statistics
class TrackedNullBackend : public NullBackend {
public:
    TrackedNullBackend() : NullBackend(), m_surfaces_created(0), m_wait_idle_calls(0) {}

    std::unique_ptr<IBackendSurface> create_surface(const SurfaceTarget& target) override {
        ++m_surfaces_created;
        return NullBackend::create_surface(target);
    }

    void wait_idle() override {
        ++m_wait_idle_calls;
        NullBackend::wait_idle();
    }

    [[nodiscard]] std::uint64_t surfaces_created() const { return m_surfaces_created; }
    [[nodiscard]] std::uint64_t wait_idle_calls() const { return m_wait_idle_calls; }

private:
    std::atomic<std::uint64_t> m_surfaces_created;
    std::atomic<std::uint64_t> m_wait_idle_calls;
};

// =============================================================================
// Null Backend with Simulated Failures
// =============================================================================

/// Null backend that can simulate failures for testing error handling
class FailableNullBackend : public NullBackend {
public:
    FailableNullBackend() : NullBackend() {}

    /// Configure surface creation to fail
    void set_surface_creation_fails(bool fails) {
        m_surface_creation_fails = fails;
    }

    /// Configure health check to fail
    void set_unhealthy(bool unhealthy) {
        m_unhealthy = unhealthy;
    }

    /// Set an error to return
    void set_error(const BackendError& error) {
        m_error = error;
    }

    /// Clear any set error
    void clear_error() {
        m_error.reset();
    }

    std::unique_ptr<IBackendSurface> create_surface(const SurfaceTarget& target) override {
        if (m_surface_creation_fails) {
            m_error = BackendError::surface_failed("Simulated surface creation failure");
            return nullptr;
        }
        return NullBackend::create_surface(target);
    }

    [[nodiscard]] bool is_healthy() const override {
        return !m_unhealthy;
    }

    [[nodiscard]] std::optional<BackendError> last_error() const override {
        return m_error;
    }

private:
    bool m_surface_creation_fails = false;
    bool m_unhealthy = false;
    std::optional<BackendError> m_error;
};

// =============================================================================
// Factory Functions for Test Variants
// =============================================================================

std::unique_ptr<IBackend> create_null_backend() {
    return std::make_unique<NullBackend>();
}

std::unique_ptr<IBackend> create_tracked_null_backend() {
    return std::make_unique<TrackedNullBackend>();
}

std::unique_ptr<IBackend> create_failable_null_backend() {
    return std::make_unique<FailableNullBackend>();
}

// =============================================================================
// Null Backend Hot-Reload Support
// =============================================================================

/// Dehydrate null backend state
RehydrationState dehydrate_null_backend(const NullBackend& backend) {
    RehydrationState state;

    const auto& caps = backend.capabilities();
    state.set_string("backend_type", "null");
    state.set_string("adapter_name", caps.adapter.name);
    state.set_bool("is_healthy", backend.is_healthy());

    return state;
}

/// Rehydrate null backend from state (creates new instance)
std::unique_ptr<NullBackend> rehydrate_null_backend(const RehydrationState& state) {
    // Null backend has no persistent state to restore
    // Just verify it was a null backend
    auto type = state.get_string("backend_type");
    if (!type || *type != "null") {
        return nullptr;
    }

    return std::make_unique<NullBackend>();
}

// =============================================================================
// Debug Utilities
// =============================================================================

namespace debug {

std::string format_null_swapchain(const NullSwapchain& swapchain) {
    std::string result = "NullSwapchain {\n";

    const auto& config = swapchain.config();
    result += "  size: " + std::to_string(config.width) + "x" + std::to_string(config.height) + "\n";
    result += "  format: " + std::string(to_string(config.format)) + "\n";
    result += "  present_mode: " + std::string(to_string(config.present_mode)) + "\n";
    result += "  image_count: " + std::to_string(config.image_count) + "\n";

    result += "}";
    return result;
}

std::string format_null_surface(const NullBackendSurface& surface) {
    std::string result = "NullBackendSurface {\n";

    result += "  is_valid: " + std::string(surface.is_valid() ? "true" : "false") + "\n";

    auto caps = surface.capabilities();
    result += "  formats: " + std::to_string(caps.formats.size()) + "\n";
    result += "  present_modes: " + std::to_string(caps.present_modes.size()) + "\n";

    result += "}";
    return result;
}

} // namespace debug

} // namespace backends
} // namespace void_presenter
