/// @file remote.cpp
/// @brief Remote shell server and client implementation for void_shell

#include "remote.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace void_shell {

namespace {

// Socket initialization (Windows needs WSAStartup)
struct SocketInit {
    SocketInit() {
#ifdef _WIN32
        WSADATA wsa_data;
        WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
    }

    ~SocketInit() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

static SocketInit g_socket_init;

inline void close_socket(int fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
}

inline int get_last_error() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

} // anonymous namespace

// =============================================================================
// RemoteConnection Implementation
// =============================================================================

RemoteConnection::RemoteConnection(ConnectionId id, int socket_fd,
                                    const std::string& address, std::uint16_t port)
    : socket_fd_(socket_fd) {
    info_.id = id;
    info_.remote_address = address;
    info_.remote_port = port;
    info_.connected_at = std::chrono::system_clock::now();
}

RemoteConnection::~RemoteConnection() {
    close();
}

bool RemoteConnection::send(MessageType type, const std::string& data) {
    if (!connected_.load()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(send_mutex_);

    MessageHeader header;
    header.type = type;
    header.length = static_cast<std::uint32_t>(data.size());
    header.sequence = next_sequence_++;

    // Send header
    int sent = ::send(socket_fd_, reinterpret_cast<const char*>(&header),
                      sizeof(header), 0);
    if (sent != sizeof(header)) {
        connected_.store(false);
        return false;
    }

    // Send data
    if (!data.empty()) {
        std::size_t total_sent = 0;
        while (total_sent < data.size()) {
            sent = ::send(socket_fd_, data.data() + total_sent,
                         static_cast<int>(data.size() - total_sent), 0);
            if (sent <= 0) {
                connected_.store(false);
                return false;
            }
            total_sent += sent;
        }
    }

    info_.bytes_sent += sizeof(header) + data.size();
    return true;
}

bool RemoteConnection::receive(MessageType& type, std::string& data) {
    if (!connected_.load()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(recv_mutex_);

    MessageHeader header;

    // Receive header
    int received = recv(socket_fd_, reinterpret_cast<char*>(&header),
                        sizeof(header), 0);
    if (received != sizeof(header)) {
        connected_.store(false);
        return false;
    }

    // Verify magic
    if (header.magic[0] != 'V' || header.magic[1] != 'S' ||
        header.magic[2] != 'H' || header.magic[3] != 'L') {
        connected_.store(false);
        return false;
    }

    type = header.type;

    // Receive data
    if (header.length > 0) {
        data.resize(header.length);
        std::size_t total_received = 0;

        while (total_received < header.length) {
            received = recv(socket_fd_, data.data() + total_received,
                           static_cast<int>(header.length - total_received), 0);
            if (received <= 0) {
                connected_.store(false);
                return false;
            }
            total_received += received;
        }
    } else {
        data.clear();
    }

    info_.bytes_received += sizeof(header) + data.size();
    return true;
}

void RemoteConnection::close() {
    if (connected_.exchange(false)) {
        close_socket(socket_fd_);
        socket_fd_ = -1;
    }
}

// =============================================================================
// RemoteServer Implementation
// =============================================================================

RemoteServer::RemoteServer() = default;

RemoteServer::~RemoteServer() {
    stop();
}

bool RemoteServer::start(std::uint16_t port) {
    if (running_.load()) {
        return true;
    }

    // Create socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        return false;
    }

    // Set socket options
    int opt = 1;
#ifdef _WIN32
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    // Bind
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close_socket(server_socket_);
        server_socket_ = -1;
        return false;
    }

    // Listen
    if (listen(server_socket_, 5) < 0) {
        close_socket(server_socket_);
        server_socket_ = -1;
        return false;
    }

    port_ = port;
    running_.store(true);
    stats_.started_at = std::chrono::system_clock::now();

    // Start accept thread
    accept_thread_ = std::thread(&RemoteServer::accept_loop, this);

    return true;
}

void RemoteServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    // Close server socket to unblock accept
    if (server_socket_ >= 0) {
        close_socket(server_socket_);
        server_socket_ = -1;
    }

