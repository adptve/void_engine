#pragma once

/// @file handle.hpp
/// @brief Reference-counted asset handles for void_asset

#include "fwd.hpp"
#include "types.hpp"
#include <atomic>
#include <memory>
#include <functional>

namespace void_asset {

// =============================================================================
// HandleData
// =============================================================================

/// Internal data for asset handle reference counting
struct HandleData {
    std::atomic<std::uint32_t> strong_count{1};
    std::atomic<std::uint32_t> weak_count{0};
    std::atomic<std::uint32_t> generation{0};
    std::atomic<LoadState> state{LoadState::NotLoaded};
    AssetId id;

    /// Increment strong count
    void add_strong() noexcept {
        strong_count.fetch_add(1, std::memory_order_relaxed);
    }

    /// Decrement strong count, returns true if this was the last strong reference
    bool release_strong() noexcept {
        return strong_count.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }

    /// Increment weak count
    void add_weak() noexcept {
        weak_count.fetch_add(1, std::memory_order_relaxed);
    }

    /// Decrement weak count, returns true if this was the last weak reference
    bool release_weak() noexcept {
        return weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }

    /// Try to upgrade weak to strong (returns false if strong count is 0)
    bool try_upgrade() noexcept {
        std::uint32_t count = strong_count.load(std::memory_order_relaxed);
        while (count > 0) {
            if (strong_count.compare_exchange_weak(count, count + 1,
                std::memory_order_acq_rel, std::memory_order_relaxed)) {
                return true;
            }
        }
        return false;
    }

    /// Get strong count
    [[nodiscard]] std::uint32_t use_count() const noexcept {
        return strong_count.load(std::memory_order_relaxed);
    }

    /// Get current generation
    [[nodiscard]] std::uint32_t get_generation() const noexcept {
        return generation.load(std::memory_order_relaxed);
    }

    /// Increment generation (on reload)
    void increment_generation() noexcept {
        generation.fetch_add(1, std::memory_order_relaxed);
    }

    /// Get load state
    [[nodiscard]] LoadState get_state() const noexcept {
        return state.load(std::memory_order_acquire);
    }

    /// Set load state
    void set_state(LoadState s) noexcept {
        state.store(s, std::memory_order_release);
    }

    /// Check if loaded
    [[nodiscard]] bool is_loaded() const noexcept {
        return get_state() == LoadState::Loaded;
    }
};

// =============================================================================
// Handle<T>
// =============================================================================

/// Strong reference-counted handle to an asset
template<typename T>
class Handle {
public:
    /// Default constructor (null handle)
    Handle() noexcept = default;

    /// Construct from handle data
    explicit Handle(std::shared_ptr<HandleData> data, T* asset = nullptr) noexcept
        : m_data(std::move(data)), m_asset(asset) {}

    /// Copy constructor
    Handle(const Handle& other) noexcept
        : m_data(other.m_data), m_asset(other.m_asset)
    {
        if (m_data) {
            m_data->add_strong();
        }
    }

    /// Move constructor
    Handle(Handle&& other) noexcept
        : m_data(std::move(other.m_data)), m_asset(other.m_asset)
    {
        other.m_asset = nullptr;
    }

    /// Destructor
    ~Handle() {
        reset();
    }

    /// Copy assignment
    Handle& operator=(const Handle& other) noexcept {
        if (this != &other) {
            reset();
            m_data = other.m_data;
            m_asset = other.m_asset;
            if (m_data) {
                m_data->add_strong();
            }
        }
        return *this;
    }

