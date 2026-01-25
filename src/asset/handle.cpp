/// @file handle.cpp
/// @brief void_asset handle implementation
///
/// Provides non-template utilities for the handle system.
/// Core handle functionality is template-based in the header.

#include <void_engine/asset/handle.hpp>

#include <sstream>

namespace void_asset {

// =============================================================================
// HandleData Utilities
// =============================================================================

std::shared_ptr<HandleData> create_handle_data(AssetId id, LoadState initial_state) {
    auto data = std::make_shared<HandleData>();
    data->id = id;
    data->set_state(initial_state);
    data->generation.store(0);
    return data;
}

// =============================================================================
// Debug Utilities
// =============================================================================

namespace debug {

std::string format_handle_data(const HandleData& data) {
    std::ostringstream oss;
    oss << "HandleData {\n";
    oss << "  id: " << data.id.raw() << "\n";
    oss << "  strong_count: " << data.use_count() << "\n";
    oss << "  weak_count: " << data.weak_count.load() << "\n";
    oss << "  generation: " << data.get_generation() << "\n";
    oss << "  state: " << load_state_name(data.get_state()) << "\n";
    oss << "}";
    return oss.str();
}

std::string format_load_state(LoadState state) {
    return load_state_name(state);
}

std::string format_untyped_handle(const UntypedHandle& handle) {
    std::ostringstream oss;
    oss << "UntypedHandle {\n";
    oss << "  valid: " << (handle.is_valid() ? "true" : "false") << "\n";
    oss << "  loaded: " << (handle.is_loaded() ? "true" : "false") << "\n";
    oss << "  state: " << load_state_name(handle.state()) << "\n";
    oss << "  id: " << handle.id().raw() << "\n";
    oss << "  type: " << handle.type_id().name() << "\n";
    oss << "}";
    return oss.str();
}

} // namespace debug

// =============================================================================
// Handle State Utilities
// =============================================================================

bool is_terminal_state(LoadState state) {
    return state == LoadState::Loaded || state == LoadState::Failed;
}

bool is_loading_state(LoadState state) {
    return state == LoadState::Loading || state == LoadState::Reloading;
}

bool can_transition_to(LoadState from, LoadState to) {
    switch (from) {
        case LoadState::NotLoaded:
            return to == LoadState::Loading;
        case LoadState::Loading:
            return to == LoadState::Loaded || to == LoadState::Failed;
        case LoadState::Loaded:
            return to == LoadState::Reloading || to == LoadState::NotLoaded;
        case LoadState::Failed:
            return to == LoadState::Loading || to == LoadState::NotLoaded;
        case LoadState::Reloading:
            return to == LoadState::Loaded || to == LoadState::Failed;
        default:
            return false;
    }
}

// =============================================================================
// Handle Pool (for pre-allocated handles)
// =============================================================================

namespace {

class HandleDataPool {
public:
    static HandleDataPool& instance() {
        static HandleDataPool pool;
        return pool;
    }

    std::shared_ptr<HandleData> acquire() {
        std::lock_guard lock(m_mutex);

        if (!m_free_list.empty()) {
            auto data = std::move(m_free_list.back());
            m_free_list.pop_back();
            // Reset the data
            data->strong_count.store(1);
            data->weak_count.store(0);
            data->generation.fetch_add(1);
            data->state.store(LoadState::NotLoaded);
            return data;
        }

        return std::make_shared<HandleData>();
    }

    void release(std::shared_ptr<HandleData> data) {
        if (!data) return;

        std::lock_guard lock(m_mutex);
        if (m_free_list.size() < m_max_pool_size) {
            m_free_list.push_back(std::move(data));
        }
    }

    void set_max_pool_size(std::size_t size) {
        std::lock_guard lock(m_mutex);
        m_max_pool_size = size;
        while (m_free_list.size() > m_max_pool_size) {
            m_free_list.pop_back();
        }
    }

    std::size_t pool_size() const {
        std::lock_guard lock(m_mutex);
        return m_free_list.size();
    }

    void clear() {
        std::lock_guard lock(m_mutex);
        m_free_list.clear();
    }

private:
    HandleDataPool() = default;

    mutable std::mutex m_mutex;
    std::vector<std::shared_ptr<HandleData>> m_free_list;
    std::size_t m_max_pool_size = 1024;
};

} // anonymous namespace

std::shared_ptr<HandleData> acquire_handle_data() {
    return HandleDataPool::instance().acquire();
}

void release_handle_data(std::shared_ptr<HandleData> data) {
    HandleDataPool::instance().release(std::move(data));
}

std::size_t handle_pool_size() {
    return HandleDataPool::instance().pool_size();
}

void clear_handle_pool() {
    HandleDataPool::instance().clear();
}

void set_handle_pool_max_size(std::size_t size) {
    HandleDataPool::instance().set_max_pool_size(size);
}

} // namespace void_asset