    // Wait for accept thread
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    // Disconnect all clients
    disconnect_all();

    // Wait for client threads
    for (auto& thread : client_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    client_threads_.clear();
}

std::vector<const ConnectionInfo*> RemoteServer::connections() const {
    std::lock_guard<std::mutex> lock(connections_mutex_);

    std::vector<const ConnectionInfo*> result;
    for (const auto& [id, conn] : connections_) {
        result.push_back(&conn->info());
    }
    return result;
}

std::size_t RemoteServer::connection_count() const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    return connections_.size();
}

bool RemoteServer::disconnect(ConnectionId id) {
    std::lock_guard<std::mutex> lock(connections_mutex_);

    auto it = connections_.find(id);
    if (it == connections_.end()) {
        return false;
    }

    it->second->close();
    return true;
}

void RemoteServer::disconnect_all() {
    std::lock_guard<std::mutex> lock(connections_mutex_);

    for (auto& [id, conn] : connections_) {
        conn->close();
    }
}

RemoteServer::Stats RemoteServer::stats() const {
    std::lock_guard<std::mutex> lock(connections_mutex_);

    Stats s = stats_;
    s.active_connections = connections_.size();

    for (const auto& [id, conn] : connections_) {
        s.bytes_sent += conn->info().bytes_sent;
        s.bytes_received += conn->info().bytes_received;
        s.total_commands += conn->info().commands_executed;
    }

    return s;
}

void RemoteServer::accept_loop() {
    while (running_.load()) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        int client_socket = accept(server_socket_,
                                    reinterpret_cast<sockaddr*>(&client_addr),
                                    &addr_len);

        if (client_socket < 0) {
            if (!running_.load()) {
                break;
            }
            continue;
        }

        // Get client address
        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, sizeof(addr_str));
        std::string address(addr_str);
        std::uint16_t port = ntohs(client_addr.sin_port);

        // Check IP whitelist
        if (!is_ip_allowed(address)) {
            close_socket(client_socket);
            continue;
        }

        // Create connection
        ConnectionId conn_id;
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);

            conn_id = ConnectionId{next_connection_id_++};
            auto conn = std::make_unique<RemoteConnection>(
                conn_id, client_socket, address, port);
            connections_[conn_id] = std::move(conn);

            stats_.total_connections++;
        }

        // Notify connection
        if (on_connect_) {
            on_connect_(conn_id);
        }

        // Start client thread
        client_threads_.emplace_back(&RemoteServer::client_loop, this, conn_id);
    }
}

void RemoteServer::client_loop(ConnectionId conn_id) {
    RemoteConnection* conn = nullptr;

    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(conn_id);
        if (it == connections_.end()) {
            return;
        }
        conn = it->second.get();
    }

    // Send initial prompt
    conn->send(MessageType::Prompt, "void_shell> ");

    while (running_.load() && conn->is_connected()) {
        MessageType type;
        std::string data;

        if (!conn->receive(type, data)) {
            break;
        }

        handle_message(*conn, type, data);

        if (type == MessageType::Disconnect) {
            break;
        }
    }

    // Notify disconnection
    if (on_disconnect_) {
        on_disconnect_(conn_id);
    }

    // Remove connection
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_.erase(conn_id);
    }
}

