#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "sendspin_connection.h"

#include <esp_websocket_client.h>

#include <functional>
#include <string>

namespace esphome {
namespace sendspin {

/// @brief A client-side WebSocket connection for Sendspin.
///
/// This class represents an outgoing WebSocket connection to a Sendspin server.
/// It inherits from SendspinConnection and implements the connection interface for
/// client-initiated connections (where the ESP device acts as a WebSocket client and
/// connects to a Sendspin server).
///
/// The class manages:
/// - The ESP-IDF websocket client handle
/// - Connection and reconnection logic
/// - Sending text messages (hello, state, time, goodbye, commands)
/// - Receiving and buffering websocket messages
///
/// Lifecycle:
/// 1. Created with server URL and Kalman filter parameters
/// 2. start() is called to initiate the connection
/// 3. loop() is called periodically to handle reconnection attempts
/// 4. disconnect() is called to gracefully close with goodbye message
class SendspinClientConnection : public SendspinConnection {
 public:
  /// @brief Constructs a client connection with the given server URL.
  /// @param url The WebSocket server URL (e.g., "ws://server.local:8927/sendspin").
  explicit SendspinClientConnection(std::string url);

  ~SendspinClientConnection() override;

  // SendspinConnection interface implementation

  /// @brief Starts the connection (initializes websocket client and connects).
  void start() override;

  /// @brief Periodic loop processing (handles reconnection attempts).
  void loop() override;

  /// @brief Disconnects from the server with a goodbye message.
  /// @param reason The reason for disconnecting.
  /// @param on_complete Optional callback invoked after goodbye send completes (or fails).
  ///                    For client connections, goodbye is synchronous, so callback is invoked immediately.
  void disconnect(SendspinGoodbyeReason reason, std::function<void()> on_complete) override;

  /// @brief Checks if the websocket connection is established.
  /// @return true if connected, false otherwise.
  bool is_connected() const override;

  /// @brief Sends a text message to the server with a completion callback.
  /// @param msg The message string to send.
  /// @param cb Callback invoked after send completes.
  /// @return ESP_OK if sent successfully, error code otherwise.
  esp_err_t send_text_message(const std::string &message, SendCompleteCallback cb) override;

  // Client connection-specific configuration

  /// @brief Sets whether to automatically reconnect on connection loss.
  /// @param enabled True to enable auto-reconnect, false to disable.
  void set_auto_reconnect(bool enabled) { this->auto_reconnect_ = enabled; }

 protected:
  /// @brief Static event handler for ESP-IDF websocket client events.
  /// @param handler_args User context (pointer to this SendspinClientConnection instance).
  /// @param base Event base.
  /// @param event_id Event ID.
  /// @param event_data Event data.
  static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

  /// @brief Handles websocket connected event.
  void handle_connected_();

  /// @brief Handles websocket disconnected event.
  void handle_disconnected_();

  /// @brief Handles websocket data event.
  /// @param data Pointer to websocket event data.
  /// @param receive_time Timestamp when the event was received (for time synchronization).
  void handle_data_(const esp_websocket_event_data_t *data, int64_t receive_time);

  /// @brief Handles websocket error event.
  void handle_error_();

  /// @brief The WebSocket server URL.
  std::string url_;

  /// @brief The ESP-IDF websocket client handle.
  esp_websocket_client_handle_t client_{nullptr};

  /// @brief Whether the websocket is currently connected.
  bool connected_{false};

  /// @brief Auto-reconnect configuration.
  bool auto_reconnect_{true};
  uint32_t reconnect_interval_ms_{5000};
  uint32_t last_reconnect_attempt_{0};
};

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP32
