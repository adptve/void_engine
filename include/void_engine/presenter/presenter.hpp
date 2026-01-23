#pragma once

/// @file presenter.hpp
/// @brief Presenter interface and management for void_presenter
///
/// Provides the core Presenter abstraction for frame output to displays.

#include "fwd.hpp"
#include "types.hpp"
#include "surface.hpp"
#include "frame.hpp"
#include "rehydration.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

namespace void_presenter {

// =============================================================================
// Presenter Error
// =============================================================================

/// Presenter error types
enum class PresenterErrorKind {
    SurfaceCreation,    ///< Surface creation failed
    SurfaceLost,        ///< Surface was lost
    FrameAcquisition,   ///< Frame acquisition failed
    PresentationFailed, ///< Presentation failed
    BackendNotAvailable,///< Backend not available
    ConfigError,        ///< Configuration error
    RehydrationFailed,  ///< Rehydration failed
};

/// Presenter error
struct PresenterError {
    PresenterErrorKind kind;
    std::string message;

    [[nodiscard]] static PresenterError surface_creation(std::string msg) {
        return {PresenterErrorKind::SurfaceCreation, std::move(msg)};
    }

    [[nodiscard]] static PresenterError surface_lost() {
        return {PresenterErrorKind::SurfaceLost, "Surface lost"};
    }

    [[nodiscard]] static PresenterError frame_acquisition(std::string msg) {
        return {PresenterErrorKind::FrameAcquisition, std::move(msg)};
    }

    [[nodiscard]] static PresenterError presentation_failed(std::string msg) {
        return {PresenterErrorKind::PresentationFailed, std::move(msg)};
    }

    [[nodiscard]] static PresenterError backend_not_available(std::string msg) {
        return {PresenterErrorKind::BackendNotAvailable, std::move(msg)};
    }

    [[nodiscard]] static PresenterError config_error(std::string msg) {
        return {PresenterErrorKind::ConfigError, std::move(msg)};
    }

    [[nodiscard]] static PresenterError rehydration_failed(std::string msg) {
        return {PresenterErrorKind::RehydrationFailed, std::move(msg)};
    }
};

// =============================================================================
// Presenter ID
// =============================================================================

/// Unique presenter identifier
struct PresenterId {
    std::uint64_t id = 0;

    PresenterId() = default;
    explicit PresenterId(std::uint64_t value) : id(value) {}

    [[nodiscard]] bool is_valid() const { return id != 0; }

    bool operator==(const PresenterId& other) const { return id == other.id; }
    bool operator!=(const PresenterId& other) const { return id != other.id; }
    bool operator<(const PresenterId& other) const { return id < other.id; }
};

// =============================================================================
// Presenter Capabilities
// =============================================================================

/// Presenter capabilities
struct PresenterCapabilities {
    std::vector<PresentMode> present_modes;     ///< Supported present modes
    std::vector<SurfaceFormat> formats;         ///< Supported formats
    std::uint32_t max_width = 4096;             ///< Maximum width
    std::uint32_t max_height = 4096;            ///< Maximum height
    bool hdr_support = false;                   ///< HDR support
    bool vrr_support = false;                   ///< Variable refresh rate support
    bool xr_passthrough = false;                ///< XR passthrough support

    /// Default capabilities
    [[nodiscard]] static PresenterCapabilities default_caps() {
        return {
            .present_modes = {PresentMode::Fifo},
            .formats = {SurfaceFormat::Bgra8UnormSrgb},
            .max_width = 4096,
            .max_height = 4096,
            .hdr_support = false,
            .vrr_support = false,
            .xr_passthrough = false,
        };
    }

    /// Get max resolution
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> max_resolution() const {
        return {max_width, max_height};
    }
};

// =============================================================================
// Presenter Configuration
// =============================================================================

/// Presenter configuration
struct PresenterConfig {
    SurfaceFormat format = SurfaceFormat::Bgra8UnormSrgb;  ///< Surface format
    PresentMode present_mode = PresentMode::Fifo;          ///< Present mode
    std::uint32_t width = 1920;                            ///< Initial width
    std::uint32_t height = 1080;                           ///< Initial height
    bool enable_hdr = false;                               ///< Enable HDR if available
    std::uint32_t target_frame_rate = 60;                  ///< Target frame rate (0 = unlimited)
    bool allow_tearing = false;                            ///< Allow tearing

    /// Get size as pair
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> size() const {
        return {width, height};
    }

    /// Builder pattern
    [[nodiscard]] PresenterConfig with_size(std::uint32_t w, std::uint32_t h) const {
        PresenterConfig copy = *this;
        copy.width = w;
        copy.height = h;
        return copy;
    }

    [[nodiscard]] PresenterConfig with_format(SurfaceFormat f) const {
        PresenterConfig copy = *this;
        copy.format = f;
        return copy;
    }

    [[nodiscard]] PresenterConfig with_present_mode(PresentMode mode) const {
        PresenterConfig copy = *this;
        copy.present_mode = mode;
        return copy;
    }

