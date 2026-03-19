#include "sendspin_server_connection.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include "lwip/sockets.h"  // for setsockopt, IPPROTO_TCP, NODELAY

#include <esp_timer.h>

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.server_connection";

/*
 * Structure holding connection context
 * and payload data for async send operations.
 */
struct AsyncRespArg {
  void *context;
  uint8_t *payload;
  size_t len;
  bool has_callback{false};
  SendCompleteCallback on_complete;
};

SendspinServerConnection::SendspinServerConnection(httpd_handle_t server, int sockfd)
    : server_(server), sockfd_(sockfd) {
  // Disabling Nagle's algorithm significantly improves the time syncing accuracy
  int nodelay = 1;
  if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) < 0) {
    ESP_LOGW(TAG, "Failed to turn on TCP_NODELAY, syncing may be inaccurate");
  }
}

void SendspinServerConnection::start() {
  // Initialize any per-connection state
  // Time filter is initialized by the hub when it sets up the connection
}

void SendspinServerConnection::loop() {
  // Time message sending is handled by the hub
}

void SendspinServerConnection::disconnect(SendspinGoodbyeReason reason, std::function<void()> on_complete) {
  if (!this->is_connected()) {
    // Not connected - invoke completion callback immediately if provided
    if (on_complete) {
      on_complete();
    }
    return;
  }

  // Send goodbye message, then trigger close, then invoke user callback
  // Capture on_complete by value to keep it alive until async callback fires
  this->send_goodbye_reason(reason, [this, on_complete](bool success, int64_t) {
    // Trigger close regardless of send success
    this->trigger_close();

    // Invoke user-provided completion callback if provided
    // Note: This is already running in httpd worker thread context (async_send_text),
    // so caller should use defer() if they need main loop context
    if (on_complete) {
      on_complete();
    }
  });
}

bool SendspinServerConnection::is_connected() const { return this->sockfd_ >= 0; }

esp_err_t SendspinServerConnection::send_text_message(const std::string &message, SendCompleteCallback on_complete) {
  if (!this->is_connected()) {
    // No client connected - invoke callback with failure if provided
    if (on_complete) {
      on_complete(false, 0);
    }
    return ESP_ERR_INVALID_STATE;
  }

  auto async_resp_allocator = RAMAllocator<AsyncRespArg>(RAMAllocator<AsyncRespArg>::ALLOC_INTERNAL);
  struct AsyncRespArg *resp_arg = async_resp_allocator.allocate(1);
  if (resp_arg == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate AsyncRespArg for message send");
    if (on_complete) {
      on_complete(false, 0);
    }
    return ESP_ERR_NO_MEM;
  }

  // Use placement new to properly construct the struct with the callback
  new (resp_arg) AsyncRespArg();

  resp_arg->context = (void *) this;
  auto message_allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::ALLOC_INTERNAL);
  resp_arg->payload = message_allocator.allocate(message.size());
  if (resp_arg->payload == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate %zu bytes for message payload", message.size());
    resp_arg->~AsyncRespArg();
    async_resp_allocator.deallocate(resp_arg, 1);
    if (on_complete) {
      on_complete(false, 0);
    }
    return ESP_ERR_NO_MEM;
  }
  resp_arg->len = message.size();

  // Move the callback into the struct if provided
  if (on_complete) {
    resp_arg->has_callback = true;
    resp_arg->on_complete = std::move(on_complete);
  }

  std::memcpy((void *) resp_arg->payload, (void *) message.data(), message.size());

  if (httpd_queue_work(this->server_, async_send_text, resp_arg) != ESP_OK) {
    ESP_LOGE(TAG, "httpd_queue_work failed!");
    message_allocator.deallocate(resp_arg->payload, resp_arg->len);
    // Need to invoke callback with failure before destroying it
    if (resp_arg->has_callback) {
      resp_arg->on_complete(false, 0);
    }
    resp_arg->~AsyncRespArg();
    async_resp_allocator.deallocate(resp_arg, 1);
    return ESP_FAIL;
  }
  return ESP_OK;
}

void SendspinServerConnection::trigger_close() {
  if (this->sockfd_ >= 0) {
    httpd_sess_trigger_close(this->server_, this->sockfd_);
  }
}

esp_err_t SendspinServerConnection::handle_data(httpd_req_t *req, int64_t receive_time) {
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

  // First call with max_len = 0 to get the frame length
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
    return ret;
  }

  // Track frame type: text/binary frames set the type, continuation frames inherit it
  if (ws_pkt.type == HTTPD_WS_TYPE_TEXT || ws_pkt.type == HTTPD_WS_TYPE_BINARY) {
    this->is_text_frame_ = (ws_pkt.type == HTTPD_WS_TYPE_TEXT);
  } else if (ws_pkt.type != HTTPD_WS_TYPE_CONTINUE) {
    // Control frames (ping, pong, close) - not handled here
    return ESP_OK;
  }

  bool is_final = ws_pkt.final;

  if (ws_pkt.len == 0) {
    // No payload data, but still dispatch if final (for empty messages or buffered data)
    if (is_final) {
      this->dispatch_completed_message_(this->is_text_frame_, receive_time);
    }
    return ESP_OK;
  }

  // Allocate/grow directly into the websocket payload buffer (zero-copy)
  uint8_t *dest = this->prepare_receive_buffer_(ws_pkt.len);
  if (dest == nullptr) {
    return ESP_ERR_NO_MEM;
  }

  // Point httpd directly at our payload buffer so it writes there without an intermediate copy
  ws_pkt.payload = dest;

  // Second call with max_len = ws_pkt.len to receive frame payload directly into our buffer
  ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
    this->reset_websocket_payload_();
    return ret;
  }

  this->commit_receive_buffer_(ws_pkt.len);

  if (is_final) {
    this->dispatch_completed_message_(this->is_text_frame_, receive_time);
  }

  return ESP_OK;
}

void SendspinServerConnection::async_send_text(void *arg) {
  struct AsyncRespArg *resp_arg = (AsyncRespArg *) arg;
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

  SendspinServerConnection *this_conn = (SendspinServerConnection *) resp_arg->context;

  ws_pkt.payload = resp_arg->payload;
  ws_pkt.len = resp_arg->len;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;

  bool send_success = false;
  if (this_conn->is_connected()) {
    esp_err_t err = httpd_ws_send_frame_async(this_conn->server_, this_conn->sockfd_, &ws_pkt);
    send_success = (err == ESP_OK);
  }

  const int64_t after_send_time = esp_timer_get_time();

  // Call the completion callback if provided
  if (resp_arg->has_callback) {
    resp_arg->on_complete(send_success, after_send_time);
  }

  auto message_allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::ALLOC_INTERNAL);
  message_allocator.deallocate(ws_pkt.payload, ws_pkt.len);

  // Properly destruct the AsyncRespArg (which includes the std::function)
  resp_arg->~AsyncRespArg();
  auto async_resp_allocator = RAMAllocator<AsyncRespArg>(RAMAllocator<AsyncRespArg>::ALLOC_INTERNAL);
  async_resp_allocator.deallocate(resp_arg, 1);
}

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP32
