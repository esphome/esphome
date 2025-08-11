#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF

#include "resonate_protocol.h"

#ifdef USE_MEDIA_PLAYER
#include "esphome/components/media_player/media_player.h"
#endif

#include "esp_check.h"
#include <esp_http_server.h>

#include <functional>
#include <optional>

namespace esphome {
namespace resonate {

class ResonateWebsocket {
 public:
  /// @brief Sends an inital hello message to the server describing the client.
  void send_hello_message(const PlayerHelloMessage *msg);

  void send_player_state_message(const PlayerStateMessage *msg);

#ifdef USE_MEDIA_PLAYER
  void send_stream_command_message(const media_player::MediaPlayerCall &call);
#endif

  void send_time_message();

  void start_server(std::function<esp_err_t((httpd_req_t *) )> &&callback, void *context, bool task_stack_in_psram,
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

  httpd_handle_t server_;
  TimeTransmittedReplacement last_time_message_;

  std::optional<int> current_client_;

  bool is_started_{false};
};
}  // namespace resonate
}  // namespace esphome
#endif
