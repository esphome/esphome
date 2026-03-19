#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include <esp_http_server.h>

#include <functional>
#include <memory>

namespace esphome {
namespace sendspin {

// Forward declarations
class SendspinHub;
class SendspinServerConnection;

/// @brief WebSocket server listener for Sendspin.
///
/// This class manages the HTTP server (httpd) that listens for incoming WebSocket
/// connections from Sendspin servers. It does not own the connections long-term;
/// instead, it creates SendspinServerConnection instances and hands them to the hub,
/// which decides whether to accept or reject them based on handoff logic.
///
/// The server supports accepting multiple connections temporarily (max 2) to enable
/// the handoff protocol where a second server can connect while one is already active.
///
/// Lifecycle:
/// 1. start() is called to begin listening for connections
/// 2. When a client connects, open_callback creates a SendspinServerConnection
/// 3. The new connection is passed to the hub via callback
/// 4. The hub completes the handshake and decides whether to keep the connection
/// 5. stop() is called to shut down the server
class SendspinWsServer {
 public:
  SendspinWsServer() = default;
  ~SendspinWsServer();

  /// @brief Callback type for notifying the hub of new connections.
  /// The hub receives ownership of the connection and must decide whether to keep it.
  using NewConnectionCallback = std::function<void(std::unique_ptr<SendspinServerConnection>)>;

  /// @brief Callback type for notifying the hub when a socket closes.
  /// The hub needs to identify which connection owns this sockfd and clean it up.
  using ConnectionClosedCallback = std::function<void(int sockfd)>;

  /// @brief Callback type for looking up a connection by sockfd.
  using FindConnectionCallback = std::function<SendspinServerConnection *(int sockfd)>;

  /// @brief Starts the HTTP server and begins listening for WebSocket connections.
  /// @param hub Pointer to the hub (used for context in callbacks).
  /// @param task_stack_in_psram Whether to allocate the HTTP server task stack in PSRAM.
  /// @param task_priority Priority for the HTTP server task.
  /// @return true if server started successfully, false otherwise.
  bool start(SendspinHub *hub, bool task_stack_in_psram, unsigned task_priority);

  /// @brief Stops the HTTP server.
  void stop();

  /// @brief Checks if the server is currently running.
  /// @return true if the server is started, false otherwise.
  bool is_started() const { return this->server_ != nullptr; }

  /// @brief Configures the maximum number of simultaneous connections.
  /// Default is 2 to support the handoff protocol (one active + one pending).
  /// @param max_connections Maximum number of open sockets (1-7).
  void set_max_connections(uint8_t max_connections) { this->max_connections_ = max_connections; }

  /// @brief Sets the callback to invoke when a new connection is accepted.
  /// @param callback The callback function.
  void set_new_connection_callback(NewConnectionCallback &&callback) {
    this->new_connection_callback_ = std::move(callback);
  }

  /// @brief Sets the callback to invoke when a socket closes.
  /// @param callback The callback function.
  void set_connection_closed_callback(ConnectionClosedCallback &&callback) {
    this->connection_closed_callback_ = std::move(callback);
  }

  /// @brief Sets callback to find a connection by socket fd.
  void set_find_connection_callback(FindConnectionCallback &&callback) {
    this->find_connection_callback_ = std::move(callback);
  }

  /// @brief Gets the httpd handle (for use by connections).
  /// @return The httpd server handle, or nullptr if not started.
  httpd_handle_t get_server() const { return this->server_; }

 protected:
  /// @brief Callback invoked when a new client opens a connection.
  /// Creates a SendspinServerConnection and notifies the hub.
  /// @param handle The httpd server handle.
  /// @param sockfd The socket file descriptor for the new connection.
  /// @return ESP_OK on success.
  static esp_err_t open_callback(httpd_handle_t handle, int sockfd);

  /// @brief Callback invoked when a client closes a connection.
  /// @param handle The httpd server handle.
  /// @param sockfd The socket file descriptor being closed.
  static void close_callback(httpd_handle_t handle, int sockfd);

  /// @brief WebSocket message handler registered with httpd.
  static esp_err_t websocket_handler(httpd_req_t *req);

  /// @brief The HTTP server handle.
  httpd_handle_t server_{nullptr};

  /// @brief Maximum number of simultaneous connections (default: 2 for handoff).
  uint8_t max_connections_{2};

  /// @brief Callback to notify the hub of new connections.
  NewConnectionCallback new_connection_callback_;

  /// @brief Callback to notify the hub when a socket closes.
  ConnectionClosedCallback connection_closed_callback_;

  /// @brief Callback to find a connection by socket fd.
  FindConnectionCallback find_connection_callback_;

  /// @brief Pointer to the hub (stored as user context for callbacks).
  SendspinHub *hub_{nullptr};
};

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP32
