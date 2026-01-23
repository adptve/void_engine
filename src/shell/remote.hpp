#pragma once

/// @file remote.hpp
/// @brief Remote shell server and client

#include "types.hpp"
#include "session.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace void_shell {

// =============================================================================
// Remote Protocol
// =============================================================================

/// @brief Remote shell message type
enum class MessageType : std::uint8_t {
    // Client -> Server
    Command = 0x01,
    TabComplete = 0x02,
    Cancel = 0x03,
    Ping = 0x04,
    Authenticate = 0x05,

    // Server -> Client
    Output = 0x10,
    Error = 0x11,
    Prompt = 0x12,
    Completion = 0x13,
    Result = 0x14,
    Pong = 0x15,
    AuthResult = 0x16,

    // Bidirectional
    Disconnect = 0xFF
};

/// @brief Remote message header
struct MessageHeader {
    std::uint8_t magic[4] = {'V', 'S', 'H', 'L'};
    MessageType type;
    std::uint32_t length;
    std::uint32_t sequence;
};

/// @brief Remote connection information
struct ConnectionInfo {
    ConnectionId id;
    std::string remote_address;
    std::uint16_t remote_port;
    std::chrono::system_clock::time_point connected_at;
    SessionId session_id;
    bool authenticated = false;
    std::size_t bytes_sent = 0;
    std::size_t bytes_received = 0;
    std::size_t commands_executed = 0;
};

// =============================================================================
// Remote Connection
// =============================================================================

/// @brief Individual remote connection
class RemoteConnection {
public:
    RemoteConnection(ConnectionId id, int socket_fd, const std::string& address, std::uint16_t port);
    ~RemoteConnection();

    // Non-copyable
    RemoteConnection(const RemoteConnection&) = delete;
    RemoteConnection& operator=(const RemoteConnection&) = delete;

    /// @brief Get connection ID
    ConnectionId id() const { return info_.id; }

    /// @brief Get connection info
    const ConnectionInfo& info() const { return info_; }

    /// @brief Check if connected
    bool is_connected() const { return connected_.load(); }

    /// @brief Send a message
    bool send(MessageType type, const std::string& data);

    /// @brief Receive a message (blocking)
    bool receive(MessageType& type, std::string& data);

    /// @brief Close connection
    void close();

    /// @brief Set associated session
    void set_session(SessionId id) { info_.session_id = id; }

    /// @brief Mark as authenticated
    void set_authenticated(bool auth) { info_.authenticated = auth; }

    /// @brief Get socket file descriptor
    int socket_fd() const { return socket_fd_; }

private:
    ConnectionInfo info_;
    int socket_fd_;
    std::atomic<bool> connected_{true};
    std::uint32_t next_sequence_ = 0;
    std::mutex send_mutex_;
    std::mutex recv_mutex_;
};

// =============================================================================
// Remote Server
// =============================================================================

/// @brief Remote shell server
class RemoteServer {
public:
    RemoteServer();
    ~RemoteServer();

    // Non-copyable
    RemoteServer(const RemoteServer&) = delete;
    RemoteServer& operator=(const RemoteServer&) = delete;

    // ==========================================================================
    // Server Control
    // ==========================================================================

    /// @brief Start the server
    bool start(std::uint16_t port);

    /// @brief Stop the server
    void stop();

    /// @brief Check if running
    bool is_running() const { return running_.load(); }

    /// @brief Get port
    std::uint16_t port() const { return port_; }

    // ==========================================================================
    // Connections
    // ==========================================================================

    /// @brief Get all connections
    std::vector<const ConnectionInfo*> connections() const;

    /// @brief Get connection count
    std::size_t connection_count() const;

    /// @brief Disconnect a client
    bool disconnect(ConnectionId id);

    /// @brief Disconnect all clients
    void disconnect_all();

    // ==========================================================================
    // Authentication
    // ==========================================================================

    /// @brief Set authentication required
    void set_auth_required(bool required) { auth_required_ = required; }

    /// @brief Set authentication callback
    using AuthCallback = std::function<bool(const std::string& username,
                                             const std::string& password)>;
    void set_auth_callback(AuthCallback callback) { auth_callback_ = std::move(callback); }

