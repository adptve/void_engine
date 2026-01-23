/// @file websocket_client.cpp
/// @brief WebSocket client implementation using Boost.Beast

#include <void_engine/asset/remote.hpp>

#ifdef VOID_HAS_BEAST
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;
#endif

#include <algorithm>
#include <queue>
#include <regex>

namespace void_asset {

#ifdef VOID_HAS_BEAST

// =============================================================================
// Beast WebSocket Client Implementation
// =============================================================================

class BeastWebSocketClient : public IWebSocketClient {
public:
    explicit BeastWebSocketClient(const RemoteConfig& config)
        : m_config(config)
        , m_ioc()
        , m_ssl_ctx(ssl::context::tlsv12_client)
        , m_resolver(net::make_strand(m_ioc))
        , m_connected(false)
    {
        // Configure SSL context
        m_ssl_ctx.set_default_verify_paths();
        if (config.verify_ssl) {
            m_ssl_ctx.set_verify_mode(ssl::verify_peer);
        } else {
            m_ssl_ctx.set_verify_mode(ssl::verify_none);
        }
    }

    ~BeastWebSocketClient() override {
        disconnect();
    }

    bool connect(const std::string& url) override {
        if (m_connected) {
            return true;
        }

        try {
            // Parse URL
            std::string host, port, path;
            bool use_ssl = false;
            if (!parse_ws_url(url, host, port, path, use_ssl)) {
                m_last_error = "Invalid WebSocket URL";
                return false;
            }

            // Resolve hostname
            auto const results = m_resolver.resolve(host, port);

            if (use_ssl) {
                // Create SSL WebSocket
                m_wss = std::make_unique<
                    websocket::stream<beast::ssl_stream<tcp::socket>>>(
                        net::make_strand(m_ioc), m_ssl_ctx);

                // Connect TCP
                auto ep = net::connect(get_lowest_layer(*m_wss), results);

                // Set SNI
                if (!SSL_set_tlsext_host_name(
                        m_wss->next_layer().native_handle(), host.c_str())) {
                    m_last_error = "Failed to set SNI";
                    return false;
                }

                // SSL handshake
                m_wss->next_layer().handshake(ssl::stream_base::client);

                // Set WebSocket options
                m_wss->set_option(websocket::stream_base::decorator(
                    [this, &host](websocket::request_type& req) {
                        req.set(http::field::host, host);
                        req.set(http::field::user_agent, m_config.user_agent);
                        if (!m_config.auth_token.empty()) {
                            req.set(http::field::authorization,
                                   "Bearer " + m_config.auth_token);
                        }
                    }));

                // WebSocket handshake
                m_wss->handshake(host + ":" + port, path);

                // Start async read
                start_async_read_ssl();
            } else {
                // Create plain WebSocket
                m_ws = std::make_unique<websocket::stream<tcp::socket>>(
                    net::make_strand(m_ioc));

                // Connect TCP
                auto ep = net::connect(m_ws->next_layer(), results);

                // Set WebSocket options
                m_ws->set_option(websocket::stream_base::decorator(
                    [this, &host](websocket::request_type& req) {
                        req.set(http::field::host, host);
                        req.set(http::field::user_agent, m_config.user_agent);
                        if (!m_config.auth_token.empty()) {
                            req.set(http::field::authorization,
                                   "Bearer " + m_config.auth_token);
                        }
                    }));

                // WebSocket handshake
                m_ws->handshake(host + ":" + port, path);

                // Start async read
                start_async_read();
            }

            m_use_ssl = use_ssl;
            m_connected = true;
            return true;

        } catch (const std::exception& e) {
            m_last_error = e.what();
            return false;
        }
    }

    void disconnect() override {
        if (!m_connected) return;

        try {
            if (m_use_ssl && m_wss) {
                m_wss->close(websocket::close_code::normal);
            } else if (m_ws) {
                m_ws->close(websocket::close_code::normal);
            }
        } catch (...) {
            // Ignore errors during disconnect
        }

        m_connected = false;
        m_ws.reset();
        m_wss.reset();
    }

    bool is_connected() const override {
        return m_connected;
    }

    void send_text(const std::string& message) override {
        if (!m_connected) return;

        try {
            if (m_use_ssl && m_wss) {
                m_wss->text(true);
                m_wss->write(net::buffer(message));
            } else if (m_ws) {
                m_ws->text(true);
                m_ws->write(net::buffer(message));
            }
        } catch (const std::exception& e) {
            handle_error(e.what());
        }
    }

    void send_binary(const std::vector<std::uint8_t>& data) override {
        if (!m_connected) return;

        try {
            if (m_use_ssl && m_wss) {
                m_wss->binary(true);
                m_wss->write(net::buffer(data.data(), data.size()));
            } else if (m_ws) {
                m_ws->binary(true);
                m_ws->write(net::buffer(data.data(), data.size()));
            }
        } catch (const std::exception& e) {
            handle_error(e.what());
        }
    }

    void set_message_callback(
        std::function<void(const WebSocketMessage&)> callback) override {
        std::lock_guard lock(m_mutex);
        m_message_callback = std::move(callback);
    }

    void set_close_callback(
        std::function<void(int code, const std::string& reason)> callback) override {
        std::lock_guard lock(m_mutex);
        m_close_callback = std::move(callback);
    }

    void set_error_callback(
        std::function<void(const std::string& error)> callback) override {
        std::lock_guard lock(m_mutex);
        m_error_callback = std::move(callback);
    }