    [[nodiscard]] PresenterConfig with_hdr(bool enabled) const {
        PresenterConfig copy = *this;
        copy.enable_hdr = enabled;
        return copy;
    }

    [[nodiscard]] PresenterConfig with_target_fps(std::uint32_t fps) const {
        PresenterConfig copy = *this;
        copy.target_frame_rate = fps;
        return copy;
    }
};

// =============================================================================
// Presenter Interface
// =============================================================================

/// Abstract presenter interface
class IPresenter : public IRehydratable {
public:
    virtual ~IPresenter() = default;

    /// Get presenter ID
    [[nodiscard]] virtual PresenterId id() const = 0;

    /// Get capabilities
    [[nodiscard]] virtual const PresenterCapabilities& capabilities() const = 0;

    /// Get current configuration
    [[nodiscard]] virtual const PresenterConfig& config() const = 0;

    /// Reconfigure the presenter
    /// @return true on success
    virtual bool reconfigure(const PresenterConfig& config) = 0;

    /// Resize the surface
    /// @return true on success
    virtual bool resize(std::uint32_t width, std::uint32_t height) = 0;

    /// Begin a new frame
    /// @param out_frame Output frame
    /// @return true on success
    virtual bool begin_frame(Frame& out_frame) = 0;

    /// Present a frame
    /// @return true on success
    virtual bool present(Frame& frame) = 0;

    /// Get current surface size
    [[nodiscard]] virtual std::pair<std::uint32_t, std::uint32_t> size() const = 0;

    /// Check if presenter is valid/healthy
    [[nodiscard]] virtual bool is_valid() const = 0;
};

// =============================================================================
// Null Presenter (for testing)
// =============================================================================

/// Null presenter for testing
class NullPresenter : public IPresenter {
public:
    explicit NullPresenter(PresenterId presenter_id)
        : m_id(presenter_id)
        , m_capabilities(PresenterCapabilities::default_caps())
        , m_frame_number(0) {}

    [[nodiscard]] PresenterId id() const override {
        return m_id;
    }

    [[nodiscard]] const PresenterCapabilities& capabilities() const override {
        return m_capabilities;
    }

    [[nodiscard]] const PresenterConfig& config() const override {
        return m_config;
    }

    bool reconfigure(const PresenterConfig& cfg) override {
        m_config = cfg;
        return true;
    }

    bool resize(std::uint32_t width, std::uint32_t height) override {
        m_config.width = width;
        m_config.height = height;
        return true;
    }

    bool begin_frame(Frame& out_frame) override {
        ++m_frame_number;
        out_frame = Frame(m_frame_number, m_config.width, m_config.height);
        if (m_config.target_frame_rate > 0) {
            out_frame.set_target_fps(m_config.target_frame_rate);
        }
        return true;
    }

    bool present(Frame& frame) override {
        frame.mark_presented();
        return true;
    }

    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> size() const override {
        return {m_config.width, m_config.height};
    }

    [[nodiscard]] bool is_valid() const override {
        return true;
    }

    // IRehydratable
    [[nodiscard]] RehydrationState dehydrate() const override {
        RehydrationState state;
        state.with_uint("frame_number", m_frame_number);
        state.with_uint("width", m_config.width);
        state.with_uint("height", m_config.height);
        return state;
    }

    bool rehydrate(const RehydrationState& state) override {
        if (auto v = state.get_uint("frame_number")) {
            m_frame_number = *v;
        }
        if (auto v = state.get_uint("width")) {
            m_config.width = static_cast<std::uint32_t>(*v);
        }
        if (auto v = state.get_uint("height")) {
            m_config.height = static_cast<std::uint32_t>(*v);
        }
        return true;
    }

private:
    PresenterId m_id;
    PresenterCapabilities m_capabilities;
    PresenterConfig m_config;
    std::uint64_t m_frame_number;
};

// =============================================================================
// Presenter Manager
// =============================================================================

/// Manages multiple presenters
class PresenterManager {
public:
    PresenterManager() = default;

    ~PresenterManager() {
        // Cleanup
        std::unique_lock lock(m_mutex);
        m_presenters.clear();
    }

    // Non-copyable
    PresenterManager(const PresenterManager&) = delete;
    PresenterManager& operator=(const PresenterManager&) = delete;

    // =========================================================================
    // ID Allocation
    // =========================================================================

    /// Allocate a new presenter ID
    [[nodiscard]] PresenterId allocate_id() {
        return PresenterId(m_next_id.fetch_add(1, std::memory_order_relaxed));
    }

    // =========================================================================
    // Registration
    // =========================================================================

    /// Register a presenter
    /// @return The presenter's ID
    PresenterId register_presenter(std::unique_ptr<IPresenter> presenter) {
        if (!presenter) return PresenterId{};

        auto id = presenter->id();
        std::unique_lock lock(m_mutex);

        // Set as primary if first
        if (m_presenters.empty()) {
            m_primary_index = 0;
        }

        m_presenters.push_back(std::move(presenter));
        return id;
    }