    /// Move assignment
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            reset();
            m_data = std::move(other.m_data);
            m_asset = other.m_asset;
            other.m_asset = nullptr;
        }
        return *this;
    }

    /// Reset handle
    void reset() noexcept {
        if (m_data && m_data->release_strong()) {
            // Last strong reference, data will be cleaned up by shared_ptr
        }
        m_data.reset();
        m_asset = nullptr;
    }

    /// Get asset pointer (nullptr if not loaded)
    [[nodiscard]] T* get() const noexcept {
        return m_asset;
    }

    /// Dereference operator
    [[nodiscard]] T& operator*() const noexcept {
        return *m_asset;
    }

    /// Arrow operator
    [[nodiscard]] T* operator->() const noexcept {
        return m_asset;
    }

    /// Check if handle is valid
    [[nodiscard]] bool is_valid() const noexcept {
        return m_data != nullptr;
    }

    /// Check if asset is loaded
    [[nodiscard]] bool is_loaded() const noexcept {
        return m_data && m_data->is_loaded() && m_asset != nullptr;
    }

    /// Check if asset is loading
    [[nodiscard]] bool is_loading() const noexcept {
        return m_data && (m_data->get_state() == LoadState::Loading ||
                          m_data->get_state() == LoadState::Reloading);
    }

    /// Check if asset load failed
    [[nodiscard]] bool is_failed() const noexcept {
        return m_data && m_data->get_state() == LoadState::Failed;
    }

    /// Get load state
    [[nodiscard]] LoadState state() const noexcept {
        return m_data ? m_data->get_state() : LoadState::NotLoaded;
    }

    /// Get asset ID
    [[nodiscard]] AssetId id() const noexcept {
        return m_data ? m_data->id : AssetId::invalid();
    }

    /// Get generation
    [[nodiscard]] std::uint32_t generation() const noexcept {
        return m_data ? m_data->get_generation() : 0;
    }

    /// Get strong reference count
    [[nodiscard]] std::uint32_t use_count() const noexcept {
        return m_data ? m_data->use_count() : 0;
    }

    /// Bool conversion (true if valid)
    explicit operator bool() const noexcept {
        return is_valid();
    }

    /// Comparison
    bool operator==(const Handle& other) const noexcept {
        return m_data == other.m_data;
    }

    bool operator!=(const Handle& other) const noexcept {
        return m_data != other.m_data;
    }

    /// Update asset pointer (called by asset server on load/reload)
    void update_asset(T* asset) noexcept {
        m_asset = asset;
    }

    /// Get internal data (for advanced use)
    [[nodiscard]] std::shared_ptr<HandleData> data() const noexcept {
        return m_data;
    }

private:
    template<typename U> friend class WeakHandle;

    std::shared_ptr<HandleData> m_data;
    T* m_asset = nullptr;
};

// =============================================================================
// WeakHandle<T>
// =============================================================================

/// Weak reference to an asset (doesn't prevent unloading)
template<typename T>
class WeakHandle {
public:
    /// Default constructor
    WeakHandle() noexcept = default;

    /// Construct from strong handle
    WeakHandle(const Handle<T>& handle) noexcept
        : m_data(handle.m_data)
    {
        if (m_data) {
            m_data->add_weak();
        }
    }

    /// Copy constructor
    WeakHandle(const WeakHandle& other) noexcept
        : m_data(other.m_data)
    {
        if (m_data) {
            m_data->add_weak();
        }
    }

    /// Move constructor
    WeakHandle(WeakHandle&& other) noexcept
        : m_data(std::move(other.m_data)) {}

    /// Destructor
    ~WeakHandle() {
        if (m_data) {
            m_data->release_weak();
        }
    }

    /// Copy assignment
    WeakHandle& operator=(const WeakHandle& other) noexcept {
        if (this != &other) {
            if (m_data) {
                m_data->release_weak();
            }
            m_data = other.m_data;
            if (m_data) {
                m_data->add_weak();
            }
        }
        return *this;
    }

    /// Move assignment
    WeakHandle& operator=(WeakHandle&& other) noexcept {
        if (this != &other) {
            if (m_data) {
                m_data->release_weak();
            }
            m_data = std::move(other.m_data);
        }
        return *this;
    }

    /// Reset handle
    void reset() noexcept {
        if (m_data) {
            m_data->release_weak();
            m_data.reset();
        }
    }

