/// @file remote.cpp
/// @brief Implementation of RemoteAssetSource

#include <void_engine/asset/remote.hpp>

#include <algorithm>
#include <sstream>

namespace void_asset {

// =============================================================================
// AsyncTaskPool Implementation
// =============================================================================

AsyncTaskPool::AsyncTaskPool(std::size_t num_threads) {
    for (std::size_t i = 0; i < num_threads; ++i) {
        m_threads.emplace_back(&AsyncTaskPool::worker_thread, this);
    }
}

AsyncTaskPool::~AsyncTaskPool() {
    {
        std::lock_guard lock(m_mutex);
        m_stop = true;
    }
    m_condition.notify_all();

    for (auto& thread : m_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void AsyncTaskPool::submit(Task task) {
    {
        std::lock_guard lock(m_mutex);
        m_tasks.push_back(std::move(task));
        ++m_pending;
    }
    m_condition.notify_one();
}

std::size_t AsyncTaskPool::pending_count() const {
    return m_pending.load();
}

void AsyncTaskPool::wait_all() {
    std::unique_lock lock(m_mutex);
    m_done_condition.wait(lock, [this] {
        return m_pending == 0;
    });
}

void AsyncTaskPool::worker_thread() {
    while (true) {
        Task task;
        {
            std::unique_lock lock(m_mutex);
            m_condition.wait(lock, [this] {
                return m_stop || !m_tasks.empty();
            });

            if (m_stop && m_tasks.empty()) {
                return;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop_front();
        }

        task();

        --m_pending;
        m_done_condition.notify_all();
    }
}

// =============================================================================
// RemoteAssetSource::Impl
// =============================================================================

class RemoteAssetSource::Impl {
public:
    explicit Impl(RemoteConfig config, std::shared_ptr<TieredCache> cache)
        : m_config(std::move(config))
        , m_cache(std::move(cache))
        , m_task_pool(m_config.max_concurrent_requests)
        , m_state(ConnectionState::Disconnected)
    {
        m_http_client = create_curl_client(m_config);
        m_ws_client = create_beast_client(m_config);
    }

    ~Impl() {
        disconnect();
    }

    bool connect() {
        if (m_state == ConnectionState::Connected) {
            return true;
        }

        m_state = ConnectionState::Connecting;

        // Set up WebSocket callbacks
        if (m_ws_client) {
            m_ws_client->set_message_callback([this](const WebSocketMessage& msg) {
                handle_ws_message(msg);
            });

            m_ws_client->set_close_callback([this](int code, const std::string& reason) {
                handle_ws_close(code, reason);
            });

            m_ws_client->set_error_callback([this](const std::string& error) {
                handle_ws_error(error);
            });

            if (m_ws_client->connect(m_config.websocket_url)) {
                m_state = ConnectionState::Connected;
                m_stats.connected_at = std::chrono::steady_clock::now();
                emit_event(RemoteEvent::connected());

                // Subscribe to project events
                send_subscribe_message();

                return true;
            }
        }

        m_state = ConnectionState::Failed;
        emit_event(RemoteEvent::error("Failed to connect to WebSocket"));
        return false;
    }

    void disconnect() {
        if (m_ws_client) {
            m_ws_client->disconnect();
        }
        m_state = ConnectionState::Disconnected;
        emit_event(RemoteEvent::disconnected());
    }

    bool is_connected() const {
        return m_state == ConnectionState::Connected;
    }

    ConnectionState connection_state() const {
        return m_state;
    }

    ConnectionStats connection_stats() const {
        std::lock_guard lock(m_stats_mutex);
        ConnectionStats stats = m_stats;
        stats.state = m_state;
        return stats;
    }

    std::future<FetchResult> fetch_async(FetchRequest request) {
        return m_task_pool.submit_with_result([this, req = std::move(request)]() mutable {
            return fetch_internal(req);
        });
    }

    FetchResult fetch_sync(const std::string& path, CachePriority priority) {
        FetchRequest req;
        req.path = path;
        req.priority = priority;
        return fetch_internal(req);
    }

    void prefetch(const std::vector<std::string>& paths) {
        for (const auto& path : paths) {
            FetchRequest req;
            req.path = path;
            req.priority = CachePriority::Low;

            m_task_pool.submit([this, req = std::move(req)]() mutable {
                fetch_internal(req);
            });
        }
    }

    bool cancel_fetch(const std::string& path) {
        std::lock_guard lock(m_pending_mutex);
        auto it = m_pending_fetches.find(path);
        if (it != m_pending_fetches.end()) {
            it->second = true; // Mark as cancelled
            return true;
        }
        return false;
    }

    void invalidate(const std::string& path) {
        if (m_cache) {
            m_cache->invalidate(path);
        }
    }

    void invalidate_pattern(const std::string& pattern) {
        // Simple wildcard matching - could be enhanced
        // For now, just invalidate exact match
        invalidate(pattern);
    }

    void set_event_callback(EventCallback callback) {
        std::lock_guard lock(m_event_mutex);
        m_event_callback = std::move(callback);
    }

    std::vector<RemoteEvent> poll_events() {
        std::lock_guard lock(m_event_mutex);
        std::vector<RemoteEvent> events(
            std::make_move_iterator(m_event_queue.begin()),
            std::make_move_iterator(m_event_queue.end())
        );
        m_event_queue.clear();
        return events;
    }

    void poll_ws() {
        if (m_ws_client) {
            m_ws_client->poll();
        }

        // Check for reconnection
        if (m_config.auto_reconnect && m_state == ConnectionState::Disconnected) {
            auto now = std::chrono::steady_clock::now();
            if (now - m_last_reconnect_attempt > m_current_reconnect_delay) {
                m_state = ConnectionState::Reconnecting;
                emit_event({RemoteEventType::Reconnecting, {}, {}, now});
                m_last_reconnect_attempt = now;

                if (connect()) {
                    m_current_reconnect_delay = m_config.reconnect_delay;
                } else {
                    // Exponential backoff
                    m_current_reconnect_delay = std::min(
                        m_current_reconnect_delay * 2,
                        m_config.max_reconnect_delay);
                    m_state = ConnectionState::Disconnected;
                }
            }
        }
    }

    void set_auth_token(const std::string& token) {
        m_config.auth_token = token;
        if (m_http_client) {
            m_http_client->set_auth_token(token);
        }
    }

    const RemoteConfig& config() const { return m_config; }
    std::shared_ptr<TieredCache> cache() const { return m_cache; }
    void set_cache(std::shared_ptr<TieredCache> cache) { m_cache = std::move(cache); }

private:
    FetchResult fetch_internal(FetchRequest& request) {
        FetchResult result;

        // Check if cancelled
        {
            std::lock_guard lock(m_pending_mutex);
            if (m_pending_fetches.count(request.path) && m_pending_fetches[request.path]) {
                m_pending_fetches.erase(request.path);
                result.error = "Fetch cancelled";
                return result;
            }
            m_pending_fetches[request.path] = false;
        }

        // Check cache first (unless force refresh)
        if (m_cache && !request.force_refresh) {
            auto cached = m_cache->get(request.path);
            if (cached) {
                auto validation = m_cache->validate(request.path);
                if (validation == ValidationResult::Valid) {
                    result.success = true;
                    result.entry = cached;
                    result.from_cache = true;
                    cleanup_pending(request.path);
                    return result;
                }

                // Use cached ETag/Last-Modified for conditional request
                if (request.if_none_match.empty()) {
                    request.if_none_match = cached->meta.etag;
                }
                if (request.if_modified_since.empty()) {
                    request.if_modified_since = cached->meta.last_modified;
                }
            }
        }

        // Build URL
        std::string url = m_config.api_base_url + "/projects/" +
                          m_config.project_id + "/assets/" + request.path;

        // Perform HTTP request
        HttpResponse response;
        if (!request.if_none_match.empty() || !request.if_modified_since.empty()) {
            response = m_http_client->get_conditional(
                url, request.if_none_match, request.if_modified_since);
        } else {
            response = m_http_client->get(url, {});
        }

        // Handle response
        if (response.is_not_modified()) {
            // Cache is still valid
            result.success = true;
            result.not_modified = true;
            result.from_cache = true;
            if (m_cache) {
                result.entry = m_cache->get(request.path);
            }
        } else if (response.is_success()) {
            // Create cache entry
            auto entry = std::make_shared<CacheEntry>();
            entry->data = std::move(response.body);
            entry->meta.size_bytes = entry->data.size();
            entry->meta.etag = response.etag();
            entry->meta.last_modified = response.last_modified();
            entry->meta.priority = request.priority;
            entry->meta.cached_at = std::chrono::steady_clock::now();
            entry->meta.last_access = entry->meta.cached_at;
            entry->meta.source_url = url;
            entry->meta.asset_type = get_extension(request.path);

            // Store in cache
            if (m_cache) {
                m_cache->put(request.path, entry);
            }

            result.success = true;
            result.entry = entry;

            // Update stats
            std::lock_guard lock(m_stats_mutex);
            m_stats.bytes_received += entry->data.size();
        } else if (response.is_not_found()) {
            result.error = "Asset not found: " + request.path;
        } else {
            result.error = "HTTP error " + std::to_string(response.status_code) +
                          ": " + response.status_message;
        }

        cleanup_pending(request.path);

        // Call completion callback if provided
        if (request.on_complete) {
            request.on_complete(result.entry, result.error);
        }

        return result;
    }

    void cleanup_pending(const std::string& path) {
        std::lock_guard lock(m_pending_mutex);
        m_pending_fetches.erase(path);
    }

    void handle_ws_message(const WebSocketMessage& msg) {
        std::lock_guard lock(m_stats_mutex);
        ++m_stats.messages_received;
        m_stats.bytes_received += msg.data.size();
        m_stats.last_message_at = std::chrono::steady_clock::now();

        if (msg.type == WebSocketMessage::Type::Text) {
            parse_server_message(msg.as_text());
        } else if (msg.type == WebSocketMessage::Type::Pong) {
            // Update ping RTT
            auto now = std::chrono::steady_clock::now();
            m_stats.last_ping_rtt = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - m_last_ping_time);
        }
    }

    void handle_ws_close(int code, const std::string& reason) {
        m_state = ConnectionState::Disconnected;
        emit_event(RemoteEvent::disconnected(
            "WebSocket closed: " + std::to_string(code) + " " + reason));

        std::lock_guard lock(m_stats_mutex);
        ++m_stats.reconnect_count;
    }

    void handle_ws_error(const std::string& error) {
        emit_event(RemoteEvent::error(error));
    }

    void parse_server_message(const std::string& json) {
        // Simple JSON parsing - in production use a proper JSON library
        // Expected format: {"type": "asset_updated", "path": "textures/foo.png"}

        auto get_json_value = [&json](const std::string& key) -> std::string {
            auto key_pos = json.find("\"" + key + "\"");
            if (key_pos == std::string::npos) return "";

            auto colon_pos = json.find(':', key_pos);
            if (colon_pos == std::string::npos) return "";

            auto start = json.find('"', colon_pos + 1);
            if (start == std::string::npos) return "";

            auto end = json.find('"', start + 1);
            if (end == std::string::npos) return "";

            return json.substr(start + 1, end - start - 1);
        };

        auto type = get_json_value("type");
        auto path = get_json_value("path");

        if (type == "asset_created") {
            emit_event({RemoteEventType::AssetCreated, path, {},
                       std::chrono::steady_clock::now()});
        } else if (type == "asset_updated") {
            // Invalidate cache and emit event for hot-reload
            if (m_cache) {
                m_cache->invalidate(path);
            }
            emit_event(RemoteEvent::asset_updated(path));
        } else if (type == "asset_deleted") {
            if (m_cache) {
                m_cache->remove(path);
            }
            emit_event({RemoteEventType::AssetDeleted, path, {},
                       std::chrono::steady_clock::now()});
        } else if (type == "scene_created") {
            emit_event({RemoteEventType::SceneCreated, path, {},
                       std::chrono::steady_clock::now()});
        } else if (type == "scene_updated") {
            emit_event({RemoteEventType::SceneUpdated, path, {},
                       std::chrono::steady_clock::now()});
        } else if (type == "scene_deleted") {
            emit_event({RemoteEventType::SceneDeleted, path, {},
                       std::chrono::steady_clock::now()});
        } else if (type == "pong") {
            emit_event({RemoteEventType::Ping, {}, {},
                       std::chrono::steady_clock::now()});
        }
    }

    void send_subscribe_message() {
        if (m_ws_client && m_ws_client->is_connected()) {
            // Send subscription request
            std::string msg = R"({"type":"subscribe","project":")" +
                             m_config.project_id + R"("})";
            m_ws_client->send_text(msg);

            std::lock_guard lock(m_stats_mutex);
            ++m_stats.messages_sent;
            m_stats.bytes_sent += msg.size();
        }
    }

