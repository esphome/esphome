#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "sendspin_protocol.h"
#include "sendspin_time_filter.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <functional>
#include <memory>
#include <string>

namespace esphome {
namespace sendspin {

/// @brief Callback type for message send completion.
/// @param success True if the message was sent successfully, false otherwise.
/// @param timestamp The actual timestamp when the message was sent (in microseconds).
using SendCompleteCallback = std::function<void(bool, int64_t)>;

/// @brief Abstract base class for Sendspin connections (server-initiated or client-initiated).
///
/// This class represents a single connection to a Sendspin server. It manages connection state,
/// time synchronization, message buffering, and the hello handshake. Derived classes implement
/// the actual transport mechanism (e.g., incoming WebSocket server connection or outgoing client).
///
/// The hub owns connection instances and uses callbacks to receive notifications about messages,
/// handshake completion, and disconnection events.
class SendspinConnection {
 public:
  virtual ~SendspinConnection();

  /// @brief Starts the connection (e.g., initiates client connection or begins message processing).
  virtual void start() = 0;

  /// @brief Periodic loop processing (e.g., poll for events, handle state machine).
  virtual void loop() = 0;

  /// @brief Disconnects from the server with a goodbye message.
  /// @param reason The reason for disconnecting (e.g., shutdown, another server).
  /// @param on_complete Optional callback invoked after goodbye is sent (or send fails/times out).
  ///                    For server connections, invoked from httpd worker thread (use defer() if needed).
  ///                    For client connections, invoked synchronously in the calling thread.
  virtual void disconnect(SendspinGoodbyeReason reason, std::function<void()> on_complete) = 0;

  /// @brief Checks if the transport connection is established.
  /// @return true if connected, false otherwise.
  virtual bool is_connected() const = 0;

  /// @brief Checks if the hello handshake has completed successfully.
  /// @return true if handshake complete (hello exchange done), false otherwise.
  bool is_handshake_complete() const { return this->client_hello_sent_ && this->server_hello_received_; }

  /// @brief Gets the socket file descriptor for this connection.
  /// @return Socket fd for server connections, -1 for client connections.
  /// @note Used by the hub to identify which connection closed when notified by the server.
  virtual int get_sockfd() const { return -1; }

  /// @brief Sends a text message to the server with a completion callback.
  /// @param msg The message string to send.
  /// @param cb Callback invoked after send completes (success, actual_send_time).
  /// @return ESP_OK if queued successfully, error code otherwise.
  virtual esp_err_t send_text_message(const std::string &message, SendCompleteCallback cb) = 0;

  /// @brief Sends a time synchronization message with a completion callback.
  /// @param cb Callback invoked after send completes, providing actual send timestamp.
  /// @return true if message was queued successfully, false otherwise.
  bool send_time_message(SendCompleteCallback cb);

  /// @brief Sends a goodbye message with completion callback.
  /// @return ESP_OK if sent successfully, error code otherwise.
  esp_err_t send_goodbye_reason(SendspinGoodbyeReason reason, SendCompleteCallback on_complete);

  // Server information accessors (populated after server/hello message is received)

  /// @brief Gets the server ID from the server/hello message.
  /// @return The server ID string (empty until hello is received).
  const std::string &get_server_id() const { return this->server_id_; }

  /// @brief Gets the connection reason from the server/hello message.
  /// @return The connection reason (discovery or playback).
  SendspinConnectionReason get_connection_reason() const { return this->connection_reason_; }

  // Callbacks set by the hub to receive notifications

  /// @brief Callback invoked when a JSON message is received.
  /// @param conn Pointer to this connection.
  /// @param message The JSON message string.
  /// @param timestamp The client timestamp when the message was received.
  std::function<void(SendspinConnection *, const std::string &, int64_t)> on_json_message;

  /// @brief Callback invoked when a binary message is received.
  /// @param conn Pointer to this connection.
  /// @param payload Pointer to the binary message data (owned by connection, valid until callback returns).
  /// @param len Length of the binary message data.
  std::function<void(SendspinConnection *, uint8_t *, size_t)> on_binary_message;

  /// @brief Callback invoked when the transport connection is ready for messaging.
  /// @param conn Pointer to this connection.
  /// @note For server connections, this is called when the WebSocket handshake completes.
  ///       For client connections, this is called when the connection to server succeeds.
  ///       The hub uses this to initiate the hello handshake.
  std::function<void(SendspinConnection *)> on_connected;

  /// @brief Callback invoked when the hello handshake completes successfully.
  /// @param conn Pointer to this connection.
  std::function<void(SendspinConnection *)> on_handshake_complete;

  /// @brief Callback invoked when the connection is closed or lost.
  /// @param conn Pointer to this connection.
  std::function<void(SendspinConnection *)> on_disconnected;

  /// @brief Converts a server timestamp to the equivalent client timestamp.
  /// @param server_time Server timestamp in microseconds.
  /// @return Equivalent client timestamp in microseconds (0 if time filter not initialized).
  int64_t get_client_time(int64_t server_time) const {
    if (this->time_filter_ == nullptr) {
      return 0;
    }
    return this->time_filter_->compute_client_time(server_time);
  }