    /// Try to upgrade to strong handle
    [[nodiscard]] Handle<T> lock() const noexcept {
        if (m_data && m_data->try_upgrade()) {
            return Handle<T>(m_data);
        }
        return Handle<T>{};
    }

    /// Check if handle expired (no strong references)
    [[nodiscard]] bool expired() const noexcept {
        return !m_data || m_data->use_count() == 0;
    }

    /// Get asset ID
    [[nodiscard]] AssetId id() const noexcept {
        return m_data ? m_data->id : AssetId::invalid();
    }

private:
    std::shared_ptr<HandleData> m_data;
};

// =============================================================================
// UntypedHandle
// =============================================================================

/// Type-erased handle for dynamic asset loading
class UntypedHandle {
public:
    /// Default constructor
    UntypedHandle() noexcept = default;

    /// Construct from typed handle
    template<typename T>
    UntypedHandle(const Handle<T>& handle)
        : m_data(handle.data())
        , m_asset(handle.get())
        , m_type_id(std::type_index(typeid(T))) {}

    /// Check if valid
    [[nodiscard]] bool is_valid() const noexcept {
        return m_data != nullptr;
    }

    /// Check if loaded
    [[nodiscard]] bool is_loaded() const noexcept {
        return m_data && m_data->is_loaded() && m_asset != nullptr;
    }

    /// Get load state
    [[nodiscard]] LoadState state() const noexcept {
        return m_data ? m_data->get_state() : LoadState::NotLoaded;
    }

    /// Get asset ID
    [[nodiscard]] AssetId id() const noexcept {
        return m_data ? m_data->id : AssetId::invalid();
    }

    /// Get type ID
    [[nodiscard]] std::type_index type_id() const noexcept {
        return m_type_id;
    }

    /// Try to downcast to typed handle
    template<typename T>
    [[nodiscard]] Handle<T> downcast() const {
        if (m_type_id == std::type_index(typeid(T))) {
            return Handle<T>(m_data, static_cast<T*>(m_asset));
        }
        return Handle<T>{};
    }

    /// Check if type matches
    template<typename T>
    [[nodiscard]] bool is_type() const noexcept {
        return m_type_id == std::type_index(typeid(T));
    }

private:
    std::shared_ptr<HandleData> m_data;
    void* m_asset = nullptr;
    std::type_index m_type_id{typeid(void)};
};

// =============================================================================
// AssetRef<T>
// =============================================================================

/// Component-friendly asset reference (can store path or handle)
template<typename T>
class AssetRef {
public:
    /// Default constructor
    AssetRef() = default;

    /// Construct from path
    explicit AssetRef(std::string path) : m_path(std::move(path)) {}

    /// Construct from handle
    explicit AssetRef(Handle<T> handle) : m_handle(std::move(handle)) {
        if (m_handle) {
            // Path would be retrieved from asset server
        }
    }

    /// Get path (may be empty if constructed from handle)
    [[nodiscard]] const std::string& path() const noexcept {
        return m_path;
    }

    /// Get handle (may be null if not loaded)
    [[nodiscard]] const Handle<T>& handle() const noexcept {
        return m_handle;
    }

    /// Check if loaded
    [[nodiscard]] bool is_loaded() const noexcept {
        return m_handle.is_loaded();
    }

    /// Get asset pointer
    [[nodiscard]] T* get() const noexcept {
        return m_handle.get();
    }

    /// Load using asset server
    template<typename Server>
    void load(Server& server) {
        if (!m_path.empty() && !m_handle.is_valid()) {
            m_handle = server.template load<T>(m_path);
        }
    }

    /// Set handle
    void set_handle(Handle<T> handle) {
        m_handle = std::move(handle);
    }

    /// Set path
    void set_path(std::string path) {
        m_path = std::move(path);
        m_handle.reset();
    }

private:
    std::string m_path;
    Handle<T> m_handle;
};

} // namespace void_asset
