#pragma once

/// @file remote.hpp
/// @brief Remote asset source for consuming assets via API
///
/// Connects to a remote asset server via:
/// - WebSocket: Real-time notifications for hot-reload
/// - HTTP (libcurl): Fetching asset data
///
/// All assets are hot-swappable - when the server notifies of changes,
/// the asset is refetched and existing handles are updated.

#include "cache.hpp"
#include "types.hpp"
#include <void_engine/core/id.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace void_asset {

// =============================================================================
// Remote Configuration
// =============================================================================

/// Remote server configuration
struct RemoteConfig {
    /// Base URL for REST API (e.g., "https://assets.example.com/api/v1")
    std::string api_base_url;

    /// WebSocket URL for real-time updates (e.g., "wss://assets.example.com/ws")
    std::string websocket_url;

    /// Project ID for asset namespace
    std::string project_id;

    /// Authentication token (Bearer)
    std::string auth_token;

    /// Enable automatic reconnection
    bool auto_reconnect = true;

    /// Initial reconnect delay
    std::chrono::milliseconds reconnect_delay{1000};

    /// Maximum reconnect delay (exponential backoff cap)
    std::chrono::milliseconds max_reconnect_delay{30000};

    /// Connection timeout
    std::chrono::milliseconds connect_timeout{10000};

    /// Request timeout for HTTP operations
    std::chrono::milliseconds request_timeout{30000};

    /// Enable SSL verification
    bool verify_ssl = true;

    /// User agent string
    std::string user_agent = "void_engine/1.0";

    /// Maximum concurrent HTTP requests
    std::size_t max_concurrent_requests = 8;
};

// =============================================================================
// Remote Events
// =============================================================================

/// Types of events from the remote server
enum class RemoteEventType {
    Connected,          ///< WebSocket connected
    Disconnected,       ///< WebSocket disconnected
    Reconnecting,       ///< Attempting to reconnect
    AssetCreated,       ///< New asset available
    AssetUpdated,       ///< Asset was modified (hot-reload trigger)
    AssetDeleted,       ///< Asset was removed
    SceneCreated,       ///< New scene available
    SceneUpdated,       ///< Scene was modified
    SceneDeleted,       ///< Scene was removed
    Error,              ///< Error occurred
    Ping,               ///< Keep-alive ping
};

/// Event from the remote asset server
struct RemoteEvent {
    RemoteEventType type;
    std::string asset_path;         ///< Path of affected asset (if applicable)
    std::string message;            ///< Error message or details
    std::chrono::steady_clock::time_point timestamp;

    [[nodiscard]] static RemoteEvent connected() {
        return {RemoteEventType::Connected, {}, {}, std::chrono::steady_clock::now()};
    }

    [[nodiscard]] static RemoteEvent disconnected(std::string msg = {}) {
        return {RemoteEventType::Disconnected, {}, std::move(msg), std::chrono::steady_clock::now()};
    }

    [[nodiscard]] static RemoteEvent asset_updated(std::string path) {
        return {RemoteEventType::AssetUpdated, std::move(path), {}, std::chrono::steady_clock::now()};
    }

    [[nodiscard]] static RemoteEvent error(std::string msg) {
        return {RemoteEventType::Error, {}, std::move(msg), std::chrono::steady_clock::now()};
    }
};

// =============================================================================
// HTTP Response
// =============================================================================

/// HTTP response from asset server
struct HttpResponse {
    int status_code = 0;
    std::string status_message;
    std::unordered_map<std::string, std::string> headers;
    std::vector<std::uint8_t> body;

    [[nodiscard]] bool is_success() const { return status_code >= 200 && status_code < 300; }
    [[nodiscard]] bool is_not_modified() const { return status_code == 304; }
    [[nodiscard]] bool is_not_found() const { return status_code == 404; }

    [[nodiscard]] std::string get_header(const std::string& name) const {
        auto it = headers.find(name);
        return it != headers.end() ? it->second : "";
    }

    [[nodiscard]] std::string etag() const { return get_header("ETag"); }
    [[nodiscard]] std::string last_modified() const { return get_header("Last-Modified"); }
    [[nodiscard]] std::string content_type() const { return get_header("Content-Type"); }
};

// =============================================================================
// Fetch Request
// =============================================================================

/// Request for fetching an asset
struct FetchRequest {
    std::string path;
    CachePriority priority = CachePriority::Normal;
    std::string if_none_match;      ///< ETag for conditional request
    std::string if_modified_since;  ///< Last-Modified for conditional request
    bool force_refresh = false;     ///< Skip cache validation

    /// Callback when fetch completes
    std::function<void(std::shared_ptr<CacheEntry>, std::string error)> on_complete;
};