  /// @brief Returns true if the time filter has received at least one measurement.
  /// @return True if time synchronization has started, false otherwise.
  bool is_time_synced() const {
    if (this->time_filter_ == nullptr) {
      return false;
    }
    return this->time_filter_->has_update();
  }

  /// @brief Gets the time filter for this connection.
  /// @return Pointer to the time filter, or nullptr if not initialized.
  SendspinTimeFilter *get_time_filter() { return this->time_filter_.get(); }

  /// @brief Initializes the time filter with Kalman parameters.
  void init_time_filter();

  // Configuration setters (called by hub after receiving server/hello message)

  /// @brief Sets the server ID (from server/hello message).
  /// @param server_id The server ID string.
  /// @note Called by hub after receiving server/hello message.
  void set_server_id(const std::string &server_id) { this->server_id_ = server_id; }

  /// @brief Sets the server name (from server/hello message).
  /// @param server_name The server name string.
  /// @note Called by hub after receiving server/hello message.
  void set_server_name(const std::string &server_name) { this->server_name_ = server_name; }

  /// @brief Sets the connection reason (from server/hello message).
  /// @param reason The connection reason.
  /// @note Called by hub after receiving server/hello message.
  void set_connection_reason(SendspinConnectionReason reason) { this->connection_reason_ = reason; }

  /// @brief Sets the client hello sent flag.
  /// @param sent True if client hello message has been sent.
  /// @note Called by hub to track handshake state.
  void set_client_hello_sent(bool sent) { this->client_hello_sent_ = sent; }

  /// @brief Sets the server hello received flag.
  /// @param received True if server hello message has been received.
  /// @note Called by hub when SERVER_HELLO is processed.
  void set_server_hello_received(bool received) { this->server_hello_received_ = received; }

  // Time message state accessors

  /// @brief Checks if a time message is pending (waiting for response).
  bool is_pending_time_message() const { return this->pending_time_message_; }

  /// @brief Sets the pending time message flag.
  void set_pending_time_message(bool pending) { this->pending_time_message_ = pending; }

  /// @brief Gets the timestamp of the last sent time message.
  int64_t get_last_sent_time_message() const { return this->last_sent_time_message_; }

  /// @brief Sets the timestamp of the last sent time message.
  void set_last_sent_time_message(int64_t timestamp) { this->last_sent_time_message_ = timestamp; }

  /// @brief Thread-safe peek at the last time replacement data.
  /// Uses a FreeRTOS queue (depth 1) so the send callback can write from any thread
  /// while the receive handler reads from any thread without data races.
  /// @return Copy of the last time replacement, or default-constructed if none available.
  TimeTransmittedReplacement peek_time_replacement() const;

 protected:
  /// @brief Deallocates the websocket payload buffer if allocated.
  void deallocate_websocket_payload_();

  /// @brief Resets the write offset without freeing the buffer (reuses it for the next message).
  void reset_websocket_payload_();

  // Per-connection state (moved from hub)

  /// Time synchronization filter (Kalman-based).
  std::unique_ptr<SendspinTimeFilter> time_filter_;

  /// Server identity (from server/hello message).
  std::string server_id_;
  std::string server_name_;

  /// Connection reason (discovery or playback, from server/hello).
  SendspinConnectionReason connection_reason_{SendspinConnectionReason::DISCOVERY};

  /// Hello handshake state.
  bool client_hello_sent_{false};
  bool server_hello_received_{false};

  /// Time message state.
  bool pending_time_message_{false};
  int64_t last_sent_time_message_{0};

  /// Thread-safe single-slot buffer for time replacement data.
  /// Written by the send callback (which may run on httpd/websocket thread),
  /// read by the receive handler (which may run on a different thread).
  QueueHandle_t time_replacement_queue_{nullptr};

  /// @brief Allocates or grows the websocket payload buffer and returns a pointer to the write position.
  ///
  /// For the first fragment, allocates a new buffer of the given size.
  /// For continuation fragments, reallocates to grow the buffer if needed.
  ///
  /// @param data_len Number of bytes that will be written.
  /// @return Pointer to the write position (websocket_payload_ + websocket_write_offset_), or nullptr on failure.
  uint8_t *prepare_receive_buffer_(size_t data_len);

  /// @brief Advances the write offset after data has been written into the buffer.
  /// @param data_len Number of bytes that were written.
  void commit_receive_buffer_(size_t data_len);

  /// @brief Dispatches a fully assembled message to the appropriate callback.
  ///
  /// For text messages: creates a std::string from the buffer, invokes on_json_message, deallocates buffer.
  /// For binary messages: invokes on_binary_message callback.
  /// If the buffer is null, does nothing.
  ///
  /// @param is_text True if this is a text message, false for binary.
  /// @param receive_time Timestamp when the data was received (microseconds).
  void dispatch_completed_message_(bool is_text, int64_t receive_time);

  /// Message buffering (for websocket frame assembly).
  uint8_t *websocket_payload_{nullptr};
  size_t websocket_write_offset_{0};
  size_t websocket_len_{0};

  /// @brief Tracks whether the current message being assembled is text (true) or binary (false).
  /// @note Needed because WebSocket continuation frames don't carry the original frame type.
  bool is_text_frame_{false};
};

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP32
