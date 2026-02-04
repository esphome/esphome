#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF

#include "sendspin_protocol.h"

#include "esp_check.h"
#include <esp_http_server.h>

#include <functional>
#include <optional>

namespace esphome {
namespace sendspin {

/// Callback invoked after a message send attempt completes.
/// @param success true if message was actually sent, false if client disconnected or send failed
/// @param actual_send_time timestamp when send completed (only meaningful if success=true)
using SendCompleteCallback = std::function<void(bool success, int64_t actual_send_time)>;

class SendspinWebsocket {
 public:
  /// @brief Sends an inital hello message to the server describing the client.
  void send_hello_message(const ClientHelloMessage *msg);
  /// @brief Sends an initial hello message with completion callback.
  /// @return ESP_OK if queued, ESP_ERR_INVALID_STATE if no client, ESP_ERR_NO_MEM/ESP_FAIL on error
  esp_err_t send_hello_message(const ClientHelloMessage *msg, SendCompleteCallback on_complete);

  void send_client_state_message(const ClientStateMessage *msg);

#ifdef USE_SENDSPIN_CONTROLLER
  void send_client_command_message(SendspinCommandType command, std::optional<uint8_t> volume = std::nullopt,
                                   std::optional<bool> mute = std::nullopt);
#endif

  void send_goodbye_reason(SendspinGoodbyeReason reason);
  /// @brief Sends a goodbye message with completion callback.
  /// @return ESP_OK if queued, ESP_ERR_INVALID_STATE if no client, ESP_ERR_NO_MEM/ESP_FAIL on error
  esp_err_t send_goodbye_reason(SendspinGoodbyeReason reason, SendCompleteCallback on_complete);
  void disconnect();

  /// @brief Sends a time synchronization message.
  /// @param on_complete Callback invoked after the message is sent, with actual send time.
  /// @return true if message was queued successfully.
  bool send_time_message(SendCompleteCallback on_complete);

  void start_server(std::function<esp_err_t((httpd_req_t *) )> &&callback,
                    std::function<void((void *) )> &&close_callback, void *context, bool task_stack_in_psram,
                    unsigned task_priority);

  bool is_connected() { return this->current_client_.has_value(); }
  bool is_started() { return this->is_started_; }

 protected:
  static esp_err_t open_callback(httpd_handle_t handle, int sockfd);
  static void close_callback(httpd_handle_t handle, int sockfd);
  static void async_send_text(void *arg);

  esp_err_t send_text_message_(const std::string &message, SendCompleteCallback on_complete);
  esp_err_t send_text_message_(const std::string &message) { return this->send_text_message_(message, nullptr); };

  std::function<void()> hub_close_callback_;

  httpd_handle_t server_;

  std::optional<int> current_client_;

  bool is_started_{false};
};
}  // namespace sendspin
}  // namespace esphome
#endif