/// Result of a fetch operation
struct FetchResult {
    bool success = false;
    std::shared_ptr<CacheEntry> entry;
    std::string error;
    bool from_cache = false;
    bool not_modified = false;  ///< 304 response, cache is still valid
};

// =============================================================================
// Connection State
// =============================================================================

/// WebSocket connection state
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Failed,
};

/// Connection statistics
struct ConnectionStats {
    ConnectionState state = ConnectionState::Disconnected;
    std::chrono::steady_clock::time_point connected_at;
    std::chrono::steady_clock::time_point last_message_at;
    std::uint64_t messages_received = 0;
    std::uint64_t messages_sent = 0;
    std::uint64_t bytes_received = 0;
    std::uint64_t bytes_sent = 0;
    std::uint32_t reconnect_count = 0;
    std::chrono::milliseconds last_ping_rtt{0};
};

// =============================================================================
// Remote Asset Source
// =============================================================================

/// Remote asset source for fetching and subscribing to asset changes
///
/// Usage:
/// ```cpp
/// RemoteConfig config;
/// config.api_base_url = "https://assets.example.com/api/v1";
/// config.websocket_url = "wss://assets.example.com/ws";
/// config.project_id = "my-project";
/// config.auth_token = "...";
///
/// RemoteAssetSource remote(config);
/// remote.set_event_callback([](const RemoteEvent& e) {
///     if (e.type == RemoteEventType::AssetUpdated) {
///         // Trigger hot-reload for e.asset_path
///     }
/// });
///
/// remote.connect();
///
/// // Fetch an asset
/// auto future = remote.fetch_async("textures/player.png");
/// auto result = future.get();
/// if (result.success) {
///     // Use result.entry->data
/// }
/// ```
class RemoteAssetSource {
public:
    using EventCallback = std::function<void(const RemoteEvent&)>;

    /// Create remote asset source with configuration
    explicit RemoteAssetSource(RemoteConfig config, std::shared_ptr<TieredCache> cache = nullptr);

    /// Destructor - disconnects and cleans up
    ~RemoteAssetSource();

    // Non-copyable, non-movable (contains mutex)
    RemoteAssetSource(const RemoteAssetSource&) = delete;
    RemoteAssetSource& operator=(const RemoteAssetSource&) = delete;
    RemoteAssetSource(RemoteAssetSource&&) = delete;
    RemoteAssetSource& operator=(RemoteAssetSource&&) = delete;

    // =========================================================================
    // Connection Management
    // =========================================================================

    /// Connect to the remote server (WebSocket)
    /// @return true if connection initiated successfully
    bool connect();

    /// Disconnect from the server
    void disconnect();

    /// Check if connected
    [[nodiscard]] bool is_connected() const;

    /// Get connection state
    [[nodiscard]] ConnectionState connection_state() const;

    /// Get connection statistics
    [[nodiscard]] ConnectionStats connection_stats() const;

    // =========================================================================
    // Asset Fetching
    // =========================================================================

    /// Fetch an asset asynchronously
    /// @param path Asset path relative to project
    /// @param priority Cache priority hint
    /// @return Future that resolves to FetchResult
    [[nodiscard]] std::future<FetchResult> fetch_async(
        const std::string& path,
        CachePriority priority = CachePriority::Normal);

    /// Fetch an asset with full request options
    [[nodiscard]] std::future<FetchResult> fetch_async(FetchRequest request);

    /// Fetch an asset synchronously (blocking)
    [[nodiscard]] FetchResult fetch(
        const std::string& path,
        CachePriority priority = CachePriority::Normal);

    /// Prefetch assets (non-blocking, lower priority)
    void prefetch(const std::vector<std::string>& paths);

    /// Cancel pending fetch
    bool cancel_fetch(const std::string& path);

    // =========================================================================
    // Asset Listing
    // =========================================================================

    /// List assets in a directory
    [[nodiscard]] std::future<std::vector<std::string>> list_assets_async(
        const std::string& directory = "");

    /// List available scenes
    [[nodiscard]] std::future<std::vector<std::string>> list_scenes_async();

    // =========================================================================
    // Cache Management
    // =========================================================================

    /// Get the cache (may be null if not configured)
    [[nodiscard]] std::shared_ptr<TieredCache> cache() const { return m_cache; }

    /// Set the cache
    void set_cache(std::shared_ptr<TieredCache> cache) { m_cache = std::move(cache); }

    /// Invalidate cached asset (will refetch on next access)
    void invalidate(const std::string& path);

    /// Invalidate all cached assets matching a pattern
    void invalidate_pattern(const std::string& pattern);