    void poll() override {
        // Process pending IO operations
        m_ioc.poll();
        m_ioc.restart();

        // Dispatch queued callbacks
        std::vector<std::function<void()>> callbacks;
        {
            std::lock_guard lock(m_mutex);
            callbacks.swap(m_pending_callbacks);
        }
        for (auto& cb : callbacks) {
            cb();
        }
    }

private:
    bool parse_ws_url(const std::string& url,
                      std::string& host, std::string& port,
                      std::string& path, bool& use_ssl) {
        // Parse ws:// or wss:// URL
        std::regex url_regex(R"(^(wss?)://([^:/]+)(?::(\d+))?(/.*)?$)");
        std::smatch match;

        if (!std::regex_match(url, match, url_regex)) {
            return false;
        }

        std::string scheme = match[1].str();
        use_ssl = (scheme == "wss");

        host = match[2].str();

        if (match[3].matched) {
            port = match[3].str();
        } else {
            port = use_ssl ? "443" : "80";
        }

        if (match[4].matched) {
            path = match[4].str();
        } else {
            path = "/";
        }

        return true;
    }

    void start_async_read() {
        if (!m_ws) return;

        m_ws->async_read(
            m_buffer,
            [this](beast::error_code ec, std::size_t bytes_transferred) {
                on_read(ec, bytes_transferred);
            });
    }

    void start_async_read_ssl() {
        if (!m_wss) return;

        m_wss->async_read(
            m_buffer,
            [this](beast::error_code ec, std::size_t bytes_transferred) {
                on_read_ssl(ec, bytes_transferred);
            });
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        if (ec == websocket::error::closed) {
            handle_close(1000, "Connection closed");
            return;
        }

        if (ec) {
            handle_error(ec.message());
            return;
        }

        handle_message(bytes_transferred, m_ws && m_ws->got_text());
        m_buffer.consume(bytes_transferred);
        start_async_read();
    }

    void on_read_ssl(beast::error_code ec, std::size_t bytes_transferred) {
        if (ec == websocket::error::closed) {
            handle_close(1000, "Connection closed");
            return;
        }

        if (ec) {
            handle_error(ec.message());
            return;
        }

        handle_message(bytes_transferred, m_wss && m_wss->got_text());
        m_buffer.consume(bytes_transferred);
        start_async_read_ssl();
    }

    void handle_message(std::size_t size, bool is_text) {
        WebSocketMessage msg;
        msg.type = is_text ? WebSocketMessage::Type::Text
                          : WebSocketMessage::Type::Binary;

        auto data = static_cast<const std::uint8_t*>(m_buffer.data().data());
        msg.data.assign(data, data + size);

        std::lock_guard lock(m_mutex);
        if (m_message_callback) {
            m_pending_callbacks.push_back([this, msg = std::move(msg)]() {
                if (m_message_callback) {
                    m_message_callback(msg);
                }
            });
        }
    }

    void handle_close(int code, const std::string& reason) {
        m_connected = false;

        std::lock_guard lock(m_mutex);
        if (m_close_callback) {
            m_pending_callbacks.push_back([this, code, reason]() {
                if (m_close_callback) {
                    m_close_callback(code, reason);
                }
            });
        }
    }

    void handle_error(const std::string& error) {
        m_last_error = error;

        std::lock_guard lock(m_mutex);
        if (m_error_callback) {
            m_pending_callbacks.push_back([this, error]() {
                if (m_error_callback) {
                    m_error_callback(error);
                }
            });
        }
    }

    RemoteConfig m_config;
    net::io_context m_ioc;
    ssl::context m_ssl_ctx;
    tcp::resolver m_resolver;

    std::unique_ptr<websocket::stream<tcp::socket>> m_ws;
    std::unique_ptr<websocket::stream<beast::ssl_stream<tcp::socket>>> m_wss;

    beast::flat_buffer m_buffer;
    bool m_use_ssl = false;
    std::atomic<bool> m_connected;
    std::string m_last_error;

    mutable std::mutex m_mutex;
    std::function<void(const WebSocketMessage&)> m_message_callback;
    std::function<void(int, const std::string&)> m_close_callback;
    std::function<void(const std::string&)> m_error_callback;
    std::vector<std::function<void()>> m_pending_callbacks;
};

std::unique_ptr<IWebSocketClient> create_beast_client(const RemoteConfig& config) {
    return std::make_unique<BeastWebSocketClient>(config);
}

#else // !VOID_HAS_BEAST

// =============================================================================
// Stub WebSocket Client (no Beast)
// =============================================================================

class StubWebSocketClient : public IWebSocketClient {
public:
    explicit StubWebSocketClient(const RemoteConfig& /*config*/) {}

    bool connect(const std::string& /*url*/) override {
        return false;
    }

    void disconnect() override {}

    bool is_connected() const override {
        return false;
    }

    void send_text(const std::string& /*message*/) override {}
    void send_binary(const std::vector<std::uint8_t>& /*data*/) override {}

    void set_message_callback(
        std::function<void(const WebSocketMessage&)> /*callback*/) override {}
    void set_close_callback(
        std::function<void(int, const std::string&)> /*callback*/) override {}
    void set_error_callback(
        std::function<void(const std::string&)> /*callback*/) override {}

    void poll() override {}
};

std::unique_ptr<IWebSocketClient> create_beast_client(const RemoteConfig& config) {
    return std::make_unique<StubWebSocketClient>(config);
}

#endif // VOID_HAS_BEAST

} // namespace void_asset
