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

class SendspinWebsocket {
 public:
  /// @brief Sends an inital hello message to the server describing the client.
  void send_hello_message(const ClientHelloMessage *msg);

  void send_client_state_message(const ClientStateMessage *msg);

#ifdef USE_SENDSPIN_CONTROLLER
  void send_client_command_message(SendspinCommandType command, std::optional<uint8_t> volume = std::nullopt,
                                   std::optional<bool> mute = std::nullopt);
#endif

  void send_time_message();

  void start_server(std::function<esp_err_t((httpd_req_t *) )> &&callback,
                    std::function<void((void *) )> &&close_callback, void *context, bool task_stack_in_psram,
                    unsigned task_priority);

  TimeTransmittedReplacement get_last_time_message() { return this->last_time_message_; }

  bool is_connected() { return this->current_client_.has_value(); }
  bool is_started() { return this->is_started_; }

 protected:
  static esp_err_t open_callback(httpd_handle_t handle, int sockfd);
  static void close_callback(httpd_handle_t handle, int sockfd);
  static void async_send_text(void *arg);

  esp_err_t send_text_message_(const std::string &message, TimeTransmittedReplacement *time_transmitted);
  esp_err_t send_text_message_(const std::string &message) { return this->send_text_message_(message, nullptr); };

  std::function<void()> hub_close_callback_;

  httpd_handle_t server_;
  TimeTransmittedReplacement last_time_message_;

  std::optional<int> current_client_;

  bool is_started_{false};
};
}  // namespace sendspin
}  // namespace esphome
#endif