    /// @brief Set allowed IPs (empty = allow all)
    void set_allowed_ips(const std::vector<std::string>& ips) { allowed_ips_ = ips; }

    // ==========================================================================
    // Callbacks
    // ==========================================================================

    using ConnectCallback = std::function<void(ConnectionId)>;
    using DisconnectCallback = std::function<void(ConnectionId)>;
    using CommandCallback = std::function<CommandResult(ConnectionId, const std::string&)>;

    void set_connect_callback(ConnectCallback cb) { on_connect_ = std::move(cb); }
    void set_disconnect_callback(DisconnectCallback cb) { on_disconnect_ = std::move(cb); }
    void set_command_callback(CommandCallback cb) { on_command_ = std::move(cb); }

    // ==========================================================================
    // Statistics
    // ==========================================================================

    struct Stats {
        std::size_t total_connections = 0;
        std::size_t active_connections = 0;
        std::size_t total_commands = 0;
        std::size_t bytes_sent = 0;
        std::size_t bytes_received = 0;
        std::chrono::system_clock::time_point started_at;
    };

    Stats stats() const;

private:
    std::atomic<bool> running_{false};
    std::uint16_t port_ = 0;
    int server_socket_ = -1;

    std::unordered_map<ConnectionId, std::unique_ptr<RemoteConnection>> connections_;
    std::uint32_t next_connection_id_ = 1;
    mutable std::mutex connections_mutex_;

    std::thread accept_thread_;
    std::vector<std::thread> client_threads_;

    bool auth_required_ = false;
    AuthCallback auth_callback_;
    std::vector<std::string> allowed_ips_;

    ConnectCallback on_connect_;
    DisconnectCallback on_disconnect_;
    CommandCallback on_command_;

    Stats stats_;

    void accept_loop();
    void client_loop(ConnectionId conn_id);
    void handle_message(RemoteConnection& conn, MessageType type, const std::string& data);
    bool is_ip_allowed(const std::string& ip) const;
};

// =============================================================================
// Remote Client
// =============================================================================

/// @brief Remote shell client
class RemoteClient {
public:
    RemoteClient();
    ~RemoteClient();

    // Non-copyable
    RemoteClient(const RemoteClient&) = delete;
    RemoteClient& operator=(const RemoteClient&) = delete;

    // ==========================================================================
    // Connection
    // ==========================================================================

    /// @brief Connect to server
    bool connect(const std::string& host, std::uint16_t port);

    /// @brief Disconnect
    void disconnect();

    /// @brief Check if connected
    bool is_connected() const;

    /// @brief Authenticate
    bool authenticate(const std::string& username, const std::string& password);

    // ==========================================================================
    // Commands
    // ==========================================================================

    /// @brief Execute command
    CommandResult execute(const std::string& command);

    /// @brief Execute command with timeout
    CommandResult execute(const std::string& command, std::chrono::milliseconds timeout);

    /// @brief Cancel current command
    void cancel();

    /// @brief Get completions
    std::vector<std::string> complete(const std::string& input, std::size_t cursor_pos);

    // ==========================================================================
    // Callbacks
    // ==========================================================================

    void set_output_callback(OutputCallback cb) { output_callback_ = std::move(cb); }
    void set_error_callback(ErrorCallback cb) { error_callback_ = std::move(cb); }
    void set_prompt_callback(std::function<void(const std::string&)> cb) {
        prompt_callback_ = std::move(cb);
    }

    // ==========================================================================
    // Interactive Mode
    // ==========================================================================

    /// @brief Run interactive session
    void run_interactive();

    /// @brief Stop interactive session
    void stop_interactive();

private:
    std::unique_ptr<RemoteConnection> connection_;
    std::atomic<bool> connected_{false};
    std::thread receive_thread_;
    std::atomic<bool> interactive_running_{false};

    OutputCallback output_callback_;
    ErrorCallback error_callback_;
    std::function<void(const std::string&)> prompt_callback_;

    // Pending response tracking
    std::mutex response_mutex_;
    std::condition_variable response_cv_;
    std::optional<CommandResult> pending_result_;
    std::vector<std::string> pending_completions_;

    void receive_loop();
    void handle_message(MessageType type, const std::string& data);
};

} // namespace void_shell