    void emit_event(RemoteEvent event) {
        std::lock_guard lock(m_event_mutex);
        if (m_event_callback) {
            m_event_callback(event);
        } else {
            m_event_queue.push_back(std::move(event));
        }
    }

    static std::string get_extension(const std::string& path) {
        auto pos = path.rfind('.');
        if (pos != std::string::npos) {
            return path.substr(pos + 1);
        }
        return "";
    }

    RemoteConfig m_config;
    std::shared_ptr<TieredCache> m_cache;
    std::unique_ptr<IHttpClient> m_http_client;
    std::unique_ptr<IWebSocketClient> m_ws_client;
    AsyncTaskPool m_task_pool;

    std::atomic<ConnectionState> m_state;
    ConnectionStats m_stats;
    mutable std::mutex m_stats_mutex;

    std::mutex m_pending_mutex;
    std::unordered_map<std::string, bool> m_pending_fetches; // path -> cancelled

    std::mutex m_event_mutex;
    EventCallback m_event_callback;
    std::deque<RemoteEvent> m_event_queue;

    std::chrono::steady_clock::time_point m_last_reconnect_attempt;
    std::chrono::steady_clock::time_point m_last_ping_time;
    std::chrono::milliseconds m_current_reconnect_delay{1000};
};

// =============================================================================
// RemoteAssetSource
// =============================================================================

