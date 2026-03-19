#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "sendspin_connection.h"

#include <esp_http_server.h>

#include <functional>

namespace esphome {
namespace sendspin {

/// @brief A server-side WebSocket connection for Sendspin.
///
/// This class represents a single incoming WebSocket connection from a Sendspin server.
/// It inherits from SendspinConnection and implements the connection interface for
/// server-initiated connections (where the ESP device acts as a WebSocket server and
/// the Sendspin server connects to it).
///
/// The class manages:
/// - The socket file descriptor for the accepted connection
/// - Sending text messages (hello, state, time, goodbye, commands)
/// - The httpd handle reference (owned by SendspinWsServer)
///
/// Lifecycle:
/// 1. Created by SendspinWsServer when a new connection is accepted
/// 2. start() is called to begin message processing
/// 3. loop() is called periodically to handle time synchronization
/// 4. disconnect() is called to gracefully close with goodbye message
class SendspinServerConnection : public SendspinConnection {
 public:
  /// @brief Constructs a server connection with the given httpd handle and socket.
  /// @param server The httpd handle (owned by the server listener).
  /// @param sockfd The socket file descriptor for this connection.
  SendspinServerConnection(httpd_handle_t server, int sockfd);

  ~SendspinServerConnection() override = default;

  // SendspinConnection interface implementation

  /// @brief Starts the connection (initializes time filter, prepares for messages).
  void start() override;

  /// @brief Periodic loop processing (handles time message sending).
  void loop() override;

  /// @brief Gracefully disconnects by sending a goodbye message, then closing.
  ///
  /// This is the high-level API for disconnection. It:
  /// 1. Sends a goodbye message with the specified reason
  /// 2. Calls trigger_close() after the message is sent (via async completion callback)
  /// 3. Invokes on_complete callback (if provided) after goodbye send completes
  ///
  /// @param reason The reason for disconnecting (sent in goodbye message).
  /// @param on_complete Optional callback invoked after goodbye send completes (or fails).
  ///                    Invoked from httpd worker thread - use defer() if main loop context is needed.
  void disconnect(SendspinGoodbyeReason reason, std::function<void()> on_complete) override;

  /// @brief Checks if the socket connection is valid.
  /// @return true if connected, false otherwise.
  bool is_connected() const override;

  /// @brief Sends a text message to the server with a completion callback.
  /// @param msg The message string to send.
  /// @param cb Callback invoked after send completes.
  /// @return ESP_OK if queued successfully, error code otherwise.
  esp_err_t send_text_message(const std::string &message, SendCompleteCallback on_complete) override;

  /// @brief Triggers the underlying socket to close.
  ///
  /// This is a low-level method that directly triggers the httpd session to close.
  /// It does NOT send a goodbye message first.
  ///
  /// Relationship with disconnect():
  /// - disconnect() is the high-level API that sends a goodbye message, then calls
  ///   trigger_close() in the completion callback after the message is sent.
  /// - trigger_close() is the low-level mechanism that actually closes the socket.
  ///
  /// Use disconnect() for graceful shutdown. Use trigger_close() only when you
  /// need to force-close without sending goodbye (e.g., after goodbye is already sent).
  void trigger_close();

  /// @brief Gets the socket file descriptor.
  /// @return The socket fd, or -1 if not connected.
  int get_sockfd() const override { return this->sockfd_; }

  /// @brief Handles incoming WebSocket data.
  /// @param req The httpd request containing the WebSocket frame.
  /// @param receive_time Timestamp when the data was received.
  /// @return ESP_OK on success, error code on failure.
  esp_err_t handle_data(httpd_req_t *req, int64_t receive_time);

 protected:
  static void async_send_text(void *arg);

  /// @brief The httpd server handle (owned by SendspinWsServer).
  httpd_handle_t server_;

  /// @brief The socket file descriptor for this connection.
  int sockfd_{-1};
};

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP32