void RemoteServer::handle_message(RemoteConnection& conn, MessageType type,
                                   const std::string& data) {
    switch (type) {
        case MessageType::Command: {
            if (auth_required_ && !conn.info().authenticated) {
                conn.send(MessageType::Error, "Not authenticated");
                break;
            }

            if (on_command_) {
                auto result = on_command_(conn.id(), data);

                if (!result.output.empty()) {
                    conn.send(MessageType::Output, result.output);
                }
                if (!result.error_message.empty()) {
                    conn.send(MessageType::Error, result.error_message);
                }

                // Send result
                std::string result_str = std::to_string(result.exit_code);
                conn.send(MessageType::Result, result_str);
            }

            // Send new prompt
            conn.send(MessageType::Prompt, "void_shell> ");
            break;
        }

        case MessageType::TabComplete: {
            // TODO: Handle tab completion
            conn.send(MessageType::Completion, "");
            break;
        }

        case MessageType::Cancel: {
            // TODO: Cancel current command
            break;
        }

        case MessageType::Ping: {
            conn.send(MessageType::Pong, data);
            break;
        }

        case MessageType::Authenticate: {
            if (!auth_required_) {
                conn.set_authenticated(true);
                conn.send(MessageType::AuthResult, "1");
                break;
            }

            // Parse username:password
            std::size_t sep = data.find(':');
            if (sep == std::string::npos) {
                conn.send(MessageType::AuthResult, "0");
                break;
            }

            std::string username = data.substr(0, sep);
            std::string password = data.substr(sep + 1);

            bool authenticated = false;
            if (auth_callback_) {
                authenticated = auth_callback_(username, password);
            }

            conn.set_authenticated(authenticated);
            conn.send(MessageType::AuthResult, authenticated ? "1" : "0");
            break;
        }

        case MessageType::Disconnect: {
            conn.send(MessageType::Disconnect, "");
            conn.close();
            break;
        }

        default:
            break;
    }
}

bool RemoteServer::is_ip_allowed(const std::string& ip) const {
    if (allowed_ips_.empty()) {
        return true;
    }

    return std::find(allowed_ips_.begin(), allowed_ips_.end(), ip) != allowed_ips_.end();
}

// =============================================================================
// RemoteClient Implementation
// =============================================================================

RemoteClient::RemoteClient() = default;

RemoteClient::~RemoteClient() {
    disconnect();
}

bool RemoteClient::connect(const std::string& host, std::uint16_t port) {
    if (connected_.load()) {
        return true;
    }

    // Resolve hostname
    struct addrinfo hints{}, *result;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result) != 0) {
        return false;
    }

    // Create socket
    int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(result);
        return false;
    }

    // Connect
    if (::connect(sock, result->ai_addr, static_cast<int>(result->ai_addrlen)) < 0) {
        close_socket(sock);
        freeaddrinfo(result);
        return false;
    }

    freeaddrinfo(result);

    // Create connection object
    connection_ = std::make_unique<RemoteConnection>(ConnectionId{1}, sock, host, port);
    connected_.store(true);

    // Start receive thread
    receive_thread_ = std::thread(&RemoteClient::receive_loop, this);

    return true;
}

void RemoteClient::disconnect() {
    if (!connected_.exchange(false)) {
        return;
    }

    // Send disconnect message
    if (connection_) {
        connection_->send(MessageType::Disconnect, "");
        connection_->close();
    }

    // Wait for receive thread
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }

    connection_.reset();
}

bool RemoteClient::is_connected() const {
    return connected_.load() && connection_ && connection_->is_connected();
}

bool RemoteClient::authenticate(const std::string& username, const std::string& password) {
    if (!is_connected()) {
        return false;
    }

    std::string auth_data = username + ":" + password;
    connection_->send(MessageType::Authenticate, auth_data);

    // Wait for response
    std::unique_lock<std::mutex> lock(response_mutex_);
    pending_result_.reset();

    // The receive thread will set pending_result_
    if (response_cv_.wait_for(lock, std::chrono::seconds(5),
                               [this] { return pending_result_.has_value(); })) {
        return pending_result_->exit_code == 1;
    }

    return false;
}

CommandResult RemoteClient::execute(const std::string& command) {
    return execute(command, std::chrono::milliseconds(30000));
}