RemoteAssetSource::RemoteAssetSource(RemoteConfig config, std::shared_ptr<TieredCache> cache)
    : m_impl(std::make_unique<Impl>(std::move(config), std::move(cache)))
    , m_config(m_impl->config())
{
}

RemoteAssetSource::~RemoteAssetSource() = default;

bool RemoteAssetSource::connect() {
    return m_impl->connect();
}

void RemoteAssetSource::disconnect() {
    m_impl->disconnect();
}

bool RemoteAssetSource::is_connected() const {
    return m_impl->is_connected();
}

ConnectionState RemoteAssetSource::connection_state() const {
    return m_impl->connection_state();
}

ConnectionStats RemoteAssetSource::connection_stats() const {
    return m_impl->connection_stats();
}

std::future<FetchResult> RemoteAssetSource::fetch_async(
    const std::string& path, CachePriority priority) {
    FetchRequest req;
    req.path = path;
    req.priority = priority;
    return m_impl->fetch_async(std::move(req));
}

std::future<FetchResult> RemoteAssetSource::fetch_async(FetchRequest request) {
    return m_impl->fetch_async(std::move(request));
}

FetchResult RemoteAssetSource::fetch(const std::string& path, CachePriority priority) {
    return m_impl->fetch_sync(path, priority);
}