    /// Unregister a presenter
    /// @return The removed presenter, or nullptr
    std::unique_ptr<IPresenter> unregister(PresenterId id) {
        std::unique_lock lock(m_mutex);

        auto it = std::find_if(m_presenters.begin(), m_presenters.end(),
            [id](const auto& p) { return p->id() == id; });

        if (it == m_presenters.end()) {
            return nullptr;
        }

        std::size_t pos = static_cast<std::size_t>(it - m_presenters.begin());
        auto presenter = std::move(*it);
        m_presenters.erase(it);

        // Update primary if needed
        if (m_primary_index) {
            if (*m_primary_index == pos) {
                m_primary_index = m_presenters.empty() ?
                    std::nullopt : std::optional<std::size_t>{0};
            } else if (*m_primary_index > pos) {
                --(*m_primary_index);
            }
        }

        return presenter;
    }

    // =========================================================================
    // Access
    // =========================================================================

    /// Get presenter by ID
    [[nodiscard]] IPresenter* get(PresenterId id) {
        std::shared_lock lock(m_mutex);
        for (auto& p : m_presenters) {
            if (p->id() == id) return p.get();
        }
        return nullptr;
    }

    /// Get presenter by ID (const)
    [[nodiscard]] const IPresenter* get(PresenterId id) const {
        std::shared_lock lock(m_mutex);
        for (const auto& p : m_presenters) {
            if (p->id() == id) return p.get();
        }
        return nullptr;
    }

    /// Get primary presenter
    [[nodiscard]] IPresenter* primary() {
        std::shared_lock lock(m_mutex);
        if (!m_primary_index || m_presenters.empty()) return nullptr;
        return m_presenters[*m_primary_index].get();
    }

    /// Get primary presenter (const)
    [[nodiscard]] const IPresenter* primary() const {
        std::shared_lock lock(m_mutex);
        if (!m_primary_index || m_presenters.empty()) return nullptr;
        return m_presenters[*m_primary_index].get();
    }

    /// Set primary presenter
    /// @return true if presenter found and set
    bool set_primary(PresenterId id) {
        std::unique_lock lock(m_mutex);
        for (std::size_t i = 0; i < m_presenters.size(); ++i) {
            if (m_presenters[i]->id() == id) {
                m_primary_index = i;
                return true;
            }
        }
        return false;
    }

    /// Get all presenter IDs
    [[nodiscard]] std::vector<PresenterId> all_ids() const {
        std::shared_lock lock(m_mutex);
        std::vector<PresenterId> result;
        result.reserve(m_presenters.size());
        for (const auto& p : m_presenters) {
            result.push_back(p->id());
        }
        return result;
    }

    /// Get presenter count
    [[nodiscard]] std::size_t count() const {
        std::shared_lock lock(m_mutex);
        return m_presenters.size();
    }

    // =========================================================================
    // Batch Operations
    // =========================================================================

    /// Begin frames on all presenters
    /// @return Vector of (id, frame) pairs for successful frames
    [[nodiscard]] std::vector<std::pair<PresenterId, Frame>> begin_all_frames() {
        std::unique_lock lock(m_mutex);
        std::vector<std::pair<PresenterId, Frame>> results;
        results.reserve(m_presenters.size());

        for (auto& p : m_presenters) {
            Frame frame(0, 0, 0);  // Dummy, will be replaced
            if (p->begin_frame(frame)) {
                results.emplace_back(p->id(), std::move(frame));
            }
        }
        return results;
    }

    /// Present on all presenters
    void present_all(std::vector<std::pair<PresenterId, Frame>>& frames) {
        std::unique_lock lock(m_mutex);

        for (auto& [id, frame] : frames) {
            for (auto& p : m_presenters) {
                if (p->id() == id) {
                    p->present(frame);
                    break;
                }
            }
        }
    }

    // =========================================================================
    // Rehydration
    // =========================================================================

    /// Get rehydration states for all presenters
    [[nodiscard]] std::vector<std::pair<PresenterId, RehydrationState>> rehydration_states() const {
        std::shared_lock lock(m_mutex);
        std::vector<std::pair<PresenterId, RehydrationState>> results;
        results.reserve(m_presenters.size());

        for (const auto& p : m_presenters) {
            results.emplace_back(p->id(), p->dehydrate());
        }
        return results;
    }

private:
    mutable std::shared_mutex m_mutex;
    std::vector<std::unique_ptr<IPresenter>> m_presenters;
    std::optional<std::size_t> m_primary_index;
    std::atomic<std::uint64_t> m_next_id{1};
};

} // namespace void_presenter

// Hash specialization for PresenterId
template<>
struct std::hash<void_presenter::PresenterId> {
    std::size_t operator()(const void_presenter::PresenterId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.id);
    }
};
