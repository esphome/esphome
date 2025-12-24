#include "sendspin_websocket.h"

#if defined(USE_ESP_IDF)

#include "esphome/components/json/json_util.h"
#include "esphome/core/log.h"

#include "lwip/sockets.h"  // for setsockopt, IPPROTO_TCP, NODELAY

#include <esp_timer.h>

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.websocket";

/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct AsyncRespArg {
  void *context;
  uint8_t *payload;
  size_t len;
  TimeTransmittedReplacement *time_transmitted = nullptr;
};

void SendspinWebsocket::start_server(std::function<esp_err_t((httpd_req_t *) )> &&callback,
                                     std::function<void((void *) )> &&close_callback, void *context,
                                     bool task_stack_in_psram, unsigned task_priority) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  if (task_stack_in_psram) {
    config.task_caps = MALLOC_CAP_SPIRAM;
  }
  config.task_priority = task_priority;
  config.server_port = 8927;
  config.max_open_sockets = 1;
  config.open_fn = SendspinWebsocket::open_callback;
  config.close_fn = SendspinWebsocket::close_callback;
  config.global_user_ctx = (void *) this;
  config.global_user_ctx_free_fn = nullptr;
  config.ctrl_port = ESP_HTTPD_DEF_CTRL_PORT + 1;  // Avoid a conflict with web_server component

  const httpd_uri_t sendspin_ws_uri = {.uri = "/sendspin",
                                       .method = HTTP_GET,
                                       .handler = *std::move(callback).target<esp_err_t (*)(httpd_req_t *)>(),
                                       .user_ctx = context,
                                       .is_websocket = true};

  // Start the httpd server
  ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&this->server_, &config) == ESP_OK) {
    // Registering the ws handler
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(this->server_, &sendspin_ws_uri);
    this->is_started_ = true;

    // Prepare the hub's close callback function to be called with the provided context
    std::function<void()> hub_close_callback_with_context = [close_callback, context]() { close_callback(context); };
    this->hub_close_callback_ = std::move(hub_close_callback_with_context);

    return;
  }
  ESP_LOGE(TAG, "Error starting server!");
}

void SendspinWebsocket::send_hello_message(const ClientHelloMessage *msg) {
  this->send_text_message_(format_client_hello_message(msg));
}

void SendspinWebsocket::send_client_state_message(const ClientStateMessage *msg) {
  this->send_text_message_(format_client_state_message(msg));
}

#ifdef USE_SENDSPIN_CONTROLLER
void SendspinWebsocket::send_client_command_message(SendspinCommandType command, std::optional<uint8_t> volume,
                                                    std::optional<bool> mute) {
  this->send_text_message_(format_client_command_message(command, volume, mute));
}
#endif

void SendspinWebsocket::send_goodbye_reason(SendspinGoodbyeReason reason) {
  this->send_text_message_(format_client_goodbye_message(reason));
}

void SendspinWebsocket::disconnect() {
  if (this->current_client_.has_value()) {
    httpd_sess_trigger_close(this->server_, this->current_client_.value());
  }
}

void SendspinWebsocket::send_time_message() {
  int64_t now = esp_timer_get_time();
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  std::string serialized_text = json::build_json([now](JsonObject root) {
    root["type"] = "client/time";
    root["payload"]["client_transmitted"] = now;
  });
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
  this->last_time_message_.transmitted_time = now;
  this->last_time_message_.actual_transmit_time = now;  // will be overwritten once known
  this->send_text_message_(serialized_text, &this->last_time_message_);
}

esp_err_t SendspinWebsocket::send_text_message_(const std::string &message,
                                                TimeTransmittedReplacement *time_transmitted) {
  if (this->current_client_.has_value()) {
    auto async_resp_allocator = RAMAllocator<AsyncRespArg>(RAMAllocator<AsyncRespArg>::ALLOC_INTERNAL);
    struct AsyncRespArg *resp_arg = async_resp_allocator.allocate(1);
    if (resp_arg == nullptr) {
      return ESP_ERR_NO_MEM;
    }

    resp_arg->context = (void *) this;
    auto message_allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::ALLOC_INTERNAL);
    resp_arg->payload = message_allocator.allocate(message.size());
    resp_arg->len = message.size();
    resp_arg->time_transmitted = time_transmitted;
    std::memcpy((void *) resp_arg->payload, (void *) message.data(), message.size());

    if (httpd_queue_work(this->server_, async_send_text, resp_arg) != ESP_OK) {
      ESP_LOGE(TAG, "httpd_queue_work failed!");
      message_allocator.deallocate(resp_arg->payload, resp_arg->len);
      return ESP_FAIL;
    }
  }
  return ESP_OK;
}

esp_err_t SendspinWebsocket::open_callback(httpd_handle_t handle, int sockfd) {
  ESP_LOGI(TAG, "client opened a connection");

  SendspinWebsocket *this_client = (SendspinWebsocket *) httpd_get_global_user_ctx(handle);
  this_client->current_client_ = sockfd;

  // Disabling Nagle's algorithm significantly improves the time syncing accuracy
  int nodelay = 1;
  if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) < 0) {
    ESP_LOGW(TAG, "Failed to turn on TCP_NODELAY, syncing may be inaccurate");
    nodelay = 0;
  }

  // // TODO: Documentation ssays this disables AMPDU. Verify that it doesn't cause issues with throughput
  // int priority = IPTOS_LOWDELAY;
  // setsockopt(sockfd, IPPROTO_IP, IP_TOS, &priority, sizeof(priority));

  return ESP_OK;
}

void SendspinWebsocket::close_callback(httpd_handle_t handle, int sockfd) {
  ESP_LOGI(TAG, "Websocket client closed a connection");
  SendspinWebsocket *this_client = (SendspinWebsocket *) httpd_get_global_user_ctx(handle);
  this_client->current_client_.reset();
  close(sockfd);

  // Call the hub's connection closed callback
  this_client->hub_close_callback_();
}

void SendspinWebsocket::async_send_text(void *arg) {
  struct AsyncRespArg *resp_arg = (AsyncRespArg *) arg;
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

  SendspinWebsocket *this_client = (SendspinWebsocket *) resp_arg->context;

  ws_pkt.payload = resp_arg->payload;
  ws_pkt.len = resp_arg->len;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;

  if (this_client->current_client_.has_value()) {
    // TODO: Handle errors
    httpd_ws_send_frame_async(this_client->server_, this_client->current_client_.value(), &ws_pkt);
  }

  const int64_t after_send_time = esp_timer_get_time();

  if (resp_arg->time_transmitted != nullptr) {
    resp_arg->time_transmitted->actual_transmit_time = after_send_time;
  }

  auto message_allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::ALLOC_INTERNAL);
  message_allocator.deallocate(ws_pkt.payload, ws_pkt.len);
  auto async_resp_allocator = RAMAllocator<AsyncRespArg>(RAMAllocator<AsyncRespArg>::ALLOC_INTERNAL);
  async_resp_allocator.deallocate(resp_arg, sizeof(AsyncRespArg));
}

}  // namespace sendspin
}  // namespace esphome

#endif