void RemoteAssetSource::prefetch(const std::vector<std::string>& paths) {
    m_impl->prefetch(paths);
}

bool RemoteAssetSource::cancel_fetch(const std::string& path) {
    return m_impl->cancel_fetch(path);
}

std::future<std::vector<std::string>> RemoteAssetSource::list_assets_async(
    const std::string& /*directory*/) {
    // TODO: Implement asset listing API
    std::promise<std::vector<std::string>> promise;
    promise.set_value({});
    return promise.get_future();
}

std::future<std::vector<std::string>> RemoteAssetSource::list_scenes_async() {
    // TODO: Implement scene listing API
    std::promise<std::vector<std::string>> promise;
    promise.set_value({});
    return promise.get_future();
}

void RemoteAssetSource::invalidate(const std::string& path) {
    m_impl->invalidate(path);
}

void RemoteAssetSource::invalidate_pattern(const std::string& pattern) {
    m_impl->invalidate_pattern(pattern);
}

void RemoteAssetSource::set_event_callback(EventCallback callback) {
    m_impl->set_event_callback(std::move(callback));
}

std::vector<RemoteEvent> RemoteAssetSource::poll_events() {
    return m_impl->poll_events();
}

void RemoteAssetSource::set_auth_token(const std::string& token) {
    m_impl->set_auth_token(token);
}

} // namespace void_asset