CommandResult RemoteClient::execute(const std::string& command,
                                     std::chrono::milliseconds timeout) {
    if (!is_connected()) {
        return CommandResult::error("Not connected");
    }

    // Send command
    connection_->send(MessageType::Command, command);

    // Wait for result
    std::unique_lock<std::mutex> lock(response_mutex_);
    pending_result_.reset();

    if (response_cv_.wait_for(lock, timeout,
                               [this] { return pending_result_.has_value(); })) {
        return *pending_result_;
    }

    return CommandResult::error("Command timeout");
}

void RemoteClient::cancel() {
    if (is_connected()) {
        connection_->send(MessageType::Cancel, "");
    }
}

std::vector<std::string> RemoteClient::complete(const std::string& input,
                                                  std::size_t cursor_pos) {
    if (!is_connected()) {
        return {};
    }

    std::string data = input + "\n" + std::to_string(cursor_pos);
    connection_->send(MessageType::TabComplete, data);

    // Wait for response
    std::unique_lock<std::mutex> lock(response_mutex_);
    pending_completions_.clear();

    if (response_cv_.wait_for(lock, std::chrono::seconds(2),
                               [this] { return !pending_completions_.empty(); })) {
        return pending_completions_;
    }

    return {};
}

void RemoteClient::run_interactive() {
    if (!is_connected()) {
        return;
    }

    interactive_running_.store(true);

    while (interactive_running_.load() && is_connected()) {
        std::string line;
        if (!std::getline(std::cin, line)) {
            break;
        }

        if (line == "exit" || line == "quit") {
            break;
        }

        auto result = execute(line);

        if (!result.output.empty() && output_callback_) {
            output_callback_(result.output);
        }
        if (!result.error_message.empty() && error_callback_) {
            error_callback_(result.error_message);
        }
    }

    interactive_running_.store(false);
}

void RemoteClient::stop_interactive() {
    interactive_running_.store(false);
}

void RemoteClient::receive_loop() {
    while (connected_.load() && connection_ && connection_->is_connected()) {
        MessageType type;
        std::string data;

        if (!connection_->receive(type, data)) {
            break;
        }

        handle_message(type, data);
    }

    connected_.store(false);
}

void RemoteClient::handle_message(MessageType type, const std::string& data) {
    switch (type) {
        case MessageType::Output: {
            if (output_callback_) {
                output_callback_(data);
            }
            break;
        }

        case MessageType::Error: {
            if (error_callback_) {
                error_callback_(data);
            }
            break;
        }

        case MessageType::Prompt: {
            if (prompt_callback_) {
                prompt_callback_(data);
            }
            break;
        }

        case MessageType::Completion: {
            std::lock_guard<std::mutex> lock(response_mutex_);

            // Parse completions (newline-separated)
            pending_completions_.clear();
            std::istringstream ss(data);
            std::string completion;
            while (std::getline(ss, completion)) {
                if (!completion.empty()) {
                    pending_completions_.push_back(completion);
                }
            }

            response_cv_.notify_all();
            break;
        }

        case MessageType::Result: {
            std::lock_guard<std::mutex> lock(response_mutex_);

            CommandResult result;
            try {
                result.exit_code = std::stoi(data);
                result.status = result.exit_code == 0
                    ? CommandStatus::Success
                    : CommandStatus::Error;
            } catch (...) {
                result.exit_code = 1;
                result.status = CommandStatus::Error;
            }

            pending_result_ = result;
            response_cv_.notify_all();
            break;
        }

        case MessageType::Pong: {
            // Ping response - could track latency
            break;
        }

        case MessageType::AuthResult: {
            std::lock_guard<std::mutex> lock(response_mutex_);

            CommandResult result;
            result.exit_code = data == "1" ? 1 : 0;
            result.status = result.exit_code == 1
                ? CommandStatus::Success
                : CommandStatus::Error;

            pending_result_ = result;
            response_cv_.notify_all();
            break;
        }

        case MessageType::Disconnect: {
            connected_.store(false);
            break;
        }

        default:
            break;
    }
}

} // namespace void_shell