    // =========================================================================
    // Event Handling
    // =========================================================================

    /// Set callback for remote events
    void set_event_callback(EventCallback callback);

    /// Poll for pending events (alternative to callback)
    [[nodiscard]] std::vector<RemoteEvent> poll_events();

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Get configuration
    [[nodiscard]] const RemoteConfig& config() const { return m_config; }

    /// Update authentication token
    void set_auth_token(const std::string& token);

private:
    // Internal implementation
    class Impl;
    std::unique_ptr<Impl> m_impl;

    RemoteConfig m_config;
    std::shared_ptr<TieredCache> m_cache;
    EventCallback m_event_callback;
    std::mutex m_event_mutex;
    std::deque<RemoteEvent> m_event_queue;
};

// =============================================================================
// Thread Pool for Async Operations
// =============================================================================

/// Simple thread pool for HTTP operations
class AsyncTaskPool {
public:
    using Task = std::function<void()>;

    /// Create thread pool with specified number of threads
    explicit AsyncTaskPool(std::size_t num_threads = 4);

    /// Destructor - waits for all tasks to complete
    ~AsyncTaskPool();

    // Non-copyable, non-movable
    AsyncTaskPool(const AsyncTaskPool&) = delete;
    AsyncTaskPool& operator=(const AsyncTaskPool&) = delete;

    /// Submit a task for execution
    void submit(Task task);

    /// Submit a task and get a future for the result
    template<typename F, typename R = std::invoke_result_t<F>>
    [[nodiscard]] std::future<R> submit_with_result(F&& func) {
        auto promise = std::make_shared<std::promise<R>>();
        auto future = promise->get_future();

        submit([promise, func = std::forward<F>(func)]() mutable {
            try {
                if constexpr (std::is_void_v<R>) {
                    func();
                    promise->set_value();
                } else {
                    promise->set_value(func());
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

        return future;
    }

    /// Get number of pending tasks
    [[nodiscard]] std::size_t pending_count() const;

    /// Wait for all tasks to complete
    void wait_all();

private:
    void worker_thread();

    std::vector<std::thread> m_threads;
    std::deque<Task> m_tasks;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_stop{false};
    std::atomic<std::size_t> m_pending{0};
    std::condition_variable m_done_condition;
};

// =============================================================================
// HTTP Client Interface
// =============================================================================

/// HTTP client interface (implemented with libcurl)
class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    /// Perform GET request
    [[nodiscard]] virtual HttpResponse get(
        const std::string& url,
        const std::unordered_map<std::string, std::string>& headers = {}) = 0;

    /// Perform GET request with conditional headers (If-None-Match, If-Modified-Since)
    [[nodiscard]] virtual HttpResponse get_conditional(
        const std::string& url,
        const std::string& etag,
        const std::string& last_modified) = 0;

    /// Set authentication token
    virtual void set_auth_token(const std::string& token) = 0;

    /// Set request timeout
    virtual void set_timeout(std::chrono::milliseconds timeout) = 0;
};

/// Create HTTP client using libcurl
[[nodiscard]] std::unique_ptr<IHttpClient> create_curl_client(const RemoteConfig& config);

// =============================================================================
// WebSocket Client Interface
// =============================================================================

/// WebSocket message
struct WebSocketMessage {
    enum class Type { Text, Binary, Ping, Pong, Close };
    Type type = Type::Text;
    std::vector<std::uint8_t> data;

    [[nodiscard]] std::string as_text() const {
        return std::string(data.begin(), data.end());
    }
};

/// WebSocket client interface (implemented with Beast)
class IWebSocketClient {
public:
    virtual ~IWebSocketClient() = default;

    /// Connect to WebSocket server
    [[nodiscard]] virtual bool connect(const std::string& url) = 0;

    /// Disconnect
    virtual void disconnect() = 0;

    /// Check if connected
    [[nodiscard]] virtual bool is_connected() const = 0;

    /// Send text message
    virtual void send_text(const std::string& message) = 0;

    /// Send binary message
    virtual void send_binary(const std::vector<std::uint8_t>& data) = 0;

    /// Set message callback
    virtual void set_message_callback(
        std::function<void(const WebSocketMessage&)> callback) = 0;

    /// Set close callback
    virtual void set_close_callback(
        std::function<void(int code, const std::string& reason)> callback) = 0;

    /// Set error callback
    virtual void set_error_callback(
        std::function<void(const std::string& error)> callback) = 0;

    /// Poll for events (call regularly from main thread)
    virtual void poll() = 0;
};

/// Create WebSocket client using Beast
[[nodiscard]] std::unique_ptr<IWebSocketClient> create_beast_client(const RemoteConfig& config);

} // namespace void_asset
